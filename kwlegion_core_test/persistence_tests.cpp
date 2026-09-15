// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include <kwlegion_core/persistence.h>
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

using namespace KWLegionCore;

namespace {

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

TEST_CASE("Persistence migrate creates the schema and is idempotent",
          "[persistence][sql][schema]") {
    Persistence persistence(":memory:", "persistence_migrate_idempotent");
    persistence.init();
    QSqlDatabase db = QSqlDatabase::database("persistence_migrate_idempotent");

    // Calling migrate() again once the schema is already current should be
    // a harmless no-op rather than trying to re-run already-applied DDL.
    CHECK_NOTHROW(persistence.migrate());

    QSqlQuery check(db);
    REQUIRE(check.exec(
        "SELECT name FROM sqlite_master WHERE type = 'table' AND name IN "
        "('replays', 'replay_external_paths', 'replay_players')"));
    int tableCount = 0;
    while (check.next()) {
        tableCount++;
    }
    CHECK(tableCount == 3);

    persistence.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("persistence_migrate_idempotent");
}

TEST_CASE("Persistence isReplayKnown reflects insertReplay",
          "[persistence][sql][replay-crud]") {
    Persistence persistence(":memory:", "persistence_is_known");
    persistence.init();
    QSqlDatabase db = QSqlDatabase::database("persistence_is_known");

    const QByteArray checksum = "checksum-known";
    CHECK_FALSE(persistence.isReplayKnown(checksum));

    persistence.insertReplay(makeMetadata(checksum));

    CHECK(persistence.isReplayKnown(checksum));
    CHECK_FALSE(persistence.isReplayKnown("checksum-different"));

    persistence.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("persistence_is_known");
}

TEST_CASE("Persistence selectReplay returns the stored fields",
          "[persistence][sql][replay-crud]") {
    Persistence persistence(":memory:", "persistence_select_replay");
    persistence.init();
    QSqlDatabase db = QSqlDatabase::database("persistence_select_replay");

    const QByteArray checksum = "checksum-select";
    const QDateTime timestamp =
        QDateTime::fromSecsSinceEpoch(1700000000, QTimeZone(QTimeZone::UTC));
    LegionParser::ReplaySynopsis metadata =
        makeMetadata(checksum, "Sample Map");
    metadata.timestamp = timestamp;
    metadata.matchTitle = "Stored Match Title";
    metadata.matchDescription = "Stored match description";
    metadata.mapReference = "data/maps/stored-reference";
    persistence.insertReplay(metadata);

    const std::optional<Replay> replay = persistence.selectReplay(checksum);
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

    persistence.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("persistence_select_replay");
}

TEST_CASE("Persistence selectReplay returns nullopt for an unknown checksum",
          "[persistence][sql][replay-crud]") {
    Persistence persistence(":memory:", "persistence_select_replay_missing");
    persistence.init();
    QSqlDatabase db =
        QSqlDatabase::database("persistence_select_replay_missing");

    CHECK_FALSE(persistence.selectReplay("does-not-exist").has_value());

    persistence.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("persistence_select_replay_missing");
}

TEST_CASE("Persistence insertReplay throws on a duplicate checksum",
          "[persistence][sql][replay-crud]") {
    Persistence persistence(":memory:", "persistence_insert_duplicate");
    persistence.init();
    QSqlDatabase db = QSqlDatabase::database("persistence_insert_duplicate");

    const QByteArray checksum = "checksum-dup";
    persistence.insertReplay(makeMetadata(checksum));

    CHECK_THROWS_AS(persistence.insertReplay(makeMetadata(checksum)),
                    StorageException);

    persistence.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("persistence_insert_duplicate");
}

TEST_CASE("Persistence doesReplayNeedAnalysis reflects insertReplayAnalysis",
          "[persistence][sql][analysis]") {
    Persistence persistence(":memory:", "persistence_needs_analysis");
    persistence.init();
    QSqlDatabase db = QSqlDatabase::database("persistence_needs_analysis");

    const QByteArray checksum = "checksum-needs-analysis";
    LegionParser::ReplaySynopsis metadata = makeMetadata(checksum);
    metadata.bodyOffset = 512;
    metadata.players = {
        LegionParser::Player{.id = 1,
                             .name = "Alice",
                             .teamNumber = 0,
                             .faction = LegionParser::Faction::GDI,
                             .isComputer = false,
                             .isReplaySaver = true}};
    persistence.insertReplay(metadata);

    // insertReplay alone no longer writes replay_analysis - that's now a
    // separate step - so a freshly-inserted replay still needs analysis.
    CHECK(persistence.doesReplayNeedAnalysis(checksum));

    // Needing analysis also requires replay_players to be populated -
    // without this, insertReplayAnalysis alone would still leave the
    // replay needing (player) analysis, see the dedicated test below.
    persistence.insertReplayPlayers(checksum, metadata.players);
    persistence.insertReplayAnalysis(metadata);

    CHECK_FALSE(persistence.doesReplayNeedAnalysis(checksum));

    persistence.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("persistence_needs_analysis");
}

TEST_CASE(
    "Persistence doesReplayNeedAnalysis is true with a replay_analysis row but "
    "no replay_players row",
    "[persistence][sql][analysis]") {
    Persistence persistence(":memory:",
                            "persistence_needs_analysis_no_players");
    persistence.init();
    QSqlDatabase db =
        QSqlDatabase::database("persistence_needs_analysis_no_players");

    const QByteArray checksum = "checksum-analysis-no-players";
    LegionParser::ReplaySynopsis metadata = makeMetadata(checksum);
    persistence.insertReplay(metadata);
    persistence.insertReplayAnalysis(metadata);
    // Deliberately never call insertReplayPlayers for this checksum.

    CHECK(persistence.doesReplayNeedAnalysis(checksum));

    persistence.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("persistence_needs_analysis_no_players");
}

TEST_CASE("Persistence insertReplayAnalysis stores the body offset",
          "[persistence][sql][analysis]") {
    Persistence persistence(":memory:", "persistence_insert_analysis");
    persistence.init();
    QSqlDatabase db = QSqlDatabase::database("persistence_insert_analysis");

    const QByteArray checksum = "checksum-insert-analysis";
    LegionParser::ReplaySynopsis metadata = makeMetadata(checksum);
    metadata.bodyOffset = 4096;
    persistence.insertReplay(metadata);
    persistence.insertReplayAnalysis(metadata);

    QSqlQuery check(db);
    REQUIRE(check.prepare(
        "SELECT body_offset FROM replay_analysis WHERE replay_checksum = "
        ":checksum"));
    check.bindValue(":checksum", checksum);
    REQUIRE(check.exec());
    REQUIRE(check.next());
    CHECK(check.value(0).toLongLong() == 4096);

    persistence.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("persistence_insert_analysis");
}

TEST_CASE("Persistence insertReplayAnalysis stores the engine ticks",
          "[persistence][sql][analysis]") {
    Persistence persistence(":memory:", "persistence_insert_analysis_ticks");
    persistence.init();
    QSqlDatabase db =
        QSqlDatabase::database("persistence_insert_analysis_ticks");

    const QByteArray checksum = "checksum-insert-analysis-ticks";
    LegionParser::ReplaySynopsis metadata = makeMetadata(checksum);
    metadata.engineTicks = 12345;
    persistence.insertReplay(metadata);
    persistence.insertReplayAnalysis(metadata);

    QSqlQuery check(db);
    REQUIRE(check.prepare(
        "SELECT engine_ticks FROM replay_analysis WHERE replay_checksum = "
        ":checksum"));
    check.bindValue(":checksum", checksum);
    REQUIRE(check.exec());
    REQUIRE(check.next());
    CHECK(check.value(0).toLongLong() == 12345);

    persistence.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("persistence_insert_analysis_ticks");
}

TEST_CASE(
    "Persistence selectReplaysNeedingAnalysis lists replays missing an "
    "analysis row",
    "[persistence][sql][analysis]") {
    Persistence persistence(":memory:", "persistence_needs_analysis_list");
    persistence.init();
    QSqlDatabase db = QSqlDatabase::database("persistence_needs_analysis_list");

    const QByteArray analyzed = "checksum-analyzed";
    const QByteArray pending = "checksum-pending";

    auto analyzedMeta = makeMetadata(analyzed);
    analyzedMeta.players = {
        LegionParser::Player{.id = 1, .name = "P1", .teamNumber = 0}};
    persistence.insertReplay(analyzedMeta);
    persistence.insertReplayPlayers(analyzedMeta.checksum,
                                    analyzedMeta.players);
    persistence.insertReplayAnalysis(analyzedMeta);

    auto pendingMeta = makeMetadata(pending);
    pendingMeta.players = {
        LegionParser::Player{.id = 1, .name = "P1", .teamNumber = 0}};
    persistence.insertReplay(pendingMeta);
    persistence.insertReplayPlayers(pendingMeta.checksum, pendingMeta.players);

    const QList<QByteArray> needsAnalysis =
        persistence.selectReplaysNeedingAnalysis();

    CHECK(needsAnalysis.size() == 1);
    CHECK(needsAnalysis.contains(pending));
    CHECK_FALSE(needsAnalysis.contains(analyzed));

    persistence.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("persistence_needs_analysis_list");
}

TEST_CASE(
    "Persistence selectReplaysNeedingAnalysis is empty once every replay has "
    "analysis and players",
    "[persistence][sql][analysis]") {
    Persistence persistence(":memory:", "persistence_needs_analysis_none");
    persistence.init();
    QSqlDatabase db = QSqlDatabase::database("persistence_needs_analysis_none");

    const QByteArray checksum = "checksum-fully-analyzed";
    auto metadata = makeMetadata(checksum);
    metadata.players = {
        LegionParser::Player{.id = 1, .name = "P1", .teamNumber = 0}};
    persistence.insertReplay(metadata);
    persistence.insertReplayPlayers(checksum, metadata.players);
    persistence.insertReplayAnalysis(metadata);

    CHECK(persistence.selectReplaysNeedingAnalysis().isEmpty());

    persistence.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("persistence_needs_analysis_none");
}

TEST_CASE("Persistence insertReplayPlayers stores every player for a replay",
          "[persistence][sql][players]") {
    Persistence persistence(":memory:", "persistence_insert_players");
    persistence.init();
    QSqlDatabase db = QSqlDatabase::database("persistence_insert_players");

    const QByteArray checksum = "checksum-players";
    persistence.insertReplay(makeMetadata(checksum));

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

    persistence.insertReplayPlayers(checksum, players);

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

    persistence.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("persistence_insert_players");
}

TEST_CASE("Persistence insertReplayPlayers with an empty list inserts nothing",
          "[persistence][sql][players]") {
    Persistence persistence(":memory:", "persistence_insert_players_empty");
    persistence.init();
    QSqlDatabase db =
        QSqlDatabase::database("persistence_insert_players_empty");

    const QByteArray checksum = "checksum-no-players";
    persistence.insertReplay(makeMetadata(checksum));

    persistence.insertReplayPlayers(checksum, {});

    CHECK(countPlayers(db, checksum) == 0);

    persistence.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("persistence_insert_players_empty");
}

TEST_CASE("Persistence insertExternalFilename reports whether the path is new",
          "[persistence][sql][external-path]") {
    Persistence persistence(":memory:", "persistence_insert_external");
    persistence.init();
    QSqlDatabase db = QSqlDatabase::database("persistence_insert_external");

    const QByteArray checksum = "checksum-external";
    persistence.insertReplay(makeMetadata(checksum));

    CHECK(
        persistence.insertExternalFilename(checksum, "C:/replays/a.KWReplay"));
    CHECK_FALSE(
        persistence.insertExternalFilename(checksum, "C:/replays/a.KWReplay"));
    CHECK(
        persistence.insertExternalFilename(checksum, "C:/replays/b.KWReplay"));

    CHECK(countExternalPaths(db, checksum) == 2);

    persistence.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("persistence_insert_external");
}

TEST_CASE(
    "Persistence insertExternalFilename reassigns a path claimed by a "
    "different checksum",
    "[persistence][sql][external-path]") {
    Persistence persistence(":memory:", "persistence_insert_external_reassign");
    persistence.init();
    QSqlDatabase db =
        QSqlDatabase::database("persistence_insert_external_reassign");

    const QByteArray oldChecksum = "checksum-old";
    const QByteArray newChecksum = "checksum-new";
    const QString path = "C:/replays/Last Replay.KWReplay";

    persistence.insertReplay(makeMetadata(oldChecksum));
    persistence.insertReplay(makeMetadata(newChecksum));
    persistence.insertExternalFilename(oldChecksum, path);

    // external_path is the sole key - a path can only ever point at one
    // checksum, so re-inserting it under a new checksum (e.g. the game's
    // rolling "Last Replay.KWReplay" being overwritten by a new match) must
    // atomically move the row rather than erroring or leaving a duplicate.
    CHECK(persistence.insertExternalFilename(newChecksum, path));
    CHECK(countExternalPaths(db, oldChecksum) == 0);
    CHECK(countExternalPaths(db, newChecksum) == 1);

    // Re-inserting the same (checksum, path) pair again is a true no-op.
    CHECK_FALSE(persistence.insertExternalFilename(newChecksum, path));
    CHECK(countExternalPaths(db, newChecksum) == 1);

    persistence.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("persistence_insert_external_reassign");
}

TEST_CASE(
    "Persistence checksumForExternalPath reflects the current owner of a path",
    "[persistence][sql][external-path]") {
    Persistence persistence(":memory:", "persistence_checksum_for_path");
    persistence.init();
    QSqlDatabase db = QSqlDatabase::database("persistence_checksum_for_path");

    const QByteArray checksum = "checksum-lookup";
    const QString path = "C:/replays/Last Replay.KWReplay";

    CHECK_FALSE(persistence.checksumForExternalPath(path).has_value());

    persistence.insertReplay(makeMetadata(checksum));
    persistence.insertExternalFilename(checksum, path);

    const std::optional<QByteArray> found =
        persistence.checksumForExternalPath(path);
    REQUIRE(found.has_value());
    CHECK(found.value() == checksum);

    persistence.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("persistence_checksum_for_path");
}

TEST_CASE("Persistence removeExternalFilename drops only the given path",
          "[persistence][sql][external-path]") {
    Persistence persistence(":memory:", "persistence_remove_external");
    persistence.init();
    QSqlDatabase db = QSqlDatabase::database("persistence_remove_external");

    const QByteArray checksum = "checksum-remove-external";
    persistence.insertReplay(makeMetadata(checksum));
    persistence.insertExternalFilename(checksum, "C:/replays/a.KWReplay");
    persistence.insertExternalFilename(checksum, "C:/replays/b.KWReplay");

    const std::optional<QByteArray> dropped =
        persistence.removeExternalFilename("C:/replays/a.KWReplay");
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

    persistence.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("persistence_remove_external");
}

TEST_CASE(
    "Persistence removeExternalFilename on an unregistered path is a harmless "
    "no-op",
    "[persistence][sql][external-path]") {
    Persistence persistence(":memory:", "persistence_remove_external_missing");
    persistence.init();
    QSqlDatabase db =
        QSqlDatabase::database("persistence_remove_external_missing");

    const QByteArray checksum = "checksum-remove-external-missing";
    persistence.insertReplay(makeMetadata(checksum));
    persistence.insertExternalFilename(checksum, "C:/replays/a.KWReplay");

    CHECK_FALSE(
        persistence.removeExternalFilename("C:/replays/does-not-exist.KWReplay")
            .has_value());

    CHECK(countExternalPaths(db, checksum) == 1);

    persistence.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("persistence_remove_external_missing");
}

TEST_CASE(
    "Persistence forgetMissingReplays drops paths absent from the current "
    "listing",
    "[persistence][sql][external-path]") {
    Persistence persistence(":memory:", "persistence_forget_missing");
    persistence.init();
    QSqlDatabase db = QSqlDatabase::database("persistence_forget_missing");

    const QByteArray checksum = "checksum-forget";
    persistence.insertReplay(makeMetadata(checksum));
    persistence.insertExternalFilename(checksum, "C:/replays/a.KWReplay");
    persistence.insertExternalFilename(checksum, "C:/replays/b.KWReplay");

    persistence.forgetMissingReplays({"C:/replays/a.KWReplay"});

    CHECK(countExternalPaths(db, checksum) == 1);

    // An empty "currently known" list means nothing on disk is known any
    // more, so every remaining path should be dropped too - the replay row
    // itself is untouched, only its external-path registrations are.
    persistence.forgetMissingReplays({});

    CHECK(countExternalPaths(db, checksum) == 0);

    const std::optional<Replay> replay = persistence.selectReplay(checksum);
    REQUIRE(replay.has_value());
    CHECK_FALSE(replay->hasExternalPath);

    persistence.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("persistence_forget_missing");
}

TEST_CASE(
    "Persistence forgetMissingReplays handles more paths than SQLite's bound "
    "parameter limit",
    "[persistence][sql][external-path]") {
    Persistence persistence(":memory:", "persistence_forget_missing_many");
    persistence.init();
    QSqlDatabase db = QSqlDatabase::database("persistence_forget_missing_many");

    const QByteArray checksum = "checksum-many-paths";
    persistence.insertReplay(makeMetadata(checksum));

    // A large replay folder may accumulate m any files
    constexpr int totalPaths = 35000;
    constexpr int keptPaths = 33000;
    QList<QString> knownPaths;
    knownPaths.reserve(keptPaths);
    for (int i = 0; i < totalPaths; i++) {
        const QString path =
            QStringLiteral("C:/replays/replay-%1.KWReplay").arg(i);
        persistence.insertExternalFilename(checksum, path);
        if (i < keptPaths) {
            knownPaths.append(path);
        }
    }
    REQUIRE(countExternalPaths(db, checksum) == totalPaths);

    persistence.forgetMissingReplays(knownPaths);

    CHECK(countExternalPaths(db, checksum) == keptPaths);

    persistence.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("persistence_forget_missing_many");
}

TEST_CASE("Persistence selectReplays reports hasExternalPath per replay",
          "[persistence][sql][replay-crud]") {
    Persistence persistence(":memory:", "persistence_select_replays");
    persistence.init();
    QSqlDatabase db = QSqlDatabase::database("persistence_select_replays");

    const QByteArray withPath = "checksum-with-path";
    const QByteArray withoutPath = "checksum-without-path";
    LegionParser::ReplaySynopsis withPathMetadata =
        makeMetadata(withPath, "Map A");
    withPathMetadata.matchTitle = "Match A";
    withPathMetadata.matchDescription = "Description A";
    withPathMetadata.mapReference = "Reference A";
    persistence.insertReplay(withPathMetadata);

    LegionParser::ReplaySynopsis withoutPathMetadata =
        makeMetadata(withoutPath, "Map B");
    withoutPathMetadata.matchTitle = "Match B";
    withoutPathMetadata.matchDescription = "Description B";
    withoutPathMetadata.mapReference = "Reference B";
    persistence.insertReplay(withoutPathMetadata);
    persistence.insertExternalFilename(withPath, "C:/replays/a.KWReplay");

    const QList<Replay> replays = persistence.selectReplays();
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

    persistence.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("persistence_select_replays");
}

TEST_CASE("Persistence selectReplay defaults overrideMatchTitle to empty",
          "[persistence][sql][override-title]") {
    Persistence persistence(":memory:", "persistence_override_title_default");
    persistence.init();
    QSqlDatabase db =
        QSqlDatabase::database("persistence_override_title_default");

    const QByteArray checksum = "checksum-override-default";
    persistence.insertReplay(makeMetadata(checksum));

    const std::optional<Replay> replay = persistence.selectReplay(checksum);
    REQUIRE(replay.has_value());
    CHECK(replay->overrideMatchTitle.isEmpty());

    persistence.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("persistence_override_title_default");
}

TEST_CASE(
    "Persistence updateOverrideTitle is reflected by selectReplay and "
    "selectReplays",
    "[persistence][sql][override-title]") {
    Persistence persistence(":memory:", "persistence_override_title_update");
    persistence.init();
    QSqlDatabase db =
        QSqlDatabase::database("persistence_override_title_update");

    const QByteArray checksum = "checksum-override-update";
    persistence.insertReplay(makeMetadata(checksum));

    persistence.updateOverrideTitle(checksum, "Custom Title");

    const std::optional<Replay> replay = persistence.selectReplay(checksum);
    REQUIRE(replay.has_value());
    CHECK(replay->overrideMatchTitle == "Custom Title");

    const QList<Replay> replays = persistence.selectReplays();
    REQUIRE(replays.size() == 1);
    CHECK(replays.first().overrideMatchTitle == "Custom Title");

    persistence.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("persistence_override_title_update");
}

TEST_CASE("Persistence updateOverrideTitle overwrites a previous override",
          "[persistence][sql][override-title]") {
    Persistence persistence(":memory:", "persistence_override_title_overwrite");
    persistence.init();
    QSqlDatabase db =
        QSqlDatabase::database("persistence_override_title_overwrite");

    const QByteArray checksum = "checksum-override-overwrite";
    persistence.insertReplay(makeMetadata(checksum));

    persistence.updateOverrideTitle(checksum, "First Title");
    persistence.updateOverrideTitle(checksum, "Second Title");

    const std::optional<Replay> replay = persistence.selectReplay(checksum);
    REQUIRE(replay.has_value());
    CHECK(replay->overrideMatchTitle == "Second Title");

    persistence.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("persistence_override_title_overwrite");
}

TEST_CASE(
    "Persistence updateOverrideTitle with an empty string clears the override",
    "[persistence][sql][override-title]") {
    Persistence persistence(":memory:", "persistence_override_title_clear");
    persistence.init();
    QSqlDatabase db =
        QSqlDatabase::database("persistence_override_title_clear");

    const QByteArray checksum = "checksum-override-clear";
    persistence.insertReplay(makeMetadata(checksum));
    persistence.updateOverrideTitle(checksum, "Custom Title");

    persistence.updateOverrideTitle(checksum, "");

    const std::optional<Replay> replay = persistence.selectReplay(checksum);
    REQUIRE(replay.has_value());
    CHECK(replay->overrideMatchTitle.isEmpty());

    persistence.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("persistence_override_title_clear");
}
