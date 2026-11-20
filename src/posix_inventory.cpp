#include "platform.hpp"

#include <cymcp/process.hpp>
#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <fcntl.h>
#include <fstream>
#include <locale.h>
#include <pwd.h>
#include <set>
#include <sstream>
#include <sys/file.h>
#include <thread>
#include <unistd.h>

#ifdef __APPLE__
#include <libproc.h>
#include <mach-o/dyld.h>
#include <sys/sysctl.h>
#endif

namespace hub {
namespace {
unsigned long number(std::string_view text, int base = 10) {
    unsigned long result = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result, base);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() ? result : 0;
}

std::string listener_url(std::string_view address, std::string_view port, int base = 10) {
    const auto value = number(port, base);
    if (!value || value > 65535) return {};
    if (address != "127.0.0.1" && address != "[::1]") return {};
    return "http://" + std::string(address) + ':' + std::to_string(value) + "/mcp";
}

#ifdef __linux__
struct TcpSocket { std::string url, inode; };
void read_tcp(const fs::path& path, bool ipv6, bool& available, std::vector<TcpSocket>& sockets) {
    std::ifstream input(path);
    if (!input) return;
    available = true;
    std::string line;
    std::getline(input, line);
    while (std::getline(input, line)) {
        std::istringstream row(line);
        std::string index, local, remote, state, queue, timer, retries, uid, timeout, inode;
        if (!(row >> index >> local >> remote >> state >> queue >> timer >> retries >> uid >> timeout >> inode) || state != "0A") continue;
        const auto colon = local.rfind(':');
        if (colon == std::string::npos) continue;
        const auto address = local.substr(0, colon);
        const bool loopback = ipv6 ? address == "00000000000000000000000000000000" ||
                                      address == "00000000000000000000000001000000" || address == "00000000000000000000000000000001" :
                                      address == "00000000" || address == "0100007F" || address == "7F000001";
        if (!loopback) continue;
        auto url = listener_url(ipv6 ? "[::1]" : "127.0.0.1", std::string_view(local).substr(colon + 1), 16);
        if (!url.empty()) sockets.push_back({std::move(url), std::move(inode)});
    }
}
#endif
} // namespace

fs::path environment_path(const char* name) {
    if (const auto* value = std::getenv(name)) return from_utf8(value);
    return {};
}

fs::path home_path() {
    if (const auto home = environment_path("HOME"); !home.empty()) return home;
    std::vector<char> buffer(16384);
    passwd account{}, *found = nullptr;
    if (getpwuid_r(getuid(), &account, buffer.data(), buffer.size(), &found) == 0 && found) return from_utf8(account.pw_dir);
    return {};
}

fs::path client_config_path() {
#ifdef __APPLE__
    const auto home = home_path();
    return home.empty() ? fs::path{} : home / "Library/Application Support";
#else
    if (const auto xdg = environment_path("XDG_CONFIG_HOME"); xdg.is_absolute()) return xdg;
    const auto home = home_path();
    return home.empty() ? fs::path{} : home / ".config";
#endif
}

fs::path data_path() {
    if (const auto override = environment_path("CYMCP_HUB_DATA"); !override.empty()) return override;
#ifdef __APPLE__
    if (const auto config = client_config_path(); !config.empty()) return config / "CyMCPHub";
#else
    if (const auto xdg = environment_path("XDG_DATA_HOME"); xdg.is_absolute()) return xdg / "cymcphub";
    if (const auto home = home_path(); !home.empty()) return home / ".local/share/cymcphub";
#endif
    throw std::runtime_error("User data directory is unavailable");
}

fs::path executable_path() {
#ifdef __APPLE__
    std::vector<char> path(4096);
    std::uint32_t size = static_cast<std::uint32_t>(path.size());
    if (_NSGetExecutablePath(path.data(), &size) != 0) {
        path.resize(size);
        if (_NSGetExecutablePath(path.data(), &size) != 0) return {};
    }
    std::error_code error;
    const auto result = fs::canonical(from_utf8(path.data()), error);
    return error ? from_utf8(path.data()) : result;
#else
    std::error_code error;
    return fs::read_symlink("/proc/self/exe", error);
#endif
}

bool same_path(const fs::path& left, const fs::path& right) { return left == right; }
bool path_is_link(const fs::path& path) { std::error_code error; return fs::is_symlink(fs::symlink_status(path, error)); }
unsigned long process_id() { return static_cast<unsigned long>(getpid()); }

