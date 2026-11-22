// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "catalog.hpp"

namespace hub {

struct UpdateInstaller {
    std::string name;
    std::string url;
    std::string sha256;
    std::size_t size;
};

struct AppUpdate {
    std::string version;
    std::string release_url;
    std::optional<UpdateInstaller> installer;
};

std::optional<AppUpdate> parse_update(const Json& release, std::string_view current, bool windows_x64);
std::optional<AppUpdate> check_update(const cymcp::Cancellation& cancellation = {});
void verify_installer(const fs::path& path, const UpdateInstaller& installer,
                      const cymcp::Cancellation& cancellation = {});
fs::path download_installer(const UpdateInstaller& installer, const cymcp::Cancellation& cancellation = {});

} // namespace hub
