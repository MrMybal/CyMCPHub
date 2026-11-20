// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "mcp_actions.hpp"
#include <cymcp/chat_ui.hpp>

namespace hub {
struct AssistantContext {
    std::mutex mutex;
    Json value = Json::object();
};

class AssistantPanel {
    std::shared_ptr<Journal> journal_;
    std::shared_ptr<AssistantContext> context_ = std::make_shared<AssistantContext>();
    std::shared_ptr<cymcp::ProviderRegistry> providers_;
    cymcp::ChatController chat_;
    cymcp::ChatPanel panel_;
    cymcp::Cancellation discovery_cancel_;
    std::future<std::vector<cymcp::DiscoveredConnection>> discovering_;
    std::vector<cymcp::DiscoveredConnection> detected_;
    std::map<std::string, std::string> transports_;
    std::optional<Server> target_;
    std::string provider_id_, connected_target_, error_;
    std::uint64_t last_diagnostic_ = 0;
    bool discovery_started_ = false;

    void discover();
    void connect(const cymcp::DiscoveredConnection&, const Options&);
    bool can_use_connector(const std::string& provider) const;

public:
    explicit AssistantPanel(std::shared_ptr<Journal> journal);
    ~AssistantPanel();
    void attach(const Server& server);
    void draft_skill(const Server& server);
    void set_context(Json context);
    void poll();
    void draw(const Options& options);
};

void register_assistant_translations();
} // namespace hub
