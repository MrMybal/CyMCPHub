// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <array>
#include <string>

namespace hub {
struct McpServiceSettings {
    bool start_on_launch = false;
    bool require_token = true;
    int port = 48786;
    bool operator==(const McpServiceSettings&) const = default;
};

struct AppSettings {
    static constexpr std::array refresh_intervals{0, 15, 30, 60, 120, 300};
    int refresh_seconds = 60;
    bool scan_ports = true;
    McpServiceSettings service;
    bool operator==(const AppSettings&) const = default;
};

AppSettings read_app_settings();
void save_app_settings(const AppSettings& settings);
std::string mcp_service_url(int port);
} // namespace hub
