// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "catalog.hpp"

namespace hub {
bool autostart_enabled();
void set_autostart(bool enabled, const Options& options);
} // namespace hub
