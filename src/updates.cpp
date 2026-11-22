// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "updates.hpp"
#include "platform.hpp"

#include <cymcp/http.hpp>
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>

namespace hub {
namespace {
constexpr auto releases = "https://github.com/MrMybal/CyMCPHub/releases/";
constexpr auto latest = "https://api.github.com/repos/MrMybal/CyMCPHub/releases/latest";
constexpr std::size_t max_installer_size = 128 * 1024 * 1024;

std::optional<std::array<unsigned, 3>> version_parts(std::string_view version) {
    std::array<unsigned, 3> parts{};
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const auto dot = version.find('.');
        if ((i < parts.size() - 1) != (dot != std::string_view::npos)) return std::nullopt;
        const auto field = version.substr(0, dot);
        if (field.empty() || (field.size() > 1 && field.front() == '0')) return std::nullopt;
        const auto [end, error] = std::from_chars(field.data(), field.data() + field.size(), parts[i]);
        if (error != std::errc{} || end != field.data() + field.size()) return std::nullopt;
        if (dot != std::string_view::npos) version.remove_prefix(dot + 1);
    }
    return parts;
}

bool valid_digest(std::string_view digest) {
    return digest.size() == 64 && std::all_of(digest.begin(), digest.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

bool github_download(std::string_view url) {
    const auto parsed = cymcp::parse_url(std::string(url));
    return parsed.scheme == "https" && parsed.port == 443 &&
           (parsed.host == "github.com" || parsed.host == "release-assets.githubusercontent.com" ||
            parsed.host == "objects.githubusercontent.com");
}

struct RemovePartial {
    fs::path path;
    ~RemovePartial() {
        std::error_code ignored;
        fs::remove(path, ignored);
    }
};
} // namespace

std::optional<AppUpdate> parse_update(const Json& release, std::string_view current, bool windows_x64) {
    const auto tag = release.value("tag_name", std::string{});
    const auto current_parts = version_parts(current);
    const auto next_parts = tag.starts_with('v') ? version_parts(std::string_view(tag).substr(1)) : std::nullopt;
    if (!current_parts || !next_parts) throw std::runtime_error("Unexpected release version");
    if (release.value("draft", false) || release.value("prerelease", false) || *next_parts <= *current_parts)
        return std::nullopt;
    const auto version = tag.substr(1);
    AppUpdate update{version, std::string(releases) + "tag/" + tag, std::nullopt};
    if (!windows_x64) return update;
    const auto name = "CyMCPHub-Setup-" + version + "-windows-x64.exe";
    const auto url = std::string(releases) + "download/" + tag + '/' + name;
    const auto items = release.find("assets");
    if (items == release.end() || !items->is_array()) return update;
    for (const auto& asset : *items) {
        if (!asset.is_object() || asset.value("name", std::string{}) != name) continue;
        const auto digest = asset.value("digest", Json{});
        const auto size = asset.value("size", Json{});
        if (!digest.is_string() || !size.is_number_unsigned() || asset.value("state", std::string{}) != "uploaded" ||
            asset.value("browser_download_url", std::string{}) != url) continue;
        const auto hash = digest.get<std::string>();
        const auto bytes = size.get<std::uint64_t>();
        if (hash.starts_with("sha256:") && valid_digest(std::string_view(hash).substr(7)) &&
            bytes > 0 && bytes <= max_installer_size)
            update.installer = UpdateInstaller{name, url, hash.substr(7), static_cast<std::size_t>(bytes)};
    }
    return update;
}

std::optional<AppUpdate> check_update(const cymcp::Cancellation& cancellation) {
    auto request = cymcp::HttpRequest{};
    request.url = latest;
    request.headers = {{"Accept", "application/vnd.github+json"}, {"X-GitHub-Api-Version", "2022-11-28"}};
    request.timeout = std::chrono::seconds(15);
    request.max_response_bytes = 1024 * 1024;
    const auto response = cymcp::HttpClient().perform(request, cancellation);
    if (response.status != 200) throw std::runtime_error("GitHub returned HTTP " + std::to_string(response.status));
#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
    constexpr bool windows_x64 = true;
#else
    constexpr bool windows_x64 = false;
#endif
    return parse_update(Json::parse(response.body), CYMCP_HUB_VERSION, windows_x64);
}

void verify_installer(const fs::path& path, const UpdateInstaller& installer, const cymcp::Cancellation& cancellation) {
    if (path_is_link(path) || !fs::is_regular_file(path) || fs::file_size(path) != installer.size ||
        cymcp::sha256_file(path, cancellation) != installer.sha256)
        throw std::runtime_error("Installer size or SHA-256 does not match the GitHub release");
}

fs::path download_installer(const UpdateInstaller& installer, const cymcp::Cancellation& cancellation) {
    const auto name = from_utf8(installer.name);
    if (name != name.filename() || name.extension() != ".exe" || installer.size == 0 ||
        installer.size > max_installer_size || !valid_digest(installer.sha256) ||
        !installer.url.starts_with(std::string(releases) + "download/") ||
        !installer.url.ends_with('/' + installer.name))
        throw std::runtime_error("Invalid release installer metadata");
    const auto folder = data_path() / "updates";
    if (path_is_link(folder)) throw std::runtime_error("Update directory cannot be a link");
    cymcp::private_directory(folder);
    const FileLock lock(folder / "download.lock");
    const auto target = folder / name;
    const RemovePartial partial{folder / (cymcp::random_token(16) + ".part")};
    auto file = cymcp::open_private_output_file(partial.path);
    cymcp::HttpRequest request;
    request.url = installer.url;
    request.timeout = std::chrono::minutes(3);
    request.idle_timeout = std::chrono::seconds(20);
    request.max_response_bytes = std::max<std::size_t>(installer.size, 8192);
    request.error_body_limit = 8192;
    for (int attempt = 0; attempt != 4; ++attempt) {
        cancellation.check();
        if (!github_download(request.url)) throw std::runtime_error("Unexpected GitHub download redirect");
        const auto response = cymcp::HttpClient().perform(request, cancellation, [&](const char* bytes, std::size_t length) {
            if (std::fwrite(bytes, 1, length, file.get()) != length)
                throw std::runtime_error("Could not write the installer");
            return true;
        });
        if (response.status == 200) {
            if (std::fflush(file.get()) != 0) throw std::runtime_error("Could not save the installer");
            file.reset();
            verify_installer(partial.path, installer, cancellation);
            if (const auto error = publish_file(partial.path, target, true))
                throw std::system_error(error, "Could not save the verified installer");
            return target;
        }
        const auto location = response.headers.find("location");
        if (response.status < 300 || response.status >= 400 || location == response.headers.end())
            throw std::runtime_error("GitHub download returned HTTP " + std::to_string(response.status));
        request.url = location->second;
    }
    throw std::runtime_error("Too many GitHub download redirects");
}

} // namespace hub
