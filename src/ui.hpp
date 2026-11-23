// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "catalog.hpp"
#ifdef _WIN32
#include <windows.h>
#else
struct SDL_Window;
#endif

namespace hub {
inline bool tray_assistant_requested = false;
inline bool tray_login_changed = false;
#ifdef _WIN32
using WindowHandle = HWND;
#else
using WindowHandle = SDL_Window*;
#endif
using Draw = std::function<void()>;
using DrawFrame = std::function<bool(const Draw& tick, const Draw& main, const Draw& assistant, bool&, bool&)>;
void draw_hub(WindowHandle window, Options options, const DrawFrame& frame);
} // namespace hub
