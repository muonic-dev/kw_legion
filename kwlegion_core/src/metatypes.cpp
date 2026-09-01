/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <kwlegion_core/metatypes.h>
#include <kwlegion_core/replay.h>

namespace KWLegionCore {

void registerMetaTypes() { qRegisterMetaType<Replay>(); }

}  // namespace KWLegionCore
