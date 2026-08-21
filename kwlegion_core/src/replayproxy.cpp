/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include "replayproxy.h"

#include <replay.h>

#include <algorithm>

#include "teammodel.h"

namespace KWLegionCore {
ReplayProxy::ReplayProxy(const Replay& replay, QObject* parent)
    : QObject(parent),
      m_checksum(replay.checksum),
      m_timestamp(replay.timestamp),
      m_mapName(replay.mapName),
      m_hasExternalPath(replay.hasExternalPath) {
    // Build the teams by scanning for players
    QList<TeamModel*> teams;

    for (const auto& player : replay.players) {
        auto it = std::ranges::find_if(teams, [&player](TeamModel* team) {
            return team->number() == player.teamNumber;
        });
        if (it == teams.end()) {
            // Cleaned up via QObject parent/child deletion and implemented this
            // way for simplicity of integrating with QML
            // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
            teams.append(new TeamModel(player.teamNumber, this));
            it = teams.end() - 1;
        }
        (*it)->addPlayer(player);
    }

    m_teams.reserve(teams.size());
    for (TeamModel* team : teams) {
        m_teams.append(team);
    }
}

void ReplayProxy::updateFromReplay(const Replay& replay) {
    // Most things are immutable but we may in the future change properties
    m_hasExternalPath = replay.hasExternalPath;
}

}  // namespace KWLegionCore