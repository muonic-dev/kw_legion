/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <kwlegion_core/replay.h>
#include <legionparser/replay.h>

#include <QList>

namespace KWLegionCore {
Replay Replay::fromSynopsis(const LegionParser::ReplaySynopsis& replay,
                          bool hasExternalPath) {
    QList<Player> players;
    players.reserve(replay.players.size());
    for (const auto& player : replay.players) {
        players.append(Player{.id = player.id,
                              .teamNumber = player.teamNumber,
                              .faction = player.faction,
                              .name = player.name,
                              .isComputer = player.isComputer});
    }
    return Replay{.checksum = replay.checksum,
                  .timestamp = replay.timestamp,
                  .matchTitle = replay.matchTitle,
                  .matchDescription = replay.matchDescription,
                  .mapName = replay.mapName,
                  .mapReference = replay.mapReference,
                  .hasExternalPath = hasExternalPath};
}
}  // namespace KWLegionCore