#pragma once

#include "hub_service.hpp"
#include <cymcp/mcp.hpp>

namespace hub {
class HubHttpServer {
    std::shared_ptr<cymcp::Registry> tools_;
    cymcp::McpServer mcp_;
    cymcp::LocalHttpServer http_;
public:
    HubHttpServer(Options options, std::shared_ptr<Journal> journal, const std::string& token);
    ~HubHttpServer();
    int start(int port);
};

class ResidentService {
    std::shared_ptr<Journal> journal_;
    std::unique_ptr<HubHttpServer> server_;
    std::string token_, error_;
    Options options_;

    void start(const Options& options, bool persist);
    void stop();

public:
    explicit ResidentService(std::shared_ptr<Journal> journal) : journal_(std::move(journal)) {}
    void restore(const Options& options);
    void toggle(const Options& options);
    void sync_options(const Options& options);
    bool running() const { return server_ != nullptr; }
    const std::string& error() const { return error_; }
    Json configuration() const;
};
} // namespace hub
