#pragma once

#include "catalog.hpp"

namespace hub {
bool autostart_enabled();
void set_autostart(bool enabled, const Options& options);
} // namespace hub