bool french_locale() {
    for (const auto* name : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        const auto value = environment_path(name);
        if (!value.empty()) return cymcp::path_utf8(value).starts_with("fr");
    }
    return false;
}

std::string group_key(std::string_view name) {
    static const auto locale = newlocale(LC_CTYPE_MASK, "", nullptr);
    std::u8string output;
    for (std::size_t i = 0; i < name.size();) {
        const auto first = static_cast<unsigned char>(name[i]);
        const std::size_t length = first < 0x80 ? 1 : first < 0xe0 ? 2 : first < 0xf0 ? 3 : 4;
        if (length > name.size() - i || first >= 0xf8 || (first >= 0x80 && first < 0xc2)) {
            output += static_cast<char8_t>(name[i++]);
            continue;
        }
        auto value = static_cast<std::uint32_t>(first & (length == 1 ? 0x7f : 0x7f >> length));
        bool valid = true;
        for (std::size_t j = 1; j < length; ++j) {
            const auto next = static_cast<unsigned char>(name[i + j]);
            valid = valid && (next & 0xc0) == 0x80;
            value = (value << 6) | (next & 0x3f);
        }
        if (!valid) { output += static_cast<char8_t>(name[i++]); continue; }
        i += length;
        value = locale ? static_cast<std::uint32_t>(towlower_l(static_cast<wint_t>(value), locale)) :
                (value >= 'A' && value <= 'Z' ? value + 32 : value);
        if (value <= 0x7f) output += static_cast<char8_t>(value);
        else {
            const int count = value <= 0x7ff ? 2 : value <= 0xffff ? 3 : 4;
            output += static_cast<char8_t>(((0xf0 << (4 - count)) & 0xff) | (value >> (6 * (count - 1))));
            for (int j = count - 2; j >= 0; --j) output += static_cast<char8_t>(0x80 | (value >> (6 * j) & 0x3f));
        }
    }
    return std::string(output.begin(), output.end());
}

