// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "mcp_actions.hpp"

namespace hub {
struct SkillDocument {
    std::string connector, name, description, markdown;
    fs::path path;
    Json to_json(bool content = false) const;
};

SkillDocument read_skill_file(const fs::path& path);
SkillDocument save_skill(const std::string& connector, const std::string& markdown, bool replace = false);
std::vector<SkillDocument> list_skills();
std::vector<SkillDocument> collect_mcp_skills(const Server& server, McpActions& session, const cymcp::Cancellation& cancellation);
std::string skill_markdown(std::string_view name, std::string_view description, std::string_view body);
} // namespace hub
