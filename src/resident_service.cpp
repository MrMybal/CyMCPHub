// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

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
#ifdef _WIN32
constexpr wchar_t registry_path[] = L"Software\\CyberAlien\\CyMCPHub";

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

HubHttpServer::HubHttpServer(Options options, std::shared_ptr<Journal> journal, const std::string& token,
                             bool require_token)
    : tools_(make_hub_registry(std::move(options), std::move(journal))), mcp_(*tools_, "CyMCPHub") {
    const auto dispatch = [this](const Json& request, const cymcp::CallContext& context) {
        return mcp_.handle(request, context);
    };
    if (require_token)
        http_ = std::make_unique<cymcp::LocalHttpServer>(token, dispatch);
    else
        http_ = std::make_unique<cymcp::LocalHttpServer>(cymcp::LocalHttpServer::UnauthenticatedLoopback{}, dispatch);
}

HubHttpServer::~HubHttpServer() {
    mcp_.cancel_all();
    http_->stop();
}

int HubHttpServer::start(int port) { return http_->start(port); }

void ResidentService::start(const Options& options, const McpServiceSettings& settings) {
    std::string token;
    if (settings.require_token) {
        token = read_token();
        if (token.size() < 32) {
            token = cymcp::random_token();
            save_token(token);
        }
    }
    auto server = std::make_unique<HubHttpServer>(options, journal_, token, settings.require_token);
    const auto port = server->start(settings.port);
    token_ = std::move(token);
    options_ = options;
    settings_ = settings;
    settings_.port = port;
    server_ = std::move(server);
    error_.clear();
    journal_->add("CyMCPHub", "mcp/service", "OK", mcp_service_url(port));
}

void ResidentService::stop() {
    server_.reset();
    token_.clear();
    error_.clear();
    journal_->add("CyMCPHub", "mcp/service", "Stopped");
}

void ResidentService::set_running(bool enabled, const Options& options, const McpServiceSettings& settings) {
    if (running() == enabled) return;
    try {
        if (enabled) start(options, settings);
        else stop();
    } catch (const std::exception& error) {
        error_ = cymcp::redact_error_message(error.what());
        journal_->add("CyMCPHub", "mcp/service", "Error", error_);
    }
}

void ResidentService::sync_options(const Options& options, const McpServiceSettings& settings) {
    if (!running() || (options.workspace == options_.workspace && options.extra_config == options_.extra_config &&
                       options.scan_ports == options_.scan_ports && settings.port == settings_.port &&
                       settings.require_token == settings_.require_token)) return;
    stop();
    set_running(true, options, settings);
}

Json ResidentService::configuration() const {
    Json connector = {{"type", "http"}, {"url", mcp_service_url(settings_.port)}};
    if (settings_.require_token && !token_.empty()) connector["headers"] = {{"Authorization", "Bearer " + token_}};
    return {{"mcpServers", {{"cymcphub", std::move(connector)}}}};
}
} // namespace hub
