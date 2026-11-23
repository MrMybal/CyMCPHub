// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "ui_kit.hpp"

#include <algorithm>
#include <cfloat>
#include <vector>

namespace hub::kit {
using cyui::dp;

std::string with_icon(const char* icon, const std::string& label) {
    if (!icon || !cyui::iconsAvailable()) return label;
    return std::string(icon) + "  " + label;
}

float actions_width(std::span<const Action> list) {
    float width = 0;
    for (const auto& action : list) {
        if (width > 0) width += ImGui::GetStyle().ItemSpacing.x;
        width += cyui::buttonWidth(with_icon(action.icon, action.label).c_str());
    }
    return width;
}

int actions(std::span<const Action> list) {
    int clicked = -1;
    for (std::size_t i = 0; i < list.size(); ++i) {
        const auto& action = list[i];
        if (i) ImGui::SameLine();
        const cyui::ScopedDisabled disabled(!action.enabled);
        if (cyui::button(with_icon(action.icon, action.label).c_str(), action.variant)) clicked = static_cast<int>(i);
        if (!action.tooltip.empty()) cyui::tooltip(action.tooltip.c_str());
    }
    return clicked;
}

int page_header(const char* title, const std::string& subtitle, std::span<const Action> list) {
    const auto& palette = cyui::palette();
    const auto start = ImGui::GetCursorScreenPos();
    const auto avail = ImGui::GetContentRegionAvail().x;
    const auto width = actions_width(list);
    cyui::heading(title, 1);
    if (!subtitle.empty()) {
        const auto position = ImGui::GetCursorScreenPos();
        const auto room = std::max(dp(60), avail - (width > 0 ? width + dp(20) : 0.0f));
        cyui::clippedText(ImGui::GetWindowDrawList(), position, room, palette.textFaint, subtitle.c_str());
        ImGui::Dummy(ImVec2(room, ImGui::GetTextLineHeight()));
        if (ImGui::CalcTextSize(subtitle.c_str()).x > room) cyui::tooltip(subtitle.c_str());
    }
    auto end = ImGui::GetCursorScreenPos();
    int clicked = -1;
    if (!list.empty()) {
        const auto block = end.y - start.y - ImGui::GetStyle().ItemSpacing.y;
        const auto top = start.y + std::max(0.0f, (block - ImGui::GetFrameHeight()) * 0.5f);
        ImGui::SetCursorScreenPos(ImVec2(start.x + std::max(0.0f, avail - width), top));
        clicked = actions(list);
        end.y = std::max(end.y, ImGui::GetItemRectMax().y + ImGui::GetStyle().ItemSpacing.y);
    }
    ImGui::SetCursorScreenPos(end);
    ImGui::Dummy(ImVec2(0, dp(6)));
    return clicked;
}

void stat_tiles(const char* id, std::span<const Stat> stats) {
    if (stats.empty()) return;
    const auto& palette = cyui::palette();
    const cyui::ScopedID scope(id);
    const auto spacing = dp(12);
    const auto count = static_cast<float>(stats.size());
    const auto width = (ImGui::GetContentRegionAvail().x - spacing * (count - 1.0f)) / count;
    const auto font = ImGui::GetFontSize();
    const auto value_size = font * 1.75f;
    const auto height = font + value_size + dp(30);
    const auto rounding = dp(cyui::metrics().roundingLarge + 2.0f);
    auto* draw = ImGui::GetWindowDrawList();
    for (std::size_t i = 0; i < stats.size(); ++i) {
        const auto& stat = stats[i];
        if (i) ImGui::SameLine(0, spacing);
        const auto origin = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(width, height));
        const ImVec2 max(origin.x + width, origin.y + height);
        draw->AddRectFilled(origin, max, palette.bg2, rounding);
        draw->AddRect(origin, max, palette.line, rounding);
        const auto pad = dp(16);
        const auto label_y = origin.y + dp(12);
        draw->AddCircleFilled(ImVec2(origin.x + pad + dp(3.5f), label_y + font * 0.5f), dp(3.5f), cyui::toneColour(stat.tone), 12);
        const auto label = cyui::tr(stat.label);
        cyui::clippedText(draw, ImVec2(origin.x + pad + dp(14), label_y), width - pad * 2 - dp(14), palette.textDim, label.c_str());
        const auto value = std::to_string(stat.value);
        draw->AddText(cyui::boldFont(), value_size, ImVec2(origin.x + pad, label_y + font + dp(6)), palette.text, value.c_str());
    }
    cyui::space(6);
}

