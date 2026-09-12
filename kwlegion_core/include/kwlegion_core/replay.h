/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <legionparser/replay.h>

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QtTypes>

namespace LegionParser {
class ReplaySynopsis;
}

namespace KWLegionCore {
using GameType = LegionParser::GameType;
using Faction = LegionParser::Faction;

struct Player {
    quint32 id;
    quint32 teamNumber;
    Faction faction;
    QString name;
    bool isComputer;
};

struct Replay {
    QByteArray checksum;
    QDateTime timestamp;
    QString matchTitle;
    QString matchDescription;
    QString mapName;
    QString mapReference;
    // Does this replay have a version that is stored in the replay directory
    bool hasExternalPath = false;

    QString overrideMatchTitle;
    quint32 engineTicks;
    quint32 bodyOffset;

    QList<Player> players;

    // Static so it remains an aggregate type
    static Replay fromSynopsis(const LegionParser::ReplaySynopsis&,
                               bool hasExternalPath);
};
}  // namespace KWLegionCore

Q_DECLARE_METATYPE(KWLegionCore::Replay);
