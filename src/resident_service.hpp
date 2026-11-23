// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "app_settings.hpp"
#include "hub_service.hpp"
#include <cymcp/mcp.hpp>

namespace hub {
class HubHttpServer {
    std::shared_ptr<cymcp::Registry> tools_;
    cymcp::McpServer mcp_;
    std::unique_ptr<cymcp::LocalHttpServer> http_;
public:
    HubHttpServer(Options options, std::shared_ptr<Journal> journal, const std::string& token, bool require_token = true);
    ~HubHttpServer();
    int start(int port);
};

class ResidentService {
    std::shared_ptr<Journal> journal_;
    std::unique_ptr<HubHttpServer> server_;
    std::string token_, error_;
    Options options_;
    McpServiceSettings settings_;

    void start(const Options& options, const McpServiceSettings& settings);
    void stop();

public:
    explicit ResidentService(std::shared_ptr<Journal> journal) : journal_(std::move(journal)) {}
    void set_running(bool enabled, const Options& options, const McpServiceSettings& settings);
    void sync_options(const Options& options, const McpServiceSettings& settings);
    bool running() const { return server_ != nullptr; }
    const std::string& error() const { return error_; }
    Json configuration() const;
};
} // namespace hub
