#include "resident_service.hpp"
#include "platform.hpp"

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace hub {
namespace {
constexpr int service_port = 48786;
#ifdef _WIN32
constexpr wchar_t registry_path[] = L"Software\\CyberAlien\\CyMCPHub";

bool enabled() {
    DWORD value = 0, size = sizeof(value);
    return RegGetValueW(HKEY_CURRENT_USER, registry_path, L"McpEnabled", RRF_RT_REG_DWORD, nullptr, &value, &size) == ERROR_SUCCESS && value;
}

void set_enabled(DWORD value) {
    if (RegSetKeyValueW(HKEY_CURRENT_USER, registry_path, L"McpEnabled", REG_DWORD, &value, sizeof(value)) != ERROR_SUCCESS)
        throw std::runtime_error("Cannot save MCP service settings");
}

std::string read_token() {
    DWORD size = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, registry_path, L"McpToken", RRF_RT_REG_BINARY, nullptr, nullptr, &size) != ERROR_SUCCESS ||
        !size || size > 8192) return {};
    std::vector<BYTE> bytes(size);
    if (RegGetValueW(HKEY_CURRENT_USER, registry_path, L"McpToken", RRF_RT_REG_BINARY, nullptr, bytes.data(), &size) != ERROR_SUCCESS) return {};
    DATA_BLOB encrypted{size, bytes.data()}, plain{};
    if (!CryptUnprotectData(&encrypted, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &plain)) return {};
    const std::unique_ptr<void, decltype(&LocalFree)> buffer(plain.pbData, LocalFree);
    return std::string(reinterpret_cast<const char*>(plain.pbData), plain.cbData);
}

void save_token(std::string& token) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, registry_path, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        throw std::runtime_error("Cannot create MCP service settings");
    const std::unique_ptr<std::remove_pointer_t<HKEY>, decltype(&RegCloseKey)> registry(key, RegCloseKey);
    DATA_BLOB plain{static_cast<DWORD>(token.size()), reinterpret_cast<BYTE*>(token.data())}, encrypted{};
    if (!CryptProtectData(&plain, L"CyMCPHub MCP", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &encrypted))
        throw std::runtime_error("Cannot protect MCP service credential");
    const std::unique_ptr<void, decltype(&LocalFree)> buffer(encrypted.pbData, LocalFree);
    if (RegSetValueExW(registry.get(), L"McpToken", 0, REG_BINARY, encrypted.pbData, encrypted.cbData) != ERROR_SUCCESS)
        throw std::runtime_error("Cannot save MCP service credential");
}
#else
fs::path service_directory() { return data_path() / "mcp-service"; }

bool enabled() {
    std::error_code error;
    return fs::is_regular_file(service_directory() / "enabled", error);
}

void set_enabled(bool value) {
    const auto path = service_directory() / "enabled";
    if (value) {
        auto file = cymcp::open_private_output_file(path);
        if (!file) throw std::runtime_error("Cannot save MCP service settings");
    } else {
        std::error_code error;
        fs::remove(path, error);
        if (error) throw std::runtime_error("Cannot save MCP service settings");
    }
}

std::string read_token() {
    const int file = open((service_directory() / "token").c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    if (file < 0) return {};
    struct stat properties{};
    char token[1025]{};
    const auto length = fstat(file, &properties) == 0 && S_ISREG(properties.st_mode) &&
                        properties.st_uid == getuid() && !(properties.st_mode & 0077) ? read(file, token, sizeof(token)) : -1;
    close(file);
    if (length < 32 || length > 1024) return {};
    return std::string(token, static_cast<std::size_t>(length));
}

void save_token(std::string& token) {
    const auto directory = service_directory();
    fs::create_directories(directory.parent_path());
    cymcp::private_directory(directory);
    auto file = cymcp::open_private_output_file(directory / "token");
    if (std::fwrite(token.data(), 1, token.size(), file.get()) != token.size() || std::fflush(file.get()) != 0)
        throw std::runtime_error("Cannot save MCP service credential");
}
#endif
} // namespace

HubHttpServer::HubHttpServer(Options options, std::shared_ptr<Journal> journal, const std::string& token)
    : tools_(make_hub_registry(std::move(options), std::move(journal))), mcp_(*tools_, "CyMCPHub"),
      http_(token, [this](const Json& request, const cymcp::CallContext& context) {
          return mcp_.handle(request, context);
      }) {}

HubHttpServer::~HubHttpServer() {
    mcp_.cancel_all();
    http_.stop();
}

int HubHttpServer::start(int port) { return http_.start(port); }

void ResidentService::start(const Options& options, bool persist) {
    auto token = read_token();
    if (token.size() < 32) {
        token = cymcp::random_token();
        save_token(token);
    }
    auto server = std::make_unique<HubHttpServer>(options, journal_, token);
    const auto port = server->start(service_port);
    if (persist) set_enabled(1);
    token_ = std::move(token);
    options_ = options;
    server_ = std::move(server);
    error_.clear();
    journal_->add("CyMCPHub", "mcp/service", "OK", "127.0.0.1:" + std::to_string(port));
}

void ResidentService::stop() {
    set_enabled(0);
    server_.reset();
    token_.clear();
    error_.clear();
    journal_->add("CyMCPHub", "mcp/service", "Stopped");
}

void ResidentService::restore(const Options& options) {
    try { if (enabled()) start(options, false); }
    catch (const std::exception& error) {
        error_ = cymcp::redact_error_message(error.what());
        journal_->add("CyMCPHub", "mcp/service", "Error", error_);
    }
}

void ResidentService::toggle(const Options& options) {
    try {
        if (running()) stop();
        else start(options, true);
    } catch (const std::exception& error) {
        error_ = cymcp::redact_error_message(error.what());
        journal_->add("CyMCPHub", "mcp/service", "Error", error_);
    }
}

void ResidentService::sync_options(const Options& options) {
    if (!running() || (options.workspace == options_.workspace && options.extra_config == options_.extra_config &&
                       options.scan_ports == options_.scan_ports)) return;
    server_.reset();
    restore(options);
}

Json ResidentService::configuration() const {
    return {{"mcpServers", {{"cymcphub", {{"type", "http"}, {"url", "http://127.0.0.1:" + std::to_string(service_port) + "/mcp"},
                {"headers", {{"Authorization", "Bearer " + token_}}}}}}}};
}
} // namespace hub
