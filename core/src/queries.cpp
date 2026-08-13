// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include "queries.h"

#include <QSqlError>
#include <stdexcept>

namespace KWLegionCore {

namespace {
// Local class that we can throw when something fails to unwind
// This should never propogate outside this TU. The SqlTransactionGuard should
// correctly roll back.
class IngestionException : public std::runtime_error {
   public:
    IngestionException(const QString& what)
        : std::runtime_error(what.toStdString()) {}
};
}  // namespace

bool Queries::isReplayKnown(const QByteArray& checksum) {
    prepare("SELECT count(*) FROM replays WHERE checksum = :checksum");
    m_query.bindValue(":checksum", checksum);
    exec();

    return m_query.value(0).toInt() != 0;
}

void Queries::insertReplay(const LegionParser::ReplayMetadata& metadata) {
    prepare(R"(
        INSERT INTO replays
            ( checksum
            , match_title
            , match_description
            , map_name
            , map_id
            , game_type
            , timestamp
            , has_commentary
            , filename
            , map_reference
            , version_major
            , version_minor
            , build_major
            , build_minor )
        VALUES
            ( :checksum
            , :match_title
            , :match_description
            , :map_name
            , :map_id
            , :game_type
            , :timestamp
            , :has_commentary
            , :filename
            , :map_reference
            , :version_major
            , :version_minor
            , :build_major
            , :build_minor);)");
    m_query.bindValue(":checksum", metadata.checksum);
    m_query.bindValue(":match_title", metadata.matchTitle);
    m_query.bindValue(":match_description", metadata.matchDescription);
    m_query.bindValue(":map_name", metadata.mapName);
    m_query.bindValue(":map_id", metadata.mapId);
    m_query.bindValue(":game_type", LegionParser::toUint8(metadata.gameType));
    m_query.bindValue(":timestamp", metadata.timestamp.toSecsSinceEpoch());
    m_query.bindValue(":has_commentary", metadata.hasCommentary);
    m_query.bindValue(":filename", metadata.filename);
    m_query.bindValue(":map_reference", metadata.mapReference);
    m_query.bindValue(":version_major", metadata.versionMajor);
    m_query.bindValue(":version_minor", metadata.versionMinor);
    m_query.bindValue(":build_major", metadata.buildMajor);
    m_query.bindValue(":build_minor", metadata.buildMinor);
    exec();
}

void Queries::insertReplayPlayers(const QByteArray& checksum,
                                  const QList<LegionParser::Player>& players) {
    prepare(R"(INSERT INTO replay_players
        ( replay_checksum
        , player_id
        , player_name
        , team_number
        , is_computer
        , is_replay_saver )
        VALUES
        ( :replay_checksum
        , :player_id
        , :player_name
        , :team_number
        , :is_computer
        , :is_replay_saver);)");
    for (const auto& player : players) {
        m_query.bindValue(":replay_checksum", checksum);
        m_query.bindValue(":player_id", player.playerId);
        m_query.bindValue(":player_name", player.playerName);
        m_query.bindValue(":team_number", player.teamNumber);
        m_query.bindValue(":is_computer", player.isComputer ? 1 : 0);
        m_query.bindValue(":is_replay_saver", player.isReplaySaver ? 1 : 0);
    }
}

void Queries::prepare(const QString& sql) {
    if (!m_query.prepare(sql)) {
        throwLast();
    }
}

void Queries::exec() {
    if (!m_query.exec()) {
        throwLast();
    }
}

void Queries::execBatch() {
    if (!m_query.execBatch()) {
        throwLast();
    }
}

void Queries::throwLast() const {
    throw IngestionException(m_query.lastError().text());
}

}  // namespace KWLegionCore
