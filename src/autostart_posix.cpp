// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "autostart.hpp"
#include "platform.hpp"

#include <fstream>

namespace hub {
namespace {
fs::path entry_path() {
#ifdef __APPLE__
    if (const auto home = home_path(); !home.empty()) return home / "Library/LaunchAgents/com.cyberalien.cymcphub.plist";
#else
    if (const auto config = client_config_path(); !config.empty()) return config / "autostart/com.cyberalien.cymcphub.desktop";
#endif
    throw std::runtime_error("Login configuration directory is unavailable");
}

std::string escape(std::string_view value) {
    std::string result;
#ifdef __APPLE__
    for (const char c : value) {
        switch (c) {
        case '&': result += "&amp;"; break;
        case '<': result += "&lt;"; break;
        case '>': result += "&gt;"; break;
        default: result += c;
        }
    }
#else
    result = '"';
    for (const char c : value) {
        if (c == '\n' || c == '\r' || c == '\t') throw std::runtime_error("Unsupported login entry path");
        if (c == '\\' || c == '"' || c == '`' || c == '$') result += c == '\\' ? "\\\\\\" : "\\\\";
        if (c == '%') result += '%';
        result += c;
    }
    result += '"';
#endif
    return result;
}
} // namespace

bool autostart_enabled() {
    std::error_code error;
    return fs::is_regular_file(entry_path(), error);
}

void set_autostart(bool enabled, const Options& options) {
    const auto path = entry_path();
    if (!enabled) {
        std::error_code error;
        fs::remove(path, error);
        if (error) throw std::runtime_error("Cannot remove login entry");
        return;
    }
    const auto executable = executable_path();
    if (executable.empty()) throw std::runtime_error("Executable path is unavailable");
    std::vector<std::string> arguments{cymcp::path_utf8(executable), "--tray", "--workspace", cymcp::path_utf8(options.workspace)};
    if (!options.extra_config.empty()) {
        arguments.emplace_back("--config");
        arguments.push_back(cymcp::path_utf8(options.extra_config));
    }
    if (!options.scan_ports) arguments.emplace_back("--no-port-scan");
    fs::create_directories(path.parent_path());
    auto temporary = path;
    temporary += '.' + cymcp::random_token(16) + ".tmp";
    std::ofstream file(temporary, std::ios::binary);
#ifdef __APPLE__
    file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
            "<plist version=\"1.0\"><dict>\n<key>Label</key><string>com.cyberalien.cymcphub</string>\n"
            "<key>RunAtLoad</key><true/>\n<key>ProgramArguments</key><array>\n";
    for (const auto& argument : arguments) file << "<string>" << escape(argument) << "</string>\n";
    file << "</array></dict></plist>\n";
#else
    file << "[Desktop Entry]\nType=Application\nName=CyMCPHub\nTerminal=false\nExec=";
    for (std::size_t i = 0; i < arguments.size(); ++i) file << (i ? " " : "") << escape(arguments[i]);
    file << '\n';
#endif
    file.close();
    if (!file || publish_file(temporary, path, true)) {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        throw std::runtime_error("Cannot save login entry");
    }
}
} // namespace hub
