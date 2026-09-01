/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

namespace KWLegionCore {

// Registers the Qt meta-types needed for queued delivery of signals crossing
// a thread boundary (e.g. ReplayStore living on the io thread). Must be
// called once at startup before any such signal can fire. Kept as its own
// translation unit so callers (main.cpp) don't need to know or include every
// type that ends up needing registration here, including ones that are
// otherwise private to kwlegion_core.
void registerMetaTypes();

}  // namespace KWLegionCore
