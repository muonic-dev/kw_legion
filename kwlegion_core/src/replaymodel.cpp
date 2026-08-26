/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include "replaymodel.h"

#include <replay.h>

#include <QRegularExpression>
#include <algorithm>

#include "storemodel.h"
#include "teammodel.h"

namespace KWLegionCore {
ReplayModel::ReplayModel(const Replay& replay, QObject* parent)
    : QObject(parent),
      m_checksum(replay.checksum),
      m_timestamp(replay.timestamp),
      m_matchTitle(replay.matchTitle),
      m_matchDescription(replay.matchDescription),
      m_mapName(replay.mapName),
      m_mapReference(replay.mapReference),
      m_hasExternalPath(replay.hasExternalPath) {
    // Build the teams by scanning for players
    QList<TeamModel*> teams;

    for (const auto& player : replay.players) {
        // We get empty player names for what is described as the commentary
        // player in replay metadata. We don't want to show this in the ui.
        if (player.name.isEmpty()) {
            continue;
        }
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

QList<int> ReplayModel::updateFromReplay(const Replay& replay) {
    // Most things are immutable but we may in the future change properties
    m_hasExternalPath = replay.hasExternalPath;
    return QList{static_cast<int>(StoreModel::Roles::HasExternalPathRole)};
}

QString ReplayModel::inferPatch() const {
    return ReplayModel::inferPatch(m_mapReference);
}

// QRegularExpression's ctor only stores the pattern (compilation is lazy on
// first match); the only throw path is std::bad_alloc
// NOLINTNEXTLINE(bugprone-throwing-static-initialization)
static const QRegularExpression PATCH_RE(".*__([0-9]+[a-z]+)$");

QString ReplayModel::inferPatch(QStringView mapPath) {
    Q_ASSERT(PATCH_RE.isValid());
    const auto match = PATCH_RE.matchView(mapPath);
    if (!match.hasMatch()) {
        return "";
    }
    return match.captured(1);
}

}  // namespace KWLegionCore