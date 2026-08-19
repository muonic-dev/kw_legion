// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include <kwlegion_core/replay.h>
#include <legionparser/replay.h>

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTimeZone>
#include <catch2/catch_test_macros.hpp>

#include "exception.h"
#include "queries.h"

using namespace KWLegionCore;

namespace {

// Opens a fresh, isolated in-memory SQLite database under connectionName and
// runs migrate() on it. Each TEST_CASE below uses its own connection name so
// databases never leak between tests.
QSqlDatabase openMigratedDb(const QString& connectionName) {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(":memory:");
    REQUIRE(db.open());

    Queries migrator{QSqlQuery(db)};
    REQUIRE(migrator.migrate());

    return db;
}

// The checksum only needs to be unique per test, not a real SHA-256 - a
// short literal tag is enough to identify a row.
LegionParser::ReplayMetadata makeMetadata(const QByteArray& checksum,
                                          const QString& mapName = "Test Map") {
    return LegionParser::ReplayMetadata{
        .versionMajor = 1,
        .versionMinor = 0,
        .buildMajor = 1,
        .buildMinor = 0,
        .gameType = LegionParser::GameType::Multiplayer,
        .hasCommentary = false,
        .matchTitle = "Test Match",
        .matchDescription = "Test Description",
        .mapName = mapName,
        .mapId = "test-map-id",
        .players = {},
        // Real timestamps are always UTC (see legionparser's
        // parseHeaderTail()), so the fixture matches that shape.
        .timestamp = QDateTime::fromSecsSinceEpoch(1700000000,
                                                   QTimeZone(QTimeZone::UTC)),
        .filename = "Test Replay",
        .mapReference = "data/maps/official/test",
        .checksum = checksum,
    };
}

int countPlayers(QSqlDatabase& db, const QByteArray& checksum) {
    QSqlQuery query(db);
    REQUIRE(query.prepare(
        "SELECT COUNT(*) FROM replay_players WHERE replay_checksum = "
        ":checksum"));
    query.bindValue(":checksum", checksum);
    REQUIRE(query.exec());
    REQUIRE(query.next());
    return query.value(0).toInt();
}

int countExternalPaths(QSqlDatabase& db, const QByteArray& checksum) {
    QSqlQuery query(db);
    REQUIRE(query.prepare(
        "SELECT COUNT(*) FROM replay_external_paths WHERE replay_checksum = "
        ":checksum"));
    query.bindValue(":checksum", checksum);
    REQUIRE(query.exec());
    REQUIRE(query.next());
    return query.value(0).toInt();
}

}  // namespace

TEST_CASE("Queries migrate creates the schema and is idempotent") {
    QSqlDatabase db = openMigratedDb("queries_migrate_idempotent");
    Queries queries{QSqlQuery(db)};

    // Calling migrate() again once the schema is already current should be
    // a harmless no-op rather than trying to re-run already-applied DDL.
    CHECK(queries.migrate());

    QSqlQuery check(db);
    REQUIRE(check.exec(
        "SELECT name FROM sqlite_master WHERE type = 'table' AND name IN "
        "('replays', 'replay_external_paths', 'replay_players')"));
    int tableCount = 0;
    while (check.next()) {
        tableCount++;
    }
    CHECK(tableCount == 3);

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_migrate_idempotent");
}

TEST_CASE("Queries isReplayKnown reflects insertReplay") {
    QSqlDatabase db = openMigratedDb("queries_is_known");
    Queries queries{QSqlQuery(db)};

    const QByteArray checksum = "checksum-known";
    CHECK_FALSE(queries.isReplayKnown(checksum));

    queries.insertReplay(makeMetadata(checksum));

    CHECK(queries.isReplayKnown(checksum));
    CHECK_FALSE(queries.isReplayKnown("checksum-different"));

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_is_known");
}

TEST_CASE("Queries selectReplay returns the stored fields") {
    QSqlDatabase db = openMigratedDb("queries_select_replay");
    Queries queries{QSqlQuery(db)};

    const QByteArray checksum = "checksum-select";
    const QDateTime timestamp =
        QDateTime::fromSecsSinceEpoch(1700000000, QTimeZone(QTimeZone::UTC));
    LegionParser::ReplayMetadata metadata =
        makeMetadata(checksum, "Sample Map");
    metadata.timestamp = timestamp;
    queries.insertReplay(metadata);

    const std::optional<Replay> replay = queries.selectReplay(checksum);
    REQUIRE(replay.has_value());
    CHECK(replay->checksum == checksum);
    CHECK(replay->mapName == "Sample Map");
    // Timestamps are stored/read as UTC end to end (see legionparser's
    // parseHeaderTail()), so this should be exact equality - not just the
    // same instant under different time specs.
    CHECK(replay->timestamp == timestamp);
    CHECK(replay->timestamp.timeSpec() == Qt::UTC);
    CHECK_FALSE(replay->hasExternalPath);

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_select_replay");
}

