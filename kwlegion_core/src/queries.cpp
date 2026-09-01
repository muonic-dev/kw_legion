// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include "queries.h"

#include <kwlegion_core/transaction.h>

#include <QSqlError>
#include <QTimeZone>
#include <QVariantList>

#include "exception.h"

namespace KWLegionCore {

constexpr std::array MIGRATIONS{
    // Store the replay here
    // Note: we use the checksum to compute a stored local state path
    "CREATE TABLE replays"
    "    ( checksum BLOB PRIMARY KEY"
    "    , match_title TEXT NOT NULL"
    "    , match_description TEXT NOT NULL"
    "    , map_name TEXT NOT NULL"
    "    , map_id TEXT NOT NULL"
    "    , game_type INT NOT NULL"
    "    , timestamp INT NOT NULL"
    "    , has_commentary INT NOT NULL"
    "    , filename TEXT NOT NULL"
    "    , map_reference TEXT NOT NULL"
    "    , version_major INT"
    "    , version_minor INT"
    "    , build_major INT"
    "    , build_minor INT"
    "    , canonical_name TEXT"
    "    ) STRICT, WITHOUT ROWID;",

    // Replays might occur more than once in the replay folder for whatever
    // reason so a single storage is not good enough
    // external_path is a full canonical path. A path can only ever point at
    // one file on disk, so it can only ever belong to one checksum at a
    // time
    "CREATE TABLE replay_external_paths"
    "    ( replay_checksum BLOB NOT NULL"
    "    , external_path TEXT NOT NULL"
    "    , PRIMARY KEY(external_path)"
    "    ) STRICT, WITHOUT ROWID;",

    // player_id isn't reliably unique per replay - e.g. the trailing
    // commentator player's id is consistently 0, and skirmish replays leave
    // every player's id at 0 - so rows are identified by rowid instead.
    "CREATE TABLE replay_players"
    "    ( replay_checksum BLOB NOT NULL"
    "    , player_id INT NOT NULL"
    "    , player_name TEXT NOT NULL"
    "    , team_number INT"
    "    , faction INT NOT NULL"
    "    , is_computer INT NOT NULL"
    "    , is_replay_saver INT NOT NULL"
    "    ) STRICT;",

    "CREATE INDEX idx_replay_players_checksum"
    "    ON replay_players(replay_checksum);",

    // In future, add acknowledged_at for tombstoning the UI marker
    // problem maps to ProblemType::TORN ProblemType::CORRUPT
    "CREATE TABLE broken_replays"
    "   ( replay_path TEXT NOT NULL PRIMARY KEY"
    "   , problem INT NOT NULL"
    "   , noticed_at INT NOT NULL"
    "   ) STRICT, WITHOUT ROWID;",
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
    prepare(
        "INSERT INTO replays"
        "    ( checksum"
        "    , match_title"
        "    , match_description"
        "    , map_name"
        "    , map_id"
        "    , game_type"
        "    , timestamp"
        "    , has_commentary"
        "    , filename"
        "    , map_reference"
        "    , version_major"
        "    , version_minor"
        "    , build_major"
        "    , build_minor )"
        " VALUES"
        "    ( :checksum"
        "    , :match_title"
        "    , :match_description"
        "    , :map_name"
        "    , :map_id"
        "    , :game_type"
        "    , :timestamp"
        "    , :has_commentary"
        "    , :filename"
        "    , :map_reference"
        "    , :version_major"
        "    , :version_minor"
        "    , :build_major"
        "    , :build_minor);");
    m_query.bindValue(":checksum", metadata.checksum);
    m_query.bindValue(":match_title", metadata.matchTitle);
    m_query.bindValue(":match_description", metadata.matchDescription);
    m_query.bindValue(":map_name", metadata.mapName);
    m_query.bindValue(":map_id", metadata.mapId);
    m_query.bindValue(":game_type", LegionParser::toUInt8(metadata.gameType));
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
    prepare(
        "INSERT INTO replay_players"
        "    ( replay_checksum"
        "    , player_id"
        "    , player_name"
        "    , team_number"
        "    , faction"
        "    , is_computer"
        "    , is_replay_saver )"
        " VALUES"
        "    ( :replay_checksum"
        "    , :player_id"
        "    , :player_name"
        "    , :team_number"
        "    , :faction"
        "    , :is_computer"
        "    , :is_replay_saver);");
    for (const auto& player : players) {
        m_query.bindValue(":replay_checksum", checksum);
        m_query.bindValue(":player_id", player.id);
        m_query.bindValue(":player_name", player.name);
        m_query.bindValue(":team_number", player.teamNumber);
        m_query.bindValue(":faction", LegionParser::toUInt8(player.faction));
        m_query.bindValue(":is_computer", player.isComputer ? 1 : 0);
        m_query.bindValue(":is_replay_saver", player.isReplaySaver ? 1 : 0);
        exec();
    }
}

std::optional<QByteArray> Queries::checksumForExternalPath(
    const QString& path) {
    prepare(
        "SELECT replay_checksum FROM replay_external_paths "
        "WHERE external_path = :external_path");
    m_query.bindValue(":external_path", path);
    exec();
    std::optional<QByteArray> result;
    if (m_query.next()) {
        result = m_query.value(0).toByteArray();
    }
    // We aren't draining so call finish explicitly
    m_query.finish();
    return result;
}

bool Queries::insertExternalFilename(const QByteArray& checksum,
                                     const QString& path) {
    // external_path is the sole key, so a conflict means either this exact
    // (checksum, path) pair is already tracked (the WHERE guard makes that a
    // no-op) or the path is re-appearing under a new checksum (e.g. the
    // game's rolling "Last Replay.KWReplay" being overwritten with a new
    // match) - in which case the row is reassigned to the new checksum right
    // here, atomically.
    prepare(
        "INSERT INTO replay_external_paths"
        "    ( replay_checksum"
        "    , external_path)"
        " VALUES"
        "    ( :replay_checksum"
        "    , :external_path)"
        " ON CONFLICT(external_path) DO UPDATE"
        "    SET replay_checksum = excluded.replay_checksum"
        "    WHERE replay_checksum != excluded.replay_checksum;");
    m_query.bindValue(":replay_checksum", checksum);
    m_query.bindValue(":external_path", path);
    exec();
    return m_query.numRowsAffected() > 0;
}

std::optional<QByteArray> Queries::removeExternalFilename(const QString& path) {
    // external_path is unique, so this affects at most one row.
    prepare(
        "DELETE FROM replay_external_paths "
        "WHERE external_path = :external_path "
        "RETURNING replay_checksum;");
    m_query.bindValue(":external_path", path);
    exec();

    std::optional<QByteArray> result;
    if (m_query.next()) {
        result = m_query.value(0).toByteArray();
    }
    // We aren't draining so finish explicitly
    m_query.finish();
    return result;
}

void Queries::forgetMissingReplays(const QList<QString>& knownPaths) {
    // Binding one placeholder per path (the previous "NOT IN (:p0, :p1, ...)"
    // approach) hits SQLite's per-statement bound-parameter limit once a
    // replay folder has accumulated enough files over time
    prepare(
        "CREATE TEMP TABLE IF NOT EXISTS known_replay_paths "
        "(path TEXT NOT NULL)");
    exec();

    prepare("DELETE FROM known_replay_paths");
    exec();

    if (!knownPaths.isEmpty()) {
        prepare("INSERT INTO known_replay_paths (path) VALUES (:path)");

        for (const auto& path : knownPaths) {
            m_query.bindValue(":path", path);
            exec();
        }
    }

    // An empty knownPaths leaves known_replay_paths empty too, so the
    // subquery matches nothing and every row still gets deleted - the same
    // "nothing is known to exist" behavior as before.
    prepare(
        "DELETE FROM replay_external_paths "
        "WHERE external_path NOT IN (SELECT path FROM known_replay_paths)");
    exec();
}

QList<Replay> Queries::selectReplays() {
    prepare(
        "SELECT checksum"
        "    , timestamp"
        "    , match_title"
        "    , match_description"
        "    , map_name"
        "    , map_reference"
        "    , EXISTS ("
        "        SELECT 1 FROM replay_external_paths"
        "        WHERE replay_checksum = replays.checksum"
        "      ) AS has_external_path"
        " FROM replays");
    exec();

    QList<Replay> replays;
    while (m_query.next()) {
        replays.append(readReplay());
    }

    throwLastIfFailed();

    return replays;
}

std::optional<Replay> Queries::selectReplay(const QByteArray& checksum) {
    prepare(
        "SELECT checksum"
        "    , timestamp"
        "    , match_title"
        "    , match_description"
        "    , map_name"
        "    , map_reference"
        "    , EXISTS ("
        "        SELECT 1 FROM replay_external_paths"
        "        WHERE replay_checksum = replays.checksum"
        "      ) AS has_external_path"
        " FROM replays"
        " WHERE checksum = :checksum");
    m_query.bindValue(":checksum", checksum);

    exec();

    if (m_query.next()) {
        return readReplay();
    }

    // Check for termination due to error
    throwLastIfFailed();

    return std::nullopt;
}

QList<Player> Queries::selectReplayPlayers(const QByteArray& checksum) {
    prepare(
        "SELECT player_id"
        "    , player_name"
        "    , team_number"
        "    , faction"
        "    , is_computer"
        "    , is_replay_saver"
        " FROM replay_players"
        " WHERE replay_checksum = :checksum");
    m_query.bindValue(":checksum", checksum);

    exec();

    QList<Player> players;
    while (m_query.next()) {
        Player player;
        player.id = m_query.value(0).toInt();
        player.name = m_query.value(1).toString();
        player.teamNumber = m_query.value(2).toInt();
        player.faction =
            LegionParser::factionFromUInt8(m_query.value(3).toUInt());
        player.isComputer = m_query.value(4).toBool();
        players.append(player);
    }

    throwLastIfFailed();

    return players;
}

QList<QString> Queries::selectExternalPaths(const QByteArray& checksum) {
    prepare(
        "SELECT external_path FROM replay_external_paths WHERE replay_checksum "
        "= :checksum");
    m_query.bindValue(":checksum", checksum);

    exec();

    QList<QString> paths;
    while (m_query.next()) {
        paths.append(m_query.value(0).toString());
    }
    throwLastIfFailed();
    return paths;
}

ProblemRecord Queries::insertPathProblem(const ProblemRecord& problem) {
    prepare(
        "INSERT INTO broken_replays(replay_path, problem, noticed_at) "
        "VALUES (:replay_path, :problem, :noticed_at) "
        " ON CONFLICT DO UPDATE "
        "   SET problem = excluded.problem "
        " RETURNING "
        "   replay_path, problem, noticed_at");
    m_query.bindValue(":replay_path", problem.path);
    m_query.bindValue(":problem", problemToUInt8(problem.type));
    m_query.bindValue(":noticed_at", problem.noticedAt.toSecsSinceEpoch());

    exec();

    nextOrThrow();

    return ProblemRecord{.path = m_query.value(0).toString(),
                         .noticedAt = QDateTime::fromSecsSinceEpoch(
                             m_query.value(1).toInt(), QTimeZone::UTC),
                         .type = problemFromUInt8(
                             static_cast<uint8_t>(m_query.value(2).toUInt()))};
}

void Queries::clearPathProblems(const QString& path) {
    prepare("DELETE FROM broken_replays WHERE replay_path = :replay_path");
    m_query.bindValue(":replay_path", path);

    exec();
}

Replay Queries::readReplay() const {
    return Replay{.checksum = m_query.value(0).toByteArray(),
                  .timestamp = QDateTime::fromSecsSinceEpoch(
                      m_query.value(1).toLongLong(), QTimeZone(QTimeZone::UTC)),
                  .matchTitle = m_query.value(2).toString(),
                  .matchDescription = m_query.value(3).toString(),
                  .mapName = m_query.value(4).toString(),
                  .mapReference = m_query.value(5).toString(),
                  .hasExternalPath = m_query.value(6).toBool()};
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

void Queries::nextOrThrow() {
    if (!m_query.next()) {
        throw StorageException("expected result row");
    }
}

}  // namespace KWLegionCore
