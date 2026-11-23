// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

// DocumentViews first: windows.h defines `pascal`, a CyUI code language name.
#include <cyui/modules/DocumentViews.h>

#include "skill_panel.hpp"
#include "ui_kit.hpp"

#include <cyui/Core.h>
#include <algorithm>
#include <cfloat>

namespace hub {
namespace {
using cyui::dp;
using cyui::tr;
using cyui::trLabel;
namespace icon = cyui::icon;

std::string skill_key(const SkillDocument& skill) { return skill.connector + '/' + skill.name; }

// The reader shows name and description above; drop the YAML front matter from the preview.
std::string skill_body(const std::string& markdown) {
    auto text = std::string_view(markdown);
    if (text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3);
    if (!text.starts_with("---")) return markdown;
    const auto first = text.find('\n');
    if (first == std::string_view::npos || text.substr(0, first).find_first_not_of("-\r") != std::string_view::npos) return markdown;
    for (auto start = first + 1; start < text.size();) {
        auto end = text.find('\n', start);
        if (end == std::string_view::npos) end = text.size();
        auto line = text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line == "---" || line == "...") return std::string(text.substr(std::min(end + 1, text.size())));
        start = end + 1;
    }
    return markdown;
}
} // namespace

void register_skill_translations() {
    cyui::registerTranslations("fr", {
        {"Skills", "Skills"}, {"Skill library", "Bibliothèque de skills"},
        {"Collect connector skills", "Collecter les skills du connecteur"},
        {"Collect detected skills", "Collecter les skills détectés"},
        {"Create with Assistant", "Créer avec l’assistant"}, {"Copy Markdown", "Copier le Markdown"},
        {"Local library", "Bibliothèque locale"}, {"No skills in this view", "Aucun skill dans cette vue"},
        {"No skills yet", "Aucun skill pour l’instant"},
        {"Collect a detected skill or create one for a connector.", "Collectez un skill détecté ou créez-en un pour un connecteur."},
        {"Reading skills…", "Lecture des skills…"}, {"Skills collected", "Skills collectés"},
        {"Search skills", "Rechercher un skill"}, {"Preview", "Aperçu"}, {"Markdown", "Markdown"},
        {"Select a skill", "Sélectionnez un skill"}, {"Read its instructions and copy them.", "Lisez ses instructions et copiez-les."},
        {"No skills for this connector", "Aucun skill pour ce connecteur"},
        {"Collect the skills it publishes, or ask the Assistant to write one.",
         "Collectez les skills qu’il publie, ou demandez à l’assistant d’en rédiger un."}
    });
}

struct SkillPanel::Reader {
    std::string key;
    cyui::Document document;
    cyui::DocumentState state;
};

SkillPanel::SkillPanel(std::shared_ptr<Journal> journal) : journal_(std::move(journal)), reader_(std::make_unique<Reader>()) {}

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

