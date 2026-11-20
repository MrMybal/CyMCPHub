// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "skills.hpp"

namespace hub {
class SkillPanel {
    std::shared_ptr<Journal> journal_;
    cymcp::Cancellation cancellation_;
    std::future<std::pair<std::vector<SkillDocument>, std::size_t>> pending_;
    std::vector<SkillDocument> skills_;
    std::string selected_, error_;
    bool loaded_ = false;

    void run(std::function<std::size_t(const cymcp::Cancellation&)> action);

public:
    explicit SkillPanel(std::shared_ptr<Journal> journal) : journal_(std::move(journal)) {}
    ~SkillPanel();
    std::string draw(const Server* target, const Options& options, const std::vector<SkillDocument>& found,
                     const std::function<void(const Server&)>& draft);
};

void register_skill_translations();
} // namespace hub