bool begin_pane(const char* id, ImVec2 size, bool padded, bool fit_height) {
    const auto& palette = cyui::palette();
    cyui::ScopedColor colours(ImGuiCol_ChildBg, palette.bg2);
    colours.push(ImGuiCol_Border, palette.line);
    cyui::ScopedVar vars(ImGuiStyleVar_ChildRounding, dp(cyui::metrics().roundingLarge + 2.0f));
    vars.push(ImGuiStyleVar_WindowPadding, padded ? dp(16, 14) : dp(6, 6));
    return ImGui::BeginChild(id, size, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding |
                                       (fit_height ? ImGuiChildFlags_AutoResizeY : ImGuiChildFlags_None));
}

void end_pane() { ImGui::EndChild(); }

bool begin_rows(const char* id) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, dp(2)));
    const cyui::ScopedColor ground(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
    return ImGui::BeginChild(id, ImVec2(0, 0), ImGuiChildFlags_None);
}

void end_rows() {
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

bool list_row(const char* id, const std::string& title, const std::string& subtitle, ImU32 dot,
              const std::string& pill, bool selected) {
    const auto& palette = cyui::palette();
    const cyui::ScopedID scope(id);
    const auto font = ImGui::GetFontSize();
    const auto origin = ImGui::GetCursorScreenPos();
    const auto width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const auto height = font * 2.0f + dp(20);
    const bool clicked = ImGui::InvisibleButton("##row", ImVec2(width, height), ImGuiButtonFlags_EnableNav);
    const bool hovered = ImGui::IsItemHovered();
    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 max(origin.x + width, origin.y + height);
    const auto rounding = dp(cyui::metrics().rounding + 1.0f);
    if (selected) {
        draw->AddRectFilled(origin, max, cyui::alpha(palette.accent, 0.13f), rounding);
        draw->AddRectFilled(ImVec2(origin.x, origin.y + dp(6)), ImVec2(origin.x + dp(3), max.y - dp(6)), palette.accent, dp(2));
    } else if (hovered) {
        draw->AddRectFilled(origin, max, palette.bg3, rounding);
    }
    const auto x = origin.x + dp(14);
    const auto top = origin.y + dp(9);
    draw->AddCircleFilled(ImVec2(x + dp(3), top + font * 0.5f), dp(3.5f), dot, 12);
    auto right = max.x - dp(10);
    if (!pill.empty()) {
        const cyui::ScopedFont reduced(cyui::boldFont(), 0.8f);
        const auto size = ImGui::CalcTextSize(pill.c_str());
        const ImVec2 pill_max(right, top + font * 0.5f + size.y * 0.5f + dp(2));
        const ImVec2 pill_min(pill_max.x - size.x - dp(12), pill_max.y - size.y - dp(4));
        draw->AddRectFilled(pill_min, pill_max, selected ? cyui::alpha(palette.accent, 0.28f) : palette.bg4, pill_max.y - pill_min.y);
        draw->AddText(ImVec2(pill_min.x + dp(6), pill_min.y + dp(2)), selected ? palette.text : palette.textDim, pill.c_str());
        right = pill_min.x - dp(8);
    }
    const auto text_x = x + dp(16);
    {
        const cyui::ScopedFont emphasis(selected ? cyui::boldFont() : cyui::uiFont(), 1.0f);
        cyui::clippedText(draw, ImVec2(text_x, top), right - text_x, palette.text, title.c_str());
    }
    cyui::clippedText(draw, ImVec2(text_x, top + font + dp(3)), max.x - dp(10) - text_x, palette.textDim, subtitle.c_str());
    if (hovered && ImGui::CalcTextSize(subtitle.c_str()).x > max.x - dp(10) - text_x)
        cyui::tooltip((title + "\n" + subtitle).c_str());
    return clicked;
}

void code_box(const char* id, const std::string& text, float height) {
    auto buffer = text;
    const cyui::ScopedColor well(ImGuiCol_FrameBg, cyui::palette().bg0);
    const cyui::ScopedFont mono(cyui::monoFont());
    ImGui::InputTextMultiline(id, buffer.data(), buffer.size() + 1,
                              ImVec2(-FLT_MIN, height > 0 ? dp(height) : -FLT_MIN), ImGuiInputTextFlags_ReadOnly);
}

bool section_header(const char* title, const char* icon, const char* tooltip) {
    if (!icon) {
        cyui::sectionTitle(title);
        return false;
    }
    ImGui::AlignTextToFramePadding();
    cyui::sectionTitle(title);
    ImGui::SameLine();
    cyui::alignRight(ImGui::GetFrameHeight() + dp(2));
    const cyui::ScopedID scope(title);
    return cyui::iconButton(cyui::iconsAvailable() ? icon : "+", tooltip);
}

void copy_to_clipboard(const std::string& text) {
    ImGui::SetClipboardText(text.c_str());
    cyui::notify(cyui::tr("Copied").c_str(), cyui::Tone::success);
}

void empty_state(const std::string& title, const std::string& message, const char* icon) {
    const auto& palette = cyui::palette();
    const auto area = ImGui::GetContentRegionAvail();
    const auto origin = ImGui::GetCursorScreenPos();
    const auto room = std::max(dp(80), area.x - dp(32));
    std::vector<std::string> lines;
    std::string line;
    std::size_t position = 0;
    while (position < message.size()) {
        const auto space = message.find(' ', position);
        const auto word = message.substr(position, space == std::string::npos ? std::string::npos : space - position);
        const auto candidate = line.empty() ? word : line + ' ' + word;
        if (!line.empty() && ImGui::CalcTextSize(candidate.c_str()).x > room) {
            lines.push_back(line);
            line = word;
        } else {
            line = candidate;
        }
        position = space == std::string::npos ? message.size() : space + 1;
    }
    if (!line.empty()) lines.push_back(line);
    auto* draw = ImGui::GetWindowDrawList();
    const auto middle = origin.y + std::max(dp(60), area.y * 0.42f);
    if (icon && cyui::iconsAvailable()) {
        const cyui::ScopedFont large(cyui::uiFont(), 2.4f);
        cyui::centredText(draw, ImVec2(origin.x, middle - dp(70)), ImVec2(origin.x + area.x, middle - dp(24)), palette.textFaint, icon);
    }
    {
        const cyui::ScopedFont bold(cyui::boldFont(), 1.1f);
        cyui::centredText(draw, ImVec2(origin.x, middle - dp(22)), ImVec2(origin.x + area.x, middle), palette.textDim, title.c_str());
    }
    const auto height = ImGui::GetTextLineHeightWithSpacing();
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const auto top = middle + dp(2) + height * static_cast<float>(i);
        cyui::centredText(draw, ImVec2(origin.x, top), ImVec2(origin.x + area.x, top + height), palette.textFaint, lines[i].c_str());
    }
    ImGui::Dummy(ImVec2(area.x, std::max(0.0f, middle + height * static_cast<float>(lines.size()) + dp(8) - origin.y)));
}

void dot(ImU32 colour, float line_height) {
    const auto height = line_height > 0 ? line_height : ImGui::GetTextLineHeight();
    const auto radius = ImGui::GetFontSize() * 0.2f;
    const auto origin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(radius * 2 + dp(6), height));
    const ImVec2 centre(origin.x + radius * 1.8f, origin.y + height * 0.5f);
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddCircleFilled(centre, radius * 1.8f, cyui::alpha(colour, 0.2f), 16);
    draw->AddCircleFilled(centre, radius, colour, 16);
}

void wrapped(const std::string& text) { ImGui::TextWrapped("%s", text.c_str()); }

void wrapped_dim(const std::string& text) {
    const cyui::ScopedColor dim(ImGuiCol_Text, cyui::palette().textDim);
    ImGui::TextWrapped("%s", text.c_str());
}
} // namespace hub::kit
