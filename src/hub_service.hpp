// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "catalog.hpp"
#include "journal.hpp"

namespace hub {
Json inventory_metadata(const Snapshot& snapshot, bool scan_ports);
std::shared_ptr<cymcp::Registry> make_hub_registry(Options options, std::shared_ptr<Journal> journal);
} // namespace hub
