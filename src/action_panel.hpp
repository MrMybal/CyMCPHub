// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "mcp_actions.hpp"

namespace hub {
class ActionPanel {
    enum class Operation { tools, resources, call, read, disconnect };
    struct State {
        std::shared_ptr<McpActions> session;
        cymcp::Cancellation cancellation;
        std::future<Json> pending;
        Operation operation = Operation::tools;
        Json tools = Json::array(), resources = Json::array();
        std::string error, result, result_name, uri;
        bool resources_result = false;
        int tool_index = 0;
        std::vector<char> arguments;
    };
    std::map<std::string, State> states_;
    std::shared_ptr<Journal> journal_;

    void poll(State&);
    void start(State&, const Server&, const Options&, Operation, std::string name = {}, Json args = Json::object());
    void draw_tools(State&, const Server&, const Options&);
    void draw_resources(State&, const Server&, const Options&);

public:
    explicit ActionPanel(std::shared_ptr<Journal> journal) : journal_(std::move(journal)) {}
    ~ActionPanel();
    void draw(const Server&, const Options&, bool resources);
};

void register_action_translations();
} // namespace hub
