// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include "queries.h"

#include <kwlegion_core/transaction.h>

#include <QSqlError>

#include "exception.h"

namespace KWLegionCore {

constexpr std::array MIGRATIONS{
    // Store the replay here
    // Note: we use the checksum to compute a stored local state path
    R"(CREATE TABLE replays
        ( checksum BLOB PRIMARY KEY
        , match_title TEXT NOT NULL
        , match_description TEXT NOT NULL
        , map_name TEXT NOT NULL
        , map_id TEXT NOT NULL
        , game_type INT NOT NULL
        , timestamp INT NOT NULL
        , has_commentary INT NOT NULL
        , filename TEXT NOT NULL
        , map_reference TEXT NOT NULL
        , version_major INT
        , version_minor INT
        , build_major INT
        , build_minor INT
        , canonical_name TEXT
        ) STRICT, WITHOUT ROWID;
    )",

    // Replays might occur more than once in the replay folder for whatever
    // reason so a single storage is not good enough
    // external_path is a full canonical path
    R"(CREATE TABLE replay_external_paths
        ( replay_checksum BLOB NOT NULL
        , external_path TEXT NOT NULL
        , PRIMARY KEY(replay_checksum, external_path)
        ) STRICT, WITHOUT ROWID;
    )",

    // player_id isn't reliably unique per replay - e.g. the trailing
    // commentator player's id is consistently 0, and skirmish replays leave
    // every player's id at 0 - so rows are identified by rowid instead.
    R"(CREATE TABLE replay_players
        ( replay_checksum BLOB NOT NULL
        , player_id INT NOT NULL
        , player_name TEXT NOT NULL
        , team_number INT
        , faction INT NOT NULL
        , is_computer INT NOT NULL
        , is_replay_saver INT NOT NULL
        ) STRICT;
    )",

    R"(CREATE INDEX idx_replay_players_checksum
        ON replay_players(replay_checksum);
    )",
};

void Queries::migrate(const QSqlDatabase& db) {
    QSqlQuery query(db);
    query.exec("PRAGMA user_version");
    query.next();
    const size_t currentVersion = query.value(0).toULongLong();

    qDebug() << "Current schema version is: " << currentVersion;

    // The behavior of PRAGMA user_version starts at 0 so this is always the
    // next thing to execute
    for (size_t nextExec = currentVersion; nextExec < MIGRATIONS.size();
         nextExec++) {
        SqlTransactionGuard tx(db);
        if (!query.exec(MIGRATIONS.at(nextExec))) {
            qCritical() << "Failed to execute migration: " << nextExec << ": "
                        << query.lastError().text();
            return;
        }
        if (!query.exec(QStringLiteral("PRAGMA user_version = %1;")
                            .arg(nextExec + 1))) {
            qCritical() << "Failed to execute migration: " << nextExec << ": "
                        << query.lastError().text();
            return;
        }
        if (!tx.commit()) {
            qCritical() << "Failed to execute migration: " << nextExec << ": "
                        << query.lastError().text();
            return;
        }
    }

    qDebug() << "Migrations successful";
}

bool Queries::isReplayKnown(const QByteArray& checksum) {
    prepare("SELECT count(*) FROM replays WHERE checksum = :checksum");
    m_query.bindValue(":checksum", checksum);
    exec();
    m_query.next();  // it's a count, there must be 1 row
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
        , faction
        , is_computer
        , is_replay_saver )
        VALUES
        ( :replay_checksum
        , :player_id
        , :player_name
        , :team_number
        , :faction
        , :is_computer
        , :is_replay_saver);)");
    for (const auto& player : players) {
        m_query.bindValue(":replay_checksum", checksum);
        m_query.bindValue(":player_id", player.playerId);
        m_query.bindValue(":player_name", player.playerName);
        m_query.bindValue(":team_number", player.teamNumber);
        m_query.bindValue(":faction", LegionParser::toUint8(player.faction));
        m_query.bindValue(":is_computer", player.isComputer ? 1 : 0);
        m_query.bindValue(":is_replay_saver", player.isReplaySaver ? 1 : 0);
        exec();
    }
}

bool Queries::insertExternalFilename(const QByteArray& checksum,
                                     const QString& path) {
    // ON CONFLICT so that this is useable for both brand new and existing
    // replay
    prepare(R"(INSERT INTO replay_external_paths
            ( replay_checksum
            , external_path)
            VALUES
            ( :replay_checksum
            , :external_path)
            ON CONFLICT DO NOTHING;)");
    m_query.bindValue(":replay_checksum", checksum);
    m_query.bindValue(":external_path", path);
    exec();
    return m_query.numRowsAffected() > 0;
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
