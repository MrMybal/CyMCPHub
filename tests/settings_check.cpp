// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "app_settings.hpp"
#include "platform.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>

namespace {
int failures = 0;

void expect(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << '\n';
    ++failures;
}

hub::AppSettings read(const hub::fs::path& directory, const char* json) {
    std::ofstream(directory / "settings.json", std::ios::binary | std::ios::trunc) << json;
    return hub::read_app_settings();
}
} // namespace

int main() {
    const auto directory = hub::fs::temp_directory_path() / ("cymcphub-settings-" + cymcp::random_token(16));
    hub::fs::create_directories(directory);
#ifdef _WIN32
    _wputenv_s(L"CYMCP_HUB_DATA", directory.c_str());
#else
    setenv("CYMCP_HUB_DATA", directory.c_str(), 1);
#endif
    try {
        const hub::AppSettings defaults;
        auto settings = read(directory, R"({"refresh_seconds":30,"scan_ports":false,"mcp":{"port":9000,"require_token":false,"start_on_launch":true}})");
        expect(settings.refresh_seconds == 30 && !settings.scan_ports, "valid fields are read");
        expect(settings.service.port == 9000 && !settings.service.require_token && settings.service.start_on_launch, "service fields are read");

        settings = read(directory, R"({"refresh_seconds":45,"scan_ports":false,"mcp":{"port":"8080","start_on_launch":true}})");
        expect(settings.refresh_seconds == defaults.refresh_seconds, "unknown interval falls back alone");
        expect(!settings.scan_ports && settings.service.start_on_launch, "other fields survive a bad one");
        expect(settings.service.port == defaults.service.port, "string port is ignored");

        settings = read(directory, R"({"mcp":{"port":4294975376}})");
        expect(settings.service.port == defaults.service.port, "out-of-range port is not narrowed");

        settings = read(directory, R"({"mcp":null,"scan_ports":1})");
        expect(settings.scan_ports == defaults.scan_ports && settings.service == defaults.service, "wrong types keep defaults");

        hub::save_app_settings(settings);
        expect(hub::read_app_settings() == settings, "saved settings read back");
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        ++failures;
    }
    std::error_code ignored;
    hub::fs::remove_all(directory, ignored);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
