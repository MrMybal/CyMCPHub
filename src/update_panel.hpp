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
    void draw(WindowHandle window);

private:
    cymcp::Cancellation cancellation_;
    std::future<std::optional<AppUpdate>> checking_;
    std::future<fs::path> downloading_;
    std::optional<AppUpdate> update_;
    fs::path downloaded_;
    std::string error_;
    bool checked_ = false;
};
} // namespace hub
