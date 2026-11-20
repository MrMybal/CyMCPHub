#pragma once

#include "catalog.hpp"
#include "skills.hpp"

namespace hub {
struct SoftwareFinding {
    std::string name, kind;
    fs::path path;
    std::vector<std::string> evidence;
    Json to_json() const;
};

struct SoftwareScan {
    std::vector<SoftwareFinding> findings;
    std::vector<SkillDocument> skills;
    std::size_t examined = 0, unreadable = 0;
    bool partial = false;
};

SoftwareScan scan_software(const fs::path& target, bool include_path,
                           const cymcp::Cancellation& cancellation = {});
fs::path workspace_target(const fs::path& workspace, const std::string& target);
} // namespace hub