void SkillPanel::poll() {
    if (!loaded_) refresh();
    if (!pending_.valid() || pending_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    try {
        auto [skills, count] = pending_.get();
        skills_ = std::move(skills);
        reader_->key.clear();
        if (count) cyui::notify((tr("Skills collected") + "  " + std::to_string(count)).c_str(), cyui::Tone::success);
    } catch (const std::exception& problem) { error_ = cymcp::redact_error_message(problem.what()); }
}

void SkillPanel::refresh() { run([](const auto&) { return std::size_t{0}; }); }

void SkillPanel::collect_found(const std::vector<SkillDocument>& found) {
    run([found](const auto& cancel) {
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

void SkillPanel::draw_document(const SkillDocument& skill) {
    const auto key = skill_key(skill);
    if (reader_->key != key) {
        reader_->document.setMarkdown(skill_body(skill.markdown));
        reader_->state = {};
        reader_->key = key;
    }
    static constexpr const char* modes[] = {"Preview", "Markdown"};
    cyui::segmented("##skill-mode", modes, 2, &mode_);
    ImGui::SameLine();
    const auto copy = kit::with_icon(icon::copy, trLabel("Copy Markdown"));
    cyui::alignRight(cyui::buttonWidth(copy.c_str()));
    if (cyui::button(copy.c_str())) kit::copy_to_clipboard(skill.markdown);
    cyui::space(4);
    if (mode_ == 1) {
        kit::code_box("##skill-markdown", skill.markdown, 0);
        return;
    }
    const cyui::ScopedColor well(ImGuiCol_ChildBg, cyui::palette().bg1);
    const cyui::ScopedVar padding(ImGuiStyleVar_WindowPadding, dp(14, 12));
    const cyui::ScopedVar rounding(ImGuiStyleVar_ChildRounding, dp(cyui::metrics().rounding + 2.0f));
    if (ImGui::BeginChild("##skill-document", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding)) {
        cyui::DocumentOptions options;
        options.linkPreviews = false;
        options.tasks = false;
        cyui::documentView("##skill-view", reader_->state, reader_->document, ImVec2(0, 0), options);
    }
    ImGui::EndChild();
}

std::string SkillPanel::draw_library() {
    if (!error_.empty()) cyui::callout(cyui::Tone::error, error_.c_str());
    const auto available = ImGui::GetContentRegionAvail();
    const auto left = std::clamp(available.x * 0.32f, dp(260), dp(380));
    const auto query = group_key(search_buffer_.data());
    std::vector<const SkillDocument*> visible;
    for (const auto& item : skills_) {
        if (query.empty() || group_key(item.name).find(query) != std::string::npos ||
            group_key(item.description).find(query) != std::string::npos || group_key(item.connector).find(query) != std::string::npos)
            visible.push_back(&item);
    }
    const auto active = std::find_if(visible.begin(), visible.end(), [&](const auto* skill) { return skill_key(*skill) == selected_; });
    const SkillDocument* selected = active != visible.end() ? *active : visible.empty() ? nullptr : visible.front();

    if (kit::begin_pane("##skill-list", ImVec2(left, std::max(available.y, dp(120))), false)) {
        ImGui::Indent(dp(8));
        cyui::space(6);
        cyui::searchField("##skill-search", search_buffer_.data(), static_cast<int>(search_buffer_.size()),
                          tr("Search skills").c_str(), -dp(8));
        ImGui::Unindent(dp(8));
        cyui::space(4);
        if (visible.empty()) {
            if (pending_.valid()) kit::empty_state(tr("Reading skills…"), {}, icon::library);
            else kit::empty_state(tr("No skills yet"), tr("Collect a detected skill or create one for a connector."), icon::library);
        } else {
            if (kit::begin_rows("##skill-rows")) {
                for (const auto* item : visible) {
                    const auto key = skill_key(*item);
                    const auto subtitle = (item->connector == "library" ? tr("Local library") : item->connector) +
                                          (item->description.empty() ? "" : "  ·  " + item->description);
                    if (kit::list_row(key.c_str(), item->name, subtitle, cyui::toneColour(cyui::Tone::accent), {}, item == selected))
                        selected = item;
                }
            }
            kit::end_rows();
        }
    }
    kit::end_pane();
    ImGui::SameLine(0, dp(14));
    if (kit::begin_pane("##skill-reader", ImVec2(0, std::max(available.y, dp(120))))) {
        if (!selected) {
            kit::empty_state(tr("Select a skill"), tr("Read its instructions and copy them."), icon::document);
        } else {
            cyui::heading(selected->name.c_str(), 2);
            cyui::badge(selected->connector == "library" ? tr("Local library").c_str() : selected->connector.c_str(), cyui::Tone::accent);
            if (!selected->path.empty()) {
                ImGui::SameLine();
                const auto path = cymcp::path_utf8(selected->path);
                cyui::textFaint("%s", path.c_str());
                cyui::tooltip(path.c_str());
            }
            if (!selected->description.empty()) kit::wrapped_dim(selected->description);
            cyui::space(6);
            draw_document(*selected);
        }
    }
    kit::end_pane();
    selected_ = selected ? skill_key(*selected) : std::string{};
    return selected ? selected->name : std::string{};
}

std::string SkillPanel::draw_connector(const Server& target, const Options& options, const std::function<void(const Server&)>& draft) {
    {
        const cyui::ScopedDisabled busy(pending_.valid() || target.status == Status::disabled);
        if (cyui::button(kit::with_icon(icon::importFile, trLabel("Collect connector skills")).c_str())) {
            run([server = target, options, journal = journal_](const auto& cancel) {
                McpActions session(server, options, journal);
                return collect_mcp_skills(server, session, cancel).size();
            });
        }
        ImGui::SameLine();
        if (cyui::primaryButton(kit::with_icon(icon::lightbulb, trLabel("Create with Assistant")).c_str())) draft(target);
    }
    if (pending_.valid()) {
        cyui::spinner("##skills-busy", ImGui::GetFontSize() * 0.32f);
        ImGui::SameLine();
        cyui::textDim("%s", tr("Reading skills…").c_str());
    }
    if (!error_.empty()) cyui::callout(cyui::Tone::error, error_.c_str());
    cyui::space(4);
    std::vector<const SkillDocument*> visible;
    for (const auto& item : skills_) if (item.connector == target.id) visible.push_back(&item);
    if (visible.empty()) {
        if (!pending_.valid()) kit::empty_state(tr("No skills for this connector"), tr("Collect the skills it publishes, or ask the Assistant to write one."), icon::library);
        return {};
    }
    const auto active = std::find_if(visible.begin(), visible.end(), [&](const auto* skill) { return skill_key(*skill) == selected_; });
    const SkillDocument* selected = active == visible.end() ? visible.front() : *active;
    if (visible.size() > 1) {
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##skill-selection", selected->name.c_str())) {
            for (const auto* item : visible) {
                const auto id = skill_key(*item);
                const cyui::ScopedID scope(id.c_str());
                if (ImGui::Selectable(item->name.c_str(), selected == item)) selected = item;
            }
            ImGui::EndCombo();
        }
    } else {
        cyui::heading(selected->name.c_str(), 3);
    }
    selected_ = skill_key(*selected);
    if (!selected->description.empty()) kit::wrapped_dim(selected->description);
    cyui::space(4);
    draw_document(*selected);
    return selected->name;
}
} // namespace hub
