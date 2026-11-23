// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

// App-level presentation helpers built on CyUI. They only compose CyUI widgets
// and palette roles; nothing here changes CyUI itself.

#include <cyui/Core.h>

#include <span>
#include <string>

namespace hub::kit {

// "icon  label" when the icon font is loaded, the label alone otherwise.
std::string with_icon(const char* icon, const std::string& label);

struct Action {
    const char* icon = nullptr;
    std::string label;
    cyui::Variant variant = cyui::Variant::secondary;
    bool enabled = true;
    std::string tooltip;
};

float actions_width(std::span<const Action> actions);
// Draws the buttons on the current line. Returns the index clicked, or -1.
int actions(std::span<const Action> actions);

// Title, faint subtitle and right-aligned actions. Returns the action clicked, or -1.
int page_header(const char* title, const std::string& subtitle, std::span<const Action> actions = {});

struct Stat {
    const char* label;
    std::size_t value;
    cyui::Tone tone;
};
void stat_tiles(const char* id, std::span<const Stat> stats);

// A bordered, rounded pane on the cards ground. Always pair with end_pane().
// With fit_height, the pane grows to its content instead of filling the region.
bool begin_pane(const char* id, ImVec2 size, bool padded = true, bool fit_height = false);
void end_pane();

// A two-line selectable row: status dot, title, subtitle and an optional count pill.
bool list_row(const char* id, const std::string& title, const std::string& subtitle, ImU32 dot,
              const std::string& pill, bool selected);
// A scrolling list of rows filling the rest of the pane.
bool begin_rows(const char* id);
void end_rows();

// A read-only monospace well.
void code_box(const char* id, const std::string& text, float height);

// A small section title with an optional icon button aligned right. Returns true when it was clicked.
bool section_header(const char* title, const char* icon = nullptr, const char* tooltip = nullptr);

// Copies text to the clipboard and confirms with a toast.
void copy_to_clipboard(const std::string& text);

// CyUI's empty state, with the message wrapped to the pane width.
void empty_state(const std::string& title, const std::string& message = {}, const char* icon = nullptr);

// A status dot centred on a line of the given height (text line height when 0).
void dot(ImU32 colour, float line_height = 0);

void wrapped(const std::string& text);
void wrapped_dim(const std::string& text);
} // namespace hub::kit
