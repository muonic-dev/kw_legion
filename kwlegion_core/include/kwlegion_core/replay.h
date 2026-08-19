/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <legionparser/replay.h>

#include <QByteArray>
#include <QDateTime>
#include <QMetaType>
#include <QString>

namespace LegionParser {
class ReplayMetadata;
}

namespace KWLegionCore {
using GameType = LegionParser::GameType;
using Faction = LegionParser::Faction;

struct Player {
    std::uint32_t id;
    std::uint32_t teamNumber;
    Faction faction;
    QString name;
    bool isComputer;
};

struct Replay {
    QByteArray checksum;
    QDateTime timestamp;
    QString mapName;
    // Does this replay have a version that is stored in the replay directory
    bool hasExternalPath = false;

    QList<Player> players;

    // Static so it remains an aggregate type
    static Replay fromReplay(const LegionParser::ReplayMetadata&,
                             bool hasExternalPath);
};
}  // namespace KWLegionCore

Q_DECLARE_METATYPE(KWLegionCore::Replay);
