/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <kwlegion_core/replay.h>
#include <legionparser/replay.h>

namespace KWLegionCore {
Replay Replay::fromReplay(const LegionParser::ReplayMetadata& replay,
                          bool hasExternalPath) {
    return Replay{.checksum = replay.checksum,
                  .timestamp = replay.timestamp,
                  .mapName = replay.mapName,
                  .hasExternalPath = hasExternalPath};
}
}  // namespace KWLegionCore