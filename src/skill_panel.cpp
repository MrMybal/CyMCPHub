// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "skill_panel.hpp"

#include <cyui/Core.h>
#include <algorithm>
#include <cfloat>

namespace hub {
namespace {
using cyui::tr;
using cyui::trLabel;
}

void register_skill_translations() {
    cyui::registerTranslations("fr", {
        {"Skills", "Skills"}, {"Skill library", "Bibliothèque de skills"},
        {"Collect connector skills", "Collecter les skills du connecteur"},
        {"Collect detected skills", "Collecter les skills détectés"},
        {"Create with Assistant", "Créer avec l’assistant"}, {"Copy Markdown", "Copier le Markdown"},
        {"Local library", "Bibliothèque locale"}, {"No skills in this view", "Aucun skill dans cette vue"},
        {"Collect a detected skill or create one for a connector.", "Collectez un skill détecté ou créez-en un pour un connecteur."},
        {"Reading skills…", "Lecture des skills…"}, {"Skills collected", "Skills collectés"}
    });
}

SkillPanel::~SkillPanel() { cancellation_.cancel(); }

void SkillPanel::run(std::function<std::size_t(const cymcp::Cancellation&)> action) {
    if (pending_.valid()) return;
    loaded_ = true;
    error_.clear();
    cancellation_ = {};
    pending_ = std::async(std::launch::async, [action = std::move(action), cancel = cancellation_] {
        auto bounded = cancel;
        bounded.deadline = cymcp::Clock::now() + std::chrono::seconds(90);
        const auto count = action(bounded);
        return std::pair{list_skills(), count};
    });
}

std::string SkillPanel::draw(const Server* target, const Options& options, const std::vector<SkillDocument>& found,
                            const std::function<void(const Server&)>& draft) {
    if (!loaded_) run([](const auto&) { return std::size_t{0}; });
    if (pending_.valid() && pending_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            auto [skills, count] = pending_.get();
            skills_ = std::move(skills);
            if (count) cyui::notify((tr("Skills collected") + "  " + std::to_string(count)).c_str(), cyui::Tone::success);
        } catch (const std::exception& problem) { error_ = cymcp::redact_error_message(problem.what()); }
    }
    {
        const cyui::ScopedDisabled busy(pending_.valid());
        if (cyui::button(trLabel("Refresh").c_str())) run([](const auto&) { return std::size_t{0}; });
        if (target) {
            ImGui::SameLine();
            const cyui::ScopedDisabled disabled(target->status == Status::disabled);
            if (cyui::button(trLabel("Collect connector skills").c_str())) {
                run([server = *target, options, journal = journal_](const auto& cancel) {
                    McpActions session(server, options, journal);
                    return collect_mcp_skills(server, session, cancel).size();
                });
            }
            ImGui::SameLine();
            if (cyui::primaryButton(trLabel("Create with Assistant").c_str())) draft(*target);
        } else if (!found.empty()) {
            ImGui::SameLine();
            const auto label = tr("Collect detected skills") + "  (" + std::to_string(found.size()) + ")###collect-found";
            if (cyui::button(label.c_str())) run([found](const auto& cancel) {
                std::size_t imported = 0;
                for (const auto& item : found) {
                    cancel.check();
                    try {
                        save_skill("library", read_skill_file(item.path).markdown);
                        ++imported;
                    } catch (const cymcp::Error& error) { if (error.code != "AlreadyExists") throw; }
                }
                return imported;
            });
        }
    }
    if (pending_.valid()) cyui::textDim("%s", tr("Reading skills…").c_str());
    if (!error_.empty()) cyui::callout(cyui::Tone::error, error_.c_str());
    auto key = [](const SkillDocument& skill) { return skill.connector + '/' + skill.name; };
    std::vector<const SkillDocument*> visible;
    for (const auto& item : skills_) if (!target || item.connector == target->id) visible.push_back(&item);
    if (visible.empty()) {
        if (!pending_.valid()) cyui::emptyState(tr("No skills in this view").c_str(), tr("Collect a detected skill or create one for a connector.").c_str());
        return {};
    }
    const auto active = std::find_if(visible.begin(), visible.end(), [&](const auto* skill) { return key(*skill) == selected_; });
    const SkillDocument* selected = active == visible.end() ? visible.front() : *active;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##skill-selection", selected->name.c_str())) {
        for (const auto* item : visible) {
            const auto id = key(*item);
            const cyui::ScopedID scope(id.c_str());
            if (ImGui::Selectable(item->name.c_str(), selected == item)) selected = item;
        }
        ImGui::EndCombo();
    }
    selected_ = key(*selected);
    ImGui::TextWrapped("%s", selected->description.c_str());
    auto markdown = selected->markdown;
    const cyui::ScopedFont font(cyui::monoFont());
    ImGui::InputTextMultiline("##skill-markdown", markdown.data(), markdown.size() + 1,
                             ImVec2(-1, cyui::dp(235)), ImGuiInputTextFlags_ReadOnly);
    if (cyui::button(trLabel("Copy Markdown").c_str())) ImGui::SetClipboardText(markdown.c_str());
    return selected->name;
}
} // namespace hub
