// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "updates.hpp"

#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--check-github") {
            const auto result = hub::check_update();
            std::cout << (result ? result->version : "GitHub check succeeded: no newer release") << '\n';
            return 0;
        }
        const auto require = [](bool result, const char* message) {
            if (!result) throw std::runtime_error(message);
        };
        const std::string name = "CyMCPHub-Setup-0.3.10-windows-x64.exe";
        const auto hash = cymcp::sha256("test");
        hub::Json asset = {{"name", name}, {"size", 4u}, {"state", "uploaded"}, {"digest", "sha256:" + hash},
            {"browser_download_url", "https://github.com/MrMybal/CyMCPHub/releases/download/v0.3.10/" + name}};
        hub::Json release = {{"tag_name", "v0.3.10"}, {"draft", false}, {"prerelease", false}, {"assets", {asset}}};
        const auto upgrade = hub::parse_update(release, "0.3.9", true);
        require(upgrade && upgrade->installer && upgrade->installer->sha256 == hash, "Missing valid upgrade");
        require(!hub::parse_update(release, "0.3.10", true), "Equal version offered");
        require(!hub::parse_update(release, "0.4.0", true), "Older version offered");
        require(!hub::parse_update(release, "0.3.9", false)->installer, "Windows installer offered to another platform");
        release["assets"][0]["browser_download_url"] = "https://example.com/" + name;
        require(!hub::parse_update(release, "0.3.9", true)->installer, "Foreign installer URL accepted");
        release["assets"][0] = asset;
        release["assets"][0]["digest"] = "sha256:broken";
        require(!hub::parse_update(release, "0.3.9", true)->installer, "Invalid digest accepted");
        release["prerelease"] = true;
        require(!hub::parse_update(release, "0.3.9", true), "Prerelease offered");

        const auto path = hub::fs::temp_directory_path() / ("cymcphub-test-" + cymcp::random_token(16));
        struct Remove { hub::fs::path file; ~Remove() { std::error_code ec; hub::fs::remove(file, ec); } } cleanup{path};
        std::ofstream(path, std::ios::binary) << "test";
        hub::verify_installer(path, *upgrade->installer);
        std::ofstream(path, std::ios::binary) << "fake";
        try { hub::verify_installer(path, *upgrade->installer); }
        catch (const std::runtime_error&) { return 0; }
        throw std::runtime_error("Modified installer accepted");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
