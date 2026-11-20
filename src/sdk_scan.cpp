#include "sdk_scan.hpp"
#include "platform.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <set>
#include <span>
#ifndef _WIN32
#include <unistd.h>
#endif

namespace hub {
namespace {
using namespace std::string_view_literals;

struct NamedCli { const char* executable; const char* name; };
constexpr NamedCli known_clis[] = {
    {"codex", "Codex"}, {"codex-x86_64-pc-windows-msvc", "Codex"}, {"codex-aarch64-pc-windows-msvc", "Codex"},
    {"codex-x86_64-unknown-linux-musl", "Codex"}, {"codex-aarch64-unknown-linux-musl", "Codex"},
    {"codex-x86_64-apple-darwin", "Codex"}, {"codex-aarch64-apple-darwin", "Codex"},
    {"claude", "Claude Code"}, {"gemini", "Gemini CLI"}, {"opencode", "OpenCode"},
    {"cursor-agent", "Cursor Agent"}, {"copilot", "GitHub Copilot CLI"}, {"aider", "Aider"},
    {"amp", "Amp"}, {"goose", "Goose"}, {"qwen", "Qwen Code"}, {"kiro-cli", "Kiro CLI"}
};

std::string folded(const fs::path& path) { return group_key(cymcp::path_utf8(path)); }

std::string path_key(const fs::path& path) {
#ifdef _WIN32
    return folded(path);
#else
    return cymcp::path_utf8(path);
#endif
}

bool inside(const fs::path& path, const fs::path& root) {
    auto part = path.begin();
    for (const auto& expected : root) {
        if (part == path.end() || !same_path(*part, expected)) return false;
        ++part;
    }
    return true;
}

const char* cli_name(const fs::path& path) {
    const auto extension = folded(path.extension());
    if (extension != ".exe" && extension != ".cmd" && extension != ".bat" && extension != ".ps1" &&
        extension != ".com" && !extension.empty()) return nullptr;
    const auto stem = folded(path.stem());
    for (const auto& cli : known_clis) if (stem == cli.executable) return cli.name;
    return nullptr;
}

bool binary_file(std::string_view extension) {
    return extension == ".exe" || extension == ".dll" || extension == ".lib" ||
           extension == ".a" || extension == ".so" || extension == ".dylib" ||
           extension == ".pyd" || extension == ".node";
}

bool native_binary(const fs::path& path, const std::string& extension, const std::string& name) {
    if (binary_file(extension) || name.find(".so.") != std::string::npos) return true;
    if (!extension.empty()) return false;
    std::array<unsigned char, 4> magic{};
    std::ifstream file(path, std::ios::binary);
    if (!file.read(reinterpret_cast<char*>(magic.data()), magic.size())) return false;
    const auto signature = (std::uint32_t(magic[0]) << 24) | (std::uint32_t(magic[1]) << 16) |
                           (std::uint32_t(magic[2]) << 8) | magic[3];
    switch (signature) {
    case 0x7f454c46: // ELF
    case 0xfeedface: case 0xcefaedfe: case 0xfeedfacf: case 0xcffaedfe: // Mach-O
    case 0xcafebabe: case 0xbebafeca: case 0xcafebabf: case 0xbfbafeca: // Universal
        return true;
    default:
        return false;
    }
}

bool source_file(std::string_view name, std::string_view extension) {
    return name == "cmakelists.txt" || extension == ".cmake" || extension == ".vcxproj" ||
           extension == ".props" || extension == ".targets" || extension == ".c" || extension == ".cpp" ||
           extension == ".cxx" || extension == ".cc" || extension == ".h" || extension == ".hpp" ||
           name == "vcpkg.json" || name == "conanfile.txt" || name == "conanfile.py";
}

constexpr std::array binary_markers{"cymcp/tools/get"sv, "com.cymcp/toolLookup"sv,
                                    "CyMCPSDK/"sv, "@cymcp@@"sv, "_ZN5cymcp"sv};
constexpr std::array source_markers{"CyMCP::"sv, "cymcp/"sv, "CyMCPSDK"sv, "cymcp_mcp.lib"sv};
constexpr auto mebibyte = std::size_t{1024 * 1024};

std::vector<std::string> sniff(const fs::path& path, bool binary, std::size_t limit, std::size_t& remaining,
                               SoftwareScan& scan, const cymcp::Cancellation& cancellation) {
    std::ifstream input(path, std::ios::binary);
    if (!input) { ++scan.unreadable; return {}; }
    const auto markers = binary ? std::span<const std::string_view>(binary_markers) :
                                  std::span<const std::string_view>(source_markers);
    std::vector<std::string> evidence;
    constexpr std::size_t overlap = 31;
    std::string block(64 * 1024 + overlap, '\0');
    std::size_t tail = 0;
    while (limit && remaining && evidence.size() < markers.size()) {
        cancellation.check();
        const auto count = std::min({block.size() - tail, limit, remaining});
        input.read(block.data() + tail, static_cast<std::streamsize>(count));
        const auto bytes = static_cast<std::size_t>(input.gcount());
        limit -= bytes;
        remaining -= bytes;
        const auto view = std::string_view(block.data(), tail + bytes);
        for (const auto marker : markers) {
            if (view.find(marker) != std::string_view::npos &&
                std::find(evidence.begin(), evidence.end(), marker) == evidence.end()) evidence.emplace_back(marker);
        }
        if (bytes < count) break;
        tail = std::min(overlap, view.size());
        std::memmove(block.data(), view.data() + view.size() - tail, tail);
    }
    if (input.bad()) ++scan.unreadable;
    else if ((!limit || !remaining) && evidence.size() < markers.size() && input.peek() != std::char_traits<char>::eof())
        scan.partial = true;
    return evidence;
}

void add_detected_skill(const fs::path& path, SoftwareScan& scan) {
    if (scan.skills.size() >= 128) { scan.partial = true; return; }
    try {
        auto skill = read_skill_file(path);
        skill.markdown.clear();
        if (std::none_of(scan.skills.begin(), scan.skills.end(), [&](const auto& item) { return item.path == skill.path; }))
            scan.skills.push_back(std::move(skill));
    } catch (const cymcp::Error&) { ++scan.unreadable; }
}

void search_skills(const fs::path& target, SoftwareScan& scan, const cymcp::Cancellation& cancellation) {
    std::error_code error;
    fs::recursive_directory_iterator entry(target, fs::directory_options::skip_permission_denied, error), end;
    std::size_t seen = 0;
    while (entry != end && !error && seen++ < 2048 && scan.skills.size() < 128) {
        cancellation.check();
        if (entry.depth() >= 5 || path_is_link(entry->path())) entry.disable_recursion_pending();
        if (folded(entry->path().filename()) == "skill.md") add_detected_skill(entry->path(), scan);
        entry.increment(error);
    }
    if (entry != end && !error) scan.partial = true;
}

void inspect_file(const fs::path& path, bool single, std::size_t& remaining, SoftwareScan& scan,
                  const cymcp::Cancellation& cancellation) {
    if (const auto* cli = cli_name(path))
        scan.findings.push_back({cli, "cli", path, {"Known executable name"}});
    const auto extension = folded(path.extension());
    const auto name = folded(path.filename());
    if (name == "skill.md") {
        ++scan.examined;
        add_detected_skill(path, scan);
        return;
    }
    const bool binary = native_binary(path, extension, name);
    if (!binary && !source_file(name, extension)) return;
    ++scan.examined;
    if (binary && (name.starts_with("cymcp_") || name.starts_with("libcymcp_") || name.starts_with("cymcpsdk.")) &&
        (extension == ".lib" || extension == ".dll" || extension == ".a" || extension == ".so" ||
         extension == ".dylib" || name.find(".so.") != std::string::npos)) {
        scan.findings.push_back({"CyMCPSDK", "sdk_library", path, {"SDK library filename"}});
        return;
    }
    auto evidence = sniff(path, binary, binary ? (single ? 256 : 64) * mebibyte : 2 * mebibyte,
                          remaining, scan, cancellation);
    if (!evidence.empty())
        scan.findings.push_back({"CyMCPSDK", binary ? "sdk_binary" : "sdk_reference", path, std::move(evidence)});
}

void path_clis(SoftwareScan& scan, const cymcp::Cancellation& cancellation) {
    std::set<std::string> found;
    for (const auto& item : scan.findings) if (item.kind == "cli") found.insert(path_key(item.path));
    std::vector<fs::path> directories;
    const auto path = cymcp::path_utf8(environment_path("PATH"));
#ifdef _WIN32
    constexpr char separator = ';';
#else
    constexpr char separator = ':';
#endif
    for (std::size_t start = 0; start < path.size();) {
        auto end = path.find(separator, start);
        auto entry = path.substr(start, end == std::string::npos ? end : end - start);
        if (entry.size() >= 2 && entry.front() == '"' && entry.back() == '"') entry = entry.substr(1, entry.size() - 2);
        if (!entry.empty()) directories.push_back(from_utf8(entry));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    if (const auto home = home_path(); !home.empty()) {
        for (const auto* dir : {".local/bin", ".cargo/bin", ".bun/bin"}) directories.push_back(home / dir);
#ifdef _WIN32
        directories.push_back(home / "scoop/shims");
#endif
    }
#ifdef _WIN32
    if (const auto local = environment_path("LOCALAPPDATA"); !local.empty()) directories.push_back(local / "Microsoft/WinGet/Links");
    if (const auto appdata = environment_path("APPDATA"); !appdata.empty()) directories.push_back(appdata / "npm");
#else
    directories.emplace_back("/usr/local/bin");
#ifdef __APPLE__
    directories.emplace_back("/opt/homebrew/bin");
#endif
#endif

    std::set<std::string> visited;
    for (const auto& dir : directories) {
        if (!dir.is_absolute() || cymcp::path_utf8(dir.root_name()).starts_with("\\\\") ||
            !visited.insert(path_key(dir.lexically_normal())).second) continue;
        cancellation.check();
        for (const auto& cli : known_clis) {
#ifdef _WIN32
            constexpr auto extensions = std::array{".exe", ".cmd", ".bat", ".ps1"};
#else
            constexpr auto extensions = std::array{""};
#endif
            for (const auto* extension : extensions) {
                const auto executable = dir / (std::string(cli.executable) + extension);
                std::error_code error;
                if (!fs::is_regular_file(executable, error)) continue;
#ifndef _WIN32
                if (access(executable.c_str(), X_OK) != 0) continue;
#endif
                if (found.insert(path_key(executable)).second)
                    scan.findings.push_back({cli.name, "cli", executable, {"Known executable name", "CLI install directory"}});
            }
        }
    }
}
} // namespace

Json SoftwareFinding::to_json() const {
    return {{"name", name}, {"kind", kind}, {"path", cymcp::path_utf8(path)}, {"evidence", evidence}};
}

fs::path workspace_target(const fs::path& workspace, const std::string& target) {
    const auto root = fs::canonical(workspace);
    const auto relative = fs::path(std::u8string(target.begin(), target.end()));
    if (relative.has_root_path()) throw cymcp::Error("InvalidPath", "Use a path relative to the workspace");
    std::error_code error;
    const auto path = fs::canonical(root / relative, error);
    if (error || !inside(path, root)) throw cymcp::Error("InvalidPath", "Scan target must exist within the workspace");
    return path;
}

SoftwareScan scan_software(const fs::path& target, bool include_path, const cymcp::Cancellation& cancellation) {
    SoftwareScan scan;
    std::size_t remaining = 384 * mebibyte;
    if (!target.empty()) {
        std::error_code error;
        const auto status = fs::status(target, error);
        if (error || (!fs::is_regular_file(status) && !fs::is_directory(status)))
            throw cymcp::Error("InvalidPath", "Scan target must be an existing file or directory");
        if (fs::is_regular_file(status)) {
            inspect_file(target, true, remaining, scan, cancellation);
            if (folded(target.filename()) != "skill.md") {
                search_skills(target.parent_path() / "skills", scan, cancellation);
                const auto companion = target.parent_path() / "SKILL.md";
                if (fs::is_regular_file(companion, error)) add_detected_skill(companion, scan);
            }
        } else {
            fs::recursive_directory_iterator item(target, fs::directory_options::skip_permission_denied, error), end;
            std::size_t visited = 0;
            if (error) ++scan.unreadable;
            while (item != end) {
                cancellation.check();
                if (++visited > 20000 || scan.examined >= 6000 || scan.findings.size() >= 500 || !remaining) {
                    scan.partial = true;
                    break;
                }
                const auto path = item->path();
                std::error_code access;
                const auto status = fs::symlink_status(path, access);
                if (access) {
                    ++scan.unreadable;
                } else if (path_is_link(path)) {
                    item.disable_recursion_pending();
                } else if (fs::is_directory(status)) {
                    const auto name = folded(path.filename());
                    if (name == ".git" || name == "node_modules" || name == ".venv" || name == ".vs" ||
                        item.depth() >= 12) item.disable_recursion_pending();
                    if (item.depth() >= 12) scan.partial = true;
                } else if (fs::is_regular_file(status)) {
                    inspect_file(path, false, remaining, scan, cancellation);
                }
                item.increment(error);
                if (error) ++scan.unreadable;
            }
        }
    }
    if (include_path) {
        path_clis(scan, cancellation);
        const auto home = home_path();
        const auto codex = environment_path("CODEX_HOME");
        if (!codex.empty()) search_skills(codex / "skills", scan, cancellation);
        if (!home.empty()) for (const auto* subdir : {".codex/skills", ".claude/skills", ".cursor/skills", ".gemini/skills",
                                                        ".config/opencode/skills"})
            search_skills(home / subdir, scan, cancellation);
    }
    std::sort(scan.findings.begin(), scan.findings.end(), [](const SoftwareFinding& a, const SoftwareFinding& b) {
        if (a.kind != b.kind) return a.kind < b.kind;
        if (a.name != b.name) return a.name < b.name;
        return a.path < b.path;
    });
    return scan;
}
} // namespace hub
