// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "app_settings.hpp"
#include "platform.hpp"

#include <algorithm>
#include <fstream>
#include <optional>
#ifdef _WIN32
#include <windows.h>
#endif

namespace hub {
namespace {
bool legacy_service_enabled() {
#ifdef _WIN32
    DWORD value = 0, size = sizeof(value);
    return RegGetValueW(HKEY_CURRENT_USER, L"Software\\CyberAlien\\CyMCPHub", L"McpEnabled",
                        RRF_RT_REG_DWORD, nullptr, &value, &size) == ERROR_SUCCESS && value;
#else
    std::error_code error;
    return fs::is_regular_file(data_path() / "mcp-service" / "enabled", error);
#endif
}

void validate(const AppSettings& settings) {
    if (settings.service.port < 1 || settings.service.port > 65535)
        throw std::runtime_error("The MCP port must be between 1 and 65535.");
    if (std::find(AppSettings::refresh_intervals.begin(), AppSettings::refresh_intervals.end(), settings.refresh_seconds)
        == AppSettings::refresh_intervals.end())
        throw std::runtime_error("Invalid refresh interval.");
}
} // namespace

AppSettings read_app_settings() {
    AppSettings settings;
    const auto path = data_path() / "settings.json";
    if (!fs::exists(path)) {
        settings.service.start_on_launch = legacy_service_enabled();
        return settings;
    }

    Json json;
    {
        // Same lock as the writer: on Windows a concurrent reader makes the atomic replace fail.
        const FileLock lock(path.parent_path() / "settings.lock");
        std::error_code error;
        const auto size = fs::file_size(path, error);
        std::ifstream file(path, std::ios::binary);
        if (error || !file || size > 16 * 1024) throw std::runtime_error("Cannot read app settings.");
        json = Json::parse(file, nullptr, false);
    }
    if (!json.is_object()) throw std::runtime_error("Cannot read app settings.");
    // Each field falls back on its default alone: one bad value keeps the others.
    const auto flag = [](const Json& object, const char* key, bool& value) {
        if (const auto found = object.find(key); found != object.end() && found->is_boolean()) value = found->get<bool>();
    };
    const auto integer = [](const Json& object, const char* key) -> std::optional<std::int64_t> {
        const auto found = object.find(key);
        if (found == object.end() || !found->is_number_integer()) return std::nullopt;
        return found->get<std::int64_t>();
    };
    if (const auto seconds = integer(json, "refresh_seconds");
        seconds && std::find(AppSettings::refresh_intervals.begin(), AppSettings::refresh_intervals.end(), *seconds) !=
                       AppSettings::refresh_intervals.end())
        settings.refresh_seconds = static_cast<int>(*seconds);
    flag(json, "scan_ports", settings.scan_ports);
    if (const auto service = json.find("mcp"); service != json.end() && service->is_object()) {
        flag(*service, "start_on_launch", settings.service.start_on_launch);
        flag(*service, "require_token", settings.service.require_token);
        if (const auto port = integer(*service, "port"); port && *port >= 1 && *port <= 65535)
            settings.service.port = static_cast<int>(*port);
    }
    return settings;
}

void save_app_settings(const AppSettings& settings) {
    validate(settings);
    const auto directory = data_path();
    fs::create_directories(directory);
    const FileLock lock(directory / "settings.lock");
    const auto path = directory / "settings.json";
    const auto temporary = directory / ("settings-" + cymcp::random_token(16) + ".tmp");
    const Json json = {{"refresh_seconds", settings.refresh_seconds}, {"scan_ports", settings.scan_ports},
                       {"mcp", {{"start_on_launch", settings.service.start_on_launch},
                                {"require_token", settings.service.require_token}, {"port", settings.service.port}}}};
    try {
        std::ofstream file(temporary, std::ios::binary);
        file << json.dump(2) << '\n';
        file.close();
        if (!file || publish_file(temporary, path, true)) throw std::runtime_error("Cannot save app settings.");
    } catch (...) {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        throw;
    }
}

std::string mcp_service_url(int port) { return "http://127.0.0.1:" + std::to_string(port) + "/mcp"; }
} // namespace hub
