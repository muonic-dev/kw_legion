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
    migrator.migrate();

    return db;
}

// The checksum only needs to be unique per test, not a real SHA-256 - a
// short literal tag is enough to identify a row.
LegionParser::ReplaySynopsis makeMetadata(const QByteArray& checksum,
                                          const QString& mapName = "Test Map") {
    return LegionParser::ReplaySynopsis{
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
    CHECK_NOTHROW(queries.migrate());

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
    LegionParser::ReplaySynopsis metadata =
        makeMetadata(checksum, "Sample Map");
    metadata.timestamp = timestamp;
    metadata.matchTitle = "Stored Match Title";
    metadata.matchDescription = "Stored match description";
    metadata.mapReference = "data/maps/stored-reference";
    queries.insertReplay(metadata);

    const std::optional<Replay> replay = queries.selectReplay(checksum);
    REQUIRE(replay.has_value());
    CHECK(replay->checksum == checksum);
    CHECK(replay->matchTitle == "Stored Match Title");
    CHECK(replay->matchDescription == "Stored match description");
    CHECK(replay->mapName == "Sample Map");
    CHECK(replay->mapReference == "data/maps/stored-reference");
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

TEST_CASE("Queries doesReplayNeedAnalysis reflects insertReplayAnalysis") {
    QSqlDatabase db = openMigratedDb("queries_needs_analysis");
    Queries queries{QSqlQuery(db)};

    const QByteArray checksum = "checksum-needs-analysis";
    LegionParser::ReplaySynopsis metadata = makeMetadata(checksum);
    metadata.bodyOffset = 512;
    queries.insertReplay(metadata);

    // insertReplay alone no longer writes replay_analysis - that's now a
    // separate step - so a freshly-inserted replay still needs analysis.
    CHECK(queries.doesReplayNeedAnalysis(checksum));

    queries.insertReplayAnalysis(metadata);

    CHECK_FALSE(queries.doesReplayNeedAnalysis(checksum));

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_needs_analysis");
}

TEST_CASE("Queries insertReplayAnalysis stores the body offset") {
    QSqlDatabase db = openMigratedDb("queries_insert_analysis");
    Queries queries{QSqlQuery(db)};

    const QByteArray checksum = "checksum-insert-analysis";
    LegionParser::ReplaySynopsis metadata = makeMetadata(checksum);
    metadata.bodyOffset = 4096;
    queries.insertReplay(metadata);
    queries.insertReplayAnalysis(metadata);

    QSqlQuery check(db);
    REQUIRE(check.prepare(
        "SELECT body_offset FROM replay_analysis WHERE replay_checksum = "
        ":checksum"));
    check.bindValue(":checksum", checksum);
    REQUIRE(check.exec());
    REQUIRE(check.next());
    CHECK(check.value(0).toLongLong() == 4096);

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_insert_analysis");
}

TEST_CASE("Queries insertReplayAnalysis throws on a duplicate checksum") {
    QSqlDatabase db = openMigratedDb("queries_insert_analysis_dup");
    Queries queries{QSqlQuery(db)};

    const QByteArray checksum = "checksum-analysis-dup";
    LegionParser::ReplaySynopsis metadata = makeMetadata(checksum);
    queries.insertReplay(metadata);
    queries.insertReplayAnalysis(metadata);

    CHECK_THROWS_AS(queries.insertReplayAnalysis(metadata), StorageException);

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_insert_analysis_dup");
}

TEST_CASE(
    "Queries selectReplaysNeedingAnalysis lists only replays missing an "
    "analysis row") {
    QSqlDatabase db = openMigratedDb("queries_needs_analysis_list");
    Queries queries{QSqlQuery(db)};

    const QByteArray analyzed = "checksum-analyzed";
    const QByteArray pending = "checksum-pending";
    queries.insertReplay(makeMetadata(analyzed));
    queries.insertReplayAnalysis(makeMetadata(analyzed));
    queries.insertReplay(makeMetadata(pending));

    const QList<QByteArray> needsAnalysis =
        queries.selectReplaysNeedingAnalysis();

    CHECK(needsAnalysis.size() == 1);
    CHECK(needsAnalysis.contains(pending));
    CHECK_FALSE(needsAnalysis.contains(analyzed));

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_needs_analysis_list");
}

TEST_CASE(
    "Queries selectReplaysNeedingAnalysis is empty once every replay has "
    "analysis") {
    QSqlDatabase db = openMigratedDb("queries_needs_analysis_none");
    Queries queries{QSqlQuery(db)};

    const QByteArray checksum = "checksum-fully-analyzed";
    queries.insertReplay(makeMetadata(checksum));
    queries.insertReplayAnalysis(makeMetadata(checksum));

    CHECK(queries.selectReplaysNeedingAnalysis().isEmpty());

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_needs_analysis_none");
}

TEST_CASE("Queries insertReplayPlayers stores every player for a replay") {
    QSqlDatabase db = openMigratedDb("queries_insert_players");
    Queries queries{QSqlQuery(db)};

    const QByteArray checksum = "checksum-players";
    queries.insertReplay(makeMetadata(checksum));

    const QList<LegionParser::Player> players{
        LegionParser::Player{.id = 1,
                             .name = "Alice",
                             .teamNumber = 0,
                             .faction = LegionParser::Faction::GDI,
                             .isComputer = false,
                             .isReplaySaver = true},
        LegionParser::Player{.id = 2,
                             .name = "Bob",
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
    "Queries insertExternalFilename reassigns a path claimed by a "
    "different checksum") {
    QSqlDatabase db = openMigratedDb("queries_insert_external_reassign");
    Queries queries{QSqlQuery(db)};

    const QByteArray oldChecksum = "checksum-old";
    const QByteArray newChecksum = "checksum-new";
    const QString path = "C:/replays/Last Replay.KWReplay";

    queries.insertReplay(makeMetadata(oldChecksum));
    queries.insertReplay(makeMetadata(newChecksum));
    queries.insertExternalFilename(oldChecksum, path);

    // external_path is the sole key - a path can only ever point at one
    // checksum, so re-inserting it under a new checksum (e.g. the game's
    // rolling "Last Replay.KWReplay" being overwritten by a new match) must
    // atomically move the row rather than erroring or leaving a duplicate.
    CHECK(queries.insertExternalFilename(newChecksum, path));
    CHECK(countExternalPaths(db, oldChecksum) == 0);
    CHECK(countExternalPaths(db, newChecksum) == 1);

    // Re-inserting the same (checksum, path) pair again is a true no-op.
    CHECK_FALSE(queries.insertExternalFilename(newChecksum, path));
    CHECK(countExternalPaths(db, newChecksum) == 1);

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_insert_external_reassign");
}

TEST_CASE(
    "Queries checksumForExternalPath reflects the current owner of a path") {
    QSqlDatabase db = openMigratedDb("queries_checksum_for_path");
    Queries queries{QSqlQuery(db)};

    const QByteArray checksum = "checksum-lookup";
    const QString path = "C:/replays/Last Replay.KWReplay";

    CHECK_FALSE(queries.checksumForExternalPath(path).has_value());

    queries.insertReplay(makeMetadata(checksum));
    queries.insertExternalFilename(checksum, path);

    const std::optional<QByteArray> found =
        queries.checksumForExternalPath(path);
    REQUIRE(found.has_value());
    CHECK(found.value() == checksum);

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_checksum_for_path");
}

TEST_CASE("Queries removeExternalFilename drops only the given path") {
    QSqlDatabase db = openMigratedDb("queries_remove_external");
    Queries queries{QSqlQuery(db)};

    const QByteArray checksum = "checksum-remove-external";
    queries.insertReplay(makeMetadata(checksum));
    queries.insertExternalFilename(checksum, "C:/replays/a.KWReplay");
    queries.insertExternalFilename(checksum, "C:/replays/b.KWReplay");

    const std::optional<QByteArray> dropped =
        queries.removeExternalFilename("C:/replays/a.KWReplay");
    REQUIRE(dropped.has_value());
    CHECK(dropped.value() == checksum);

    CHECK(countExternalPaths(db, checksum) == 1);

    QSqlQuery check(db);
    REQUIRE(check.prepare(
        "SELECT COUNT(*) FROM replay_external_paths WHERE external_path = "
        ":path"));
    check.bindValue(":path", "C:/replays/b.KWReplay");
    REQUIRE(check.exec());
    REQUIRE(check.next());
    CHECK(check.value(0).toInt() == 1);

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_remove_external");
}

TEST_CASE(
    "Queries removeExternalFilename on an unregistered path is a harmless "
    "no-op") {
    QSqlDatabase db = openMigratedDb("queries_remove_external_missing");
    Queries queries{QSqlQuery(db)};

    const QByteArray checksum = "checksum-remove-external-missing";
    queries.insertReplay(makeMetadata(checksum));
    queries.insertExternalFilename(checksum, "C:/replays/a.KWReplay");

    CHECK_FALSE(
        queries.removeExternalFilename("C:/replays/does-not-exist.KWReplay")
            .has_value());

    CHECK(countExternalPaths(db, checksum) == 1);

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_remove_external_missing");
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
    LegionParser::ReplaySynopsis withPathMetadata =
        makeMetadata(withPath, "Map A");
    withPathMetadata.matchTitle = "Match A";
    withPathMetadata.matchDescription = "Description A";
    withPathMetadata.mapReference = "Reference A";
    queries.insertReplay(withPathMetadata);

    LegionParser::ReplaySynopsis withoutPathMetadata =
        makeMetadata(withoutPath, "Map B");
    withoutPathMetadata.matchTitle = "Match B";
    withoutPathMetadata.matchDescription = "Description B";
    withoutPathMetadata.mapReference = "Reference B";
    queries.insertReplay(withoutPathMetadata);
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
    CHECK(byChecksum.value(withPath).matchTitle == "Match A");
    CHECK(byChecksum.value(withPath).matchDescription == "Description A");
    CHECK(byChecksum.value(withPath).mapName == "Map A");
    CHECK(byChecksum.value(withPath).mapReference == "Reference A");
    CHECK(byChecksum.value(withoutPath).matchTitle == "Match B");
    CHECK(byChecksum.value(withoutPath).matchDescription == "Description B");
    CHECK(byChecksum.value(withoutPath).mapName == "Map B");
    CHECK(byChecksum.value(withoutPath).mapReference == "Reference B");

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_select_replays");
}

TEST_CASE("Queries selectReplay defaults overrideMatchTitle to empty") {
    QSqlDatabase db = openMigratedDb("queries_override_title_default");
    Queries queries{QSqlQuery(db)};

    const QByteArray checksum = "checksum-override-default";
    queries.insertReplay(makeMetadata(checksum));

    const std::optional<Replay> replay = queries.selectReplay(checksum);
    REQUIRE(replay.has_value());
    CHECK(replay->overrideMatchTitle.isEmpty());

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_override_title_default");
}

TEST_CASE(
    "Queries updateOverrideTitle is reflected by selectReplay and "
    "selectReplays") {
    QSqlDatabase db = openMigratedDb("queries_override_title_update");
    Queries queries{QSqlQuery(db)};

    const QByteArray checksum = "checksum-override-update";
    queries.insertReplay(makeMetadata(checksum));

    queries.updateOverrideTitle(checksum, "Custom Title");

    const std::optional<Replay> replay = queries.selectReplay(checksum);
    REQUIRE(replay.has_value());
    CHECK(replay->overrideMatchTitle == "Custom Title");

    const QList<Replay> replays = queries.selectReplays();
    REQUIRE(replays.size() == 1);
    CHECK(replays.first().overrideMatchTitle == "Custom Title");

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_override_title_update");
}

TEST_CASE("Queries updateOverrideTitle overwrites a previous override") {
    QSqlDatabase db = openMigratedDb("queries_override_title_overwrite");
    Queries queries{QSqlQuery(db)};

    const QByteArray checksum = "checksum-override-overwrite";
    queries.insertReplay(makeMetadata(checksum));

    queries.updateOverrideTitle(checksum, "First Title");
    queries.updateOverrideTitle(checksum, "Second Title");

    const std::optional<Replay> replay = queries.selectReplay(checksum);
    REQUIRE(replay.has_value());
    CHECK(replay->overrideMatchTitle == "Second Title");

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_override_title_overwrite");
}

TEST_CASE(
    "Queries updateOverrideTitle with an empty string clears the override") {
    QSqlDatabase db = openMigratedDb("queries_override_title_clear");
    Queries queries{QSqlQuery(db)};

    const QByteArray checksum = "checksum-override-clear";
    queries.insertReplay(makeMetadata(checksum));
    queries.updateOverrideTitle(checksum, "Custom Title");

    queries.updateOverrideTitle(checksum, "");

    const std::optional<Replay> replay = queries.selectReplay(checksum);
    REQUIRE(replay.has_value());
    CHECK(replay->overrideMatchTitle.isEmpty());

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("queries_override_title_clear");
}