FileLock::FileLock(const fs::path& path) {
    const int fd = open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) throw std::runtime_error("Cannot access data lock");
    for (int tries = 0; tries < 40; ++tries) {
        if (flock(fd, LOCK_EX | LOCK_NB) == 0) { handle_ = fd; return; }
        if (errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    close(fd);
    throw std::runtime_error("Data store is busy");
}

FileLock::~FileLock() { close(static_cast<int>(handle_)); }

std::error_code publish_file(const fs::path& temporary, const fs::path& destination, bool replace) {
    if (replace) {
        std::error_code error;
        fs::rename(temporary, destination, error);
        return error;
    }
    if (link(temporary.c_str(), destination.c_str()) != 0) return {errno, std::generic_category()};
    std::error_code ignored;
    fs::remove(temporary, ignored);
    return {};
}

std::vector<Process> running_processes(const cymcp::Cancellation& cancellation, bool& available) {
    std::vector<Process> result;
    available = false;
#ifdef __APPLE__
    const int capacity = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
    if (capacity <= 0) return result;
    std::vector<pid_t> ids(static_cast<std::size_t>(capacity) / sizeof(pid_t) + 256);
    const int used = proc_listpids(PROC_ALL_PIDS, 0, ids.data(), static_cast<int>(ids.size() * sizeof(pid_t)));
    if (used <= 0) return result;
    int maximum = 0;
    size_t bytes = sizeof(maximum);
    int args_max[] = {CTL_KERN, KERN_ARGMAX};
    if (sysctl(args_max, 2, &maximum, &bytes, nullptr, 0) != 0 || maximum <= 0 || maximum > 8 * 1024 * 1024) return result;
    available = true;
    std::vector<char> buffer(static_cast<std::size_t>(maximum));
    for (std::size_t i = 0; i < static_cast<std::size_t>(used) / sizeof(pid_t); ++i) {
        cancellation.check();
        if (ids[i] <= 0) continue;
        int request[] = {CTL_KERN, KERN_PROCARGS2, ids[i]};
        bytes = buffer.size();
        if (sysctl(request, 3, buffer.data(), &bytes, nullptr, 0) != 0 || bytes <= sizeof(int)) continue;
        int count = 0;
        std::memcpy(&count, buffer.data(), sizeof(count));
        const char* begin = buffer.data();
        const auto end = begin + bytes;
        auto p = std::find(begin + sizeof(count), end, '\0');
        while (p != end && !*p) ++p;
        Process process{static_cast<unsigned long>(ids[i]), {}};
        for (int n = 0; n < count && p != end; ++n) {
            auto next = std::find(p, end, '\0');
            if (next == end) break;
            process.arguments.emplace_back(p, next);
            p = next + 1;
        }
        if (!process.arguments.empty()) result.push_back(std::move(process));
    }
#else
    std::error_code error;
    fs::directory_iterator it("/proc", error), end;
    if (error) return result;
    available = true;
    std::string bytes(128 * 1024, '\0');
    for (; it != end && !error; it.increment(error)) {
        cancellation.check();
        const auto pid = number(cymcp::path_utf8(it->path().filename()));
        if (!pid) continue;
        std::ifstream input(it->path() / "cmdline", std::ios::binary);
        if (!input) continue;
        input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        const auto length = static_cast<std::size_t>(input.gcount());
        if (!length || length >= bytes.size() || bytes[length - 1] != '\0') continue;
        Process process{pid, {}};
        for (std::size_t offset = 0; offset < length;) {
            const auto end_of_argument = bytes.find('\0', offset);
            process.arguments.emplace_back(bytes.data() + offset, end_of_argument - offset);
            offset = end_of_argument + 1;
        }
        result.push_back(std::move(process));
    }
#endif
    return result;
}

std::vector<Listener> local_listeners(bool& available) {
    std::vector<Listener> listeners;
    available = false;
#ifdef __APPLE__
    if (access("/usr/sbin/lsof", X_OK) != 0) return listeners;
    try {
        cymcp::ProcessOptions options;
        options.executable = "/usr/sbin/lsof";
        options.arguments = {"-nP", "-iTCP", "-sTCP:LISTEN", "-Fpn"};
        cymcp::Process process(options);
        cymcp::Cancellation timeout;
        timeout.deadline = cymcp::Clock::now() + std::chrono::seconds(3);
        unsigned long pid = 0;
        cymcp::ProcessLine line;
        while (process.next(line, timeout)) {
            if (line.stderr_stream || line.text.empty()) continue;
            const auto text = std::string_view(line.text).substr(1);
            if (line.text.front() == 'p') pid = number(text);
            if (line.text.front() != 'n') continue;
            const auto colon = text.rfind(':');
            if (colon == std::string_view::npos) continue;
            const auto addr = text.substr(0, colon), port = text.substr(colon + 1);
            for (auto host : {std::string_view("127.0.0.1"), std::string_view("[::1]")}) {
                if (addr != host && addr != "*" && addr != "0.0.0.0" && addr != "[::]") continue;
                auto url = listener_url(host, port);
                if (!url.empty()) listeners.push_back({std::move(url), pid});
            }
        }
        const auto code = process.wait(timeout);
        available = code == 0 || code == 1;
    } catch (const cymcp::Error&) {}
#else
    std::vector<TcpSocket> sockets;
    read_tcp("/proc/net/tcp", false, available, sockets);
    read_tcp("/proc/net/tcp6", true, available, sockets);
    std::set<std::string> inodes;
    for (const auto& item : sockets) inodes.insert(item.inode);
    std::map<std::string, unsigned long> owners;
    std::error_code error;
    for (fs::directory_iterator proc("/proc", error), end; proc != end && !error && owners.size() < inodes.size(); proc.increment(error)) {
        const auto pid = number(cymcp::path_utf8(proc->path().filename()));
        if (!pid) continue;
        std::error_code files;
        for (fs::directory_iterator fd(proc->path() / "fd", files); fd != end && !files; fd.increment(files)) {
            std::error_code link_error;
            const auto target = cymcp::path_utf8(fs::read_symlink(fd->path(), link_error));
            if (link_error || !target.starts_with("socket:[") || !target.ends_with(']')) continue;
            const auto inode = target.substr(8, target.size() - 9);
            if (inodes.contains(inode)) owners.try_emplace(inode, pid);
        }
    }
    for (auto& item : sockets) {
        const auto owner = owners.find(item.inode);
        listeners.push_back({std::move(item.url), owner == owners.end() ? 0 : owner->second});
    }
#endif
    return listeners;
}
} // namespace hub
