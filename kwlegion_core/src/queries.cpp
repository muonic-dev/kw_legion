// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include "queries.h"

#include <kwlegion_core/transaction.h>

#include <QSqlError>
#include <QStringList>
#include <QTimeZone>

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

bool Queries::migrate() {
    m_query.exec("PRAGMA user_version");
    m_query.next();
    const size_t currentVersion = m_query.value(0).toULongLong();

    qDebug() << "Current schema version is: " << currentVersion;

    // The behavior of PRAGMA user_version starts at 0 so this is always the
    // next thing to execute
    for (size_t nextExec = currentVersion; nextExec < MIGRATIONS.size();
         nextExec++) {
        if (!m_query.exec(MIGRATIONS.at(nextExec))) {
            return false;
        }
        if (!m_query.exec(QStringLiteral("PRAGMA user_version = %1;")
                              .arg(nextExec + 1))) {
            return false;
        }
    }

    return true;
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

bool Queries::removeStaleExternalFilename(const QByteArray& checksum,
                                          const QString& path) {
    // The same external path (e.g. the game's rolling "Last Replay.KWReplay")
    // can be re-written with different content over time, so it may still be
    // registered under a checksum from a previous match. Once we know the
    // current checksum for that path, any other checksum still claiming it
    // is stale and should be dropped.
    prepare(R"(DELETE FROM replay_external_paths
            WHERE external_path = :external_path
              AND replay_checksum != :replay_checksum;)");
    m_query.bindValue(":external_path", path);
    m_query.bindValue(":replay_checksum", checksum);
    exec();
    return m_query.numRowsAffected() > 0;
}

void Queries::forgetMissingReplays(const QList<QString>& knownPaths) {
    // NOT IN needs one bound placeholder per path - QSqlQuery has no way to
    // bind a whole list to a single placeholder. An empty knownPaths yields
    // "NOT IN ()", which SQLite evaluates as true for every row, so this
    // still does the right thing when nothing is known to exist.
    QStringList placeholders;
    placeholders.reserve(knownPaths.size());
    for (qsizetype i = 0; i < knownPaths.size(); i++) {
        placeholders.append(QStringLiteral(":p%1").arg(i));
    }

    prepare(QStringLiteral("DELETE FROM replay_external_paths "
                           "WHERE external_path NOT IN (%1)")
                .arg(placeholders.join(QStringLiteral(", "))));

    for (qsizetype i = 0; i < knownPaths.size(); i++) {
        m_query.bindValue(QStringLiteral(":p%1").arg(i), knownPaths.at(i));
    }

    exec();
}

QList<Replay> Queries::selectReplays() {
    prepare(R"(SELECT checksum
                    , timestamp
                    , map_name
                    , EXISTS (
                        SELECT 1 FROM replay_external_paths
                        WHERE replay_checksum = replays.checksum
                      ) AS has_external_path
                    
               FROM replays)");
    exec();

    QList<Replay> replays;
    while (m_query.next()) {
        replays.append(readReplay());
    }

    throwLastIfFailed();

    return replays;
}

std::optional<Replay> Queries::selectReplay(const QByteArray& checksum) {
    prepare(R"(SELECT checksum
                    , timestamp
                    , map_name
                    , EXISTS (
                        SELECT 1 FROM replay_external_paths
                        WHERE replay_checksum = replays.checksum
                      ) AS has_external_path
                    
               FROM replays
               WHERE checksum = :checksum)");
    m_query.bindValue(":checksum", checksum);

    exec();

    if (m_query.next()) {
        return readReplay();
    }

    // Check for termination due to error
    throwLastIfFailed();

    return std::nullopt;
}

Replay Queries::readReplay() const {
    return Replay{.checksum = m_query.value(0).toByteArray(),
                  .timestamp = QDateTime::fromSecsSinceEpoch(
                      m_query.value(1).toLongLong(), QTimeZone(QTimeZone::UTC)),
                  .mapName = m_query.value(2).toString(),
                  .hasExternalPath = m_query.value(3).toBool()};
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
    throw StorageException(m_query.lastError().text());
}

void Queries::throwLastIfFailed() const {
    if (m_query.lastError().isValid()) {
        throw StorageException(m_query.lastError().text());
    }
}

}  // namespace KWLegionCore
