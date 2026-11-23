// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "ui.hpp"
#include "updates.hpp"
#include <future>

namespace hub {
class UpdatePanel {
public:
    ~UpdatePanel();
    // Polls background work every frame; draws the dialog while `open` is true.
    void draw(WindowHandle window, bool& open);
    bool available() const { return update_.has_value(); }

private:
    void check();
    cymcp::Cancellation cancellation_;
    std::future<std::optional<AppUpdate>> checking_;
    std::future<fs::path> downloading_;
    std::optional<AppUpdate> update_;
    fs::path downloaded_;
    std::string error_;
    bool checked_ = false, was_open_ = false;
};

void register_update_translations();
} // namespace hub