TEST_CASE("Queries selectReplay returns nullopt for an unknown checksum") {
    QSqlDatabase db = openMigratedDb("queries_select_replay_missing");
    Queries queries{QSqlQuery(db)};

    CHECK_FALSE(queries.selectReplay("does-not-exist").has_value());

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_select_replay_missing");
}

TEST_CASE("Queries insertReplay throws on a duplicate checksum") {
    QSqlDatabase db = openMigratedDb("queries_insert_duplicate");
    Queries queries{QSqlQuery(db)};

    const QByteArray checksum = "checksum-dup";
    queries.insertReplay(makeMetadata(checksum));

    CHECK_THROWS_AS(queries.insertReplay(makeMetadata(checksum)),
                    StorageException);

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_insert_duplicate");
}

TEST_CASE("Queries insertReplayPlayers stores every player for a replay") {
    QSqlDatabase db = openMigratedDb("queries_insert_players");
    Queries queries{QSqlQuery(db)};

    const QByteArray checksum = "checksum-players";
    queries.insertReplay(makeMetadata(checksum));

    const QList<LegionParser::Player> players{
        LegionParser::Player{.playerId = 1,
                             .playerName = "Alice",
                             .teamNumber = 0,
                             .faction = LegionParser::Faction::GDI,
                             .isComputer = false,
                             .isReplaySaver = true},
        LegionParser::Player{.playerId = 2,
                             .playerName = "Bob",
                             .teamNumber = 1,
                             .faction = LegionParser::Faction::Nod,
                             .isComputer = true,
                             .isReplaySaver = false},
    };

    queries.insertReplayPlayers(checksum, players);

    CHECK(countPlayers(db, checksum) == 2);

    QSqlQuery check(db);
    REQUIRE(check.prepare(
        "SELECT player_name, is_computer, is_replay_saver FROM "
        "replay_players WHERE replay_checksum = :checksum AND player_id = "
        ":id"));
    check.bindValue(":checksum", checksum);
    check.bindValue(":id", 2);
    REQUIRE(check.exec());
    REQUIRE(check.next());
    CHECK(check.value(0).toString() == "Bob");
    CHECK(check.value(1).toInt() == 1);
    CHECK(check.value(2).toInt() == 0);

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_insert_players");
}

TEST_CASE("Queries insertReplayPlayers with an empty list inserts nothing") {
    QSqlDatabase db = openMigratedDb("queries_insert_players_empty");
    Queries queries{QSqlQuery(db)};

    const QByteArray checksum = "checksum-no-players";
    queries.insertReplay(makeMetadata(checksum));

    queries.insertReplayPlayers(checksum, {});

    CHECK(countPlayers(db, checksum) == 0);

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_insert_players_empty");
}

TEST_CASE("Queries insertExternalFilename reports whether the path is new") {
    QSqlDatabase db = openMigratedDb("queries_insert_external");
    Queries queries{QSqlQuery(db)};

    const QByteArray checksum = "checksum-external";
    queries.insertReplay(makeMetadata(checksum));

    CHECK(queries.insertExternalFilename(checksum, "C:/replays/a.KWReplay"));
    CHECK_FALSE(
        queries.insertExternalFilename(checksum, "C:/replays/a.KWReplay"));
    CHECK(queries.insertExternalFilename(checksum, "C:/replays/b.KWReplay"));

    CHECK(countExternalPaths(db, checksum) == 2);

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_insert_external");
}

TEST_CASE(
    "Queries removeStaleExternalFilename drops the path's registration "
    "under other checksums") {
    QSqlDatabase db = openMigratedDb("queries_remove_stale_external");
    Queries queries{QSqlQuery(db)};

    const QByteArray oldChecksum = "checksum-old";
    const QByteArray newChecksum = "checksum-new";
    const QString path = "C:/replays/Last Replay.KWReplay";

    queries.insertReplay(makeMetadata(oldChecksum));
    queries.insertReplay(makeMetadata(newChecksum));
    queries.insertExternalFilename(oldChecksum, path);

    // The path is only registered under the old checksum, so re-pointing it
    // at the new checksum should find and remove a stale row.
    CHECK(queries.removeStaleExternalFilename(newChecksum, path));
    CHECK(countExternalPaths(db, oldChecksum) == 0);

    queries.insertExternalFilename(newChecksum, path);

    // Now nothing else claims the path, so there is no stale row to remove.
    CHECK_FALSE(queries.removeStaleExternalFilename(newChecksum, path));
    CHECK(countExternalPaths(db, newChecksum) == 1);

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_remove_stale_external");
}

