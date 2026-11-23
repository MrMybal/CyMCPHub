// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "skills.hpp"

#include <array>
#include <memory>

namespace hub {
class SkillPanel {
    std::shared_ptr<Journal> journal_;
    cymcp::Cancellation cancellation_;
    std::future<std::pair<std::vector<SkillDocument>, std::size_t>> pending_;
    std::vector<SkillDocument> skills_;
    std::string selected_, error_, search_;
    std::array<char, 256> search_buffer_{};
    bool loaded_ = false;
    int mode_ = 0;
    struct Reader;
    std::unique_ptr<Reader> reader_;

    void run(std::function<std::size_t(const cymcp::Cancellation&)> action);
    void draw_document(const SkillDocument& skill);

public:
    explicit SkillPanel(std::shared_ptr<Journal> journal);
    ~SkillPanel();
    void poll();
    bool busy() const { return pending_.valid(); }
    std::size_t count() const { return skills_.size(); }
    void refresh();
    void collect_found(const std::vector<SkillDocument>& found);
    // The full library page: list and reader. Returns the selected skill name.
    std::string draw_library();
    // The connector tab: its own skills and actions. Returns the selected skill name.
    std::string draw_connector(const Server& target, const Options& options, const std::function<void(const Server&)>& draft);
};

void register_skill_translations();
} // namespace hub