TEST_CASE("Queries removeStaleExternalFilename leaves other paths untouched") {
    QSqlDatabase db = openMigratedDb("queries_remove_stale_external_other");
    Queries queries{QSqlQuery(db)};

    const QByteArray checksum = "checksum-unrelated";
    queries.insertReplay(makeMetadata(checksum));
    queries.insertExternalFilename(checksum, "C:/replays/other.KWReplay");

    CHECK_FALSE(queries.removeStaleExternalFilename(
        checksum, "C:/replays/Last Replay.KWReplay"));
    CHECK(countExternalPaths(db, checksum) == 1);

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_remove_stale_external_other");
}

TEST_CASE(
    "Queries forgetMissingReplays drops paths absent from the current "
    "listing") {
    QSqlDatabase db = openMigratedDb("queries_forget_missing");
    Queries queries{QSqlQuery(db)};

    const QByteArray checksum = "checksum-forget";
    queries.insertReplay(makeMetadata(checksum));
    queries.insertExternalFilename(checksum, "C:/replays/a.KWReplay");
    queries.insertExternalFilename(checksum, "C:/replays/b.KWReplay");

    queries.forgetMissingReplays({"C:/replays/a.KWReplay"});

    CHECK(countExternalPaths(db, checksum) == 1);

    // An empty "currently known" list means nothing on disk is known any
    // more, so every remaining path should be dropped too - the replay row
    // itself is untouched, only its external-path registrations are.
    queries.forgetMissingReplays({});

    CHECK(countExternalPaths(db, checksum) == 0);

    const std::optional<Replay> replay = queries.selectReplay(checksum);
    REQUIRE(replay.has_value());
    CHECK_FALSE(replay->hasExternalPath);

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_forget_missing");
}

TEST_CASE(
    "Queries forgetMissingReplays handles more paths than SQLite's bound "
    "parameter limit") {
    QSqlDatabase db = openMigratedDb("queries_forget_missing_many");
    Queries queries{QSqlQuery(db)};

    const QByteArray checksum = "checksum-many-paths";
    queries.insertReplay(makeMetadata(checksum));

    // A large replay folder may accumulate m any files
    constexpr int totalPaths = 35000;
    constexpr int keptPaths = 33000;
    QList<QString> knownPaths;
    knownPaths.reserve(keptPaths);
    for (int i = 0; i < totalPaths; i++) {
        const QString path =
            QStringLiteral("C:/replays/replay-%1.KWReplay").arg(i);
        queries.insertExternalFilename(checksum, path);
        if (i < keptPaths) {
            knownPaths.append(path);
        }
    }
    REQUIRE(countExternalPaths(db, checksum) == totalPaths);

    queries.forgetMissingReplays(knownPaths);

    CHECK(countExternalPaths(db, checksum) == keptPaths);

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_forget_missing_many");
}

TEST_CASE("Queries selectReplays reports hasExternalPath per replay") {
    QSqlDatabase db = openMigratedDb("queries_select_replays");
    Queries queries{QSqlQuery(db)};

    const QByteArray withPath = "checksum-with-path";
    const QByteArray withoutPath = "checksum-without-path";
    queries.insertReplay(makeMetadata(withPath, "Map A"));
    queries.insertReplay(makeMetadata(withoutPath, "Map B"));
    queries.insertExternalFilename(withPath, "C:/replays/a.KWReplay");

    const QList<Replay> replays = queries.selectReplays();
    REQUIRE(replays.size() == 2);

    QHash<QByteArray, Replay> byChecksum;
    for (const auto& replay : replays) {
        byChecksum.insert(replay.checksum, replay);
    }

    REQUIRE(byChecksum.contains(withPath));
    REQUIRE(byChecksum.contains(withoutPath));
    CHECK(byChecksum.value(withPath).hasExternalPath);
    CHECK_FALSE(byChecksum.value(withoutPath).hasExternalPath);
    CHECK(byChecksum.value(withPath).mapName == "Map A");
    CHECK(byChecksum.value(withoutPath).mapName == "Map B");

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_select_replays");
}
