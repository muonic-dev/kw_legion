// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include <legionparser/exception.h>
#include <legionparser/synopsisparser.h>

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QSet>
#include <QTimeZone>
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace LegionParser;

namespace {

ReplaySynopsis parseReplay(const QString& filename) {
    const QString replayPath =
        QDir(QString::fromUtf8(REPLAY_TEST_DATA_DIR)).filePath(filename);
    QFile replayFile(replayPath);
    REQUIRE(replayFile.open(QIODevice::ReadOnly));
    return LegionParser::SynopsisParser::parse(replayFile);
}

// Each test_<faction>s.KWReplay is a skirmish where Muonic played a mirror
// match against a Hard AI opponent, so both players' declared factions
// should agree.
void checkMirrorMatchFaction(const QString& filename, Faction faction) {
    const ReplaySynopsis metadata = parseReplay(filename);

    REQUIRE(metadata.players.size() == 3);

    CHECK(metadata.players.at(0).name == QLatin1String("Muonic"));
    CHECK(metadata.players.at(0).isReplaySaver);
    CHECK(metadata.players.at(0).faction == faction);

    CHECK(metadata.players.at(1).name == QLatin1String("Hard AI"));
    CHECK(metadata.players.at(1).faction == faction);

    // The trailing commentator slot's faction field isn't a real faction ID.
    CHECK(metadata.players.at(2).faction == Faction::Unknown);
}

// QBuffer is seekable, so this stands in for the pipe/socket case where
// looksComplete can't inspect the tail at all.
class SequentialBuffer : public QBuffer {
   public:
    using QBuffer::QBuffer;

    [[nodiscard]] bool isSequential() const override { return true; }
};

}  // namespace

TEST_CASE("torn read (footer cut off mid-write) is rejected",
          "[legionparser][body]") {
    // test_torn_footer.KWReplay is test_gdis.KWReplay truncated 5 bytes into
    // the "C&C3 REPLAY FOOTER" magic, simulating a replay read while the
    // game was still flushing its footer to disk.
    CHECK_THROWS_AS(parseReplay(QString::fromUtf8("test_torn_footer.KWReplay")),
                    TornDataException);
}

TEST_CASE("looksComplete accepts every replay the parser accepts",
          "[legionparser][footer]") {
    // The invariant the short-circuit rests on: looksComplete inspects a
    // fixed-size window of the tail and must never reject a file a full
    // parse would have accepted. It runs first, so its answer wins - a
    // window smaller than the one verifyFooter gets would silently strand
    // any replay whose footer landed in the gap.
    const QDir dir(QString::fromUtf8(REPLAY_TEST_DATA_DIR));
    const QStringList fixtures = dir.entryList(
        QStringList{QString::fromUtf8("*.KWReplay")}, QDir::Files);
    REQUIRE_FALSE(fixtures.isEmpty());

    for (const QString& filename : fixtures) {
        QFile replayFile(dir.filePath(filename));
        REQUIRE(replayFile.open(QIODevice::ReadOnly));

        bool accepted = true;
        try {
            const ReplaySynopsis metadata = SynopsisParser::parse(replayFile);
            CHECK_FALSE(metadata.checksum.isEmpty());
        } catch (const ReplayParseException&) {
            // e.g. the deliberately torn fixture - nothing to agree about.
            accepted = false;
        }

        if (accepted) {
            REQUIRE(replayFile.seek(0));
            INFO("fixture: " << filename.toStdString());
            CHECK(SynopsisParser::looksComplete(replayFile));
        }
    }
}

TEST_CASE("looksComplete rejects a footer cut off mid-write",
          "[legionparser][footer]") {
    const QDir dir(QString::fromUtf8(REPLAY_TEST_DATA_DIR));
    QFile replayFile(
        dir.filePath(QString::fromUtf8("test_torn_footer.KWReplay")));
    REQUIRE(replayFile.open(QIODevice::ReadOnly));

    CHECK_FALSE(SynopsisParser::looksComplete(replayFile));
}

TEST_CASE("looksComplete leaves the device position untouched",
          "[legionparser][footer]") {
    // Deliberately the largest fixture, so the tail window is genuinely
    // seeked to rather than covering the whole file.
    const QDir dir(QString::fromUtf8(REPLAY_TEST_DATA_DIR));
    QFile replayFile(
        dir.filePath(QString::fromUtf8("8-player all random ffa.KWReplay")));
    REQUIRE(replayFile.open(QIODevice::ReadOnly));
    REQUIRE(replayFile.seek(37));

    CHECK(SynopsisParser::looksComplete(replayFile));
    CHECK(replayFile.pos() == 37);
}

TEST_CASE("looksComplete rejects a file too small to hold a footer",
          "[legionparser][footer]") {
    QByteArray tiny(QByteArrayLiteral("abcd"));
    QBuffer buffer(&tiny);
    REQUIRE(buffer.open(QIODevice::ReadOnly));

    CHECK_FALSE(SynopsisParser::looksComplete(buffer));
}

TEST_CASE("looksComplete defers on a device it cannot seek",
          "[legionparser][footer]") {
    // Nothing about this content ends in a footer, but an unseekable device
    // can't be inspected - answering false would report a torn file on the
    // strength of a check that never ran.
    QByteArray bytes(1024, 'x');
    SequentialBuffer buffer(&bytes);
    REQUIRE(buffer.open(QIODevice::ReadOnly));

    CHECK(SynopsisParser::looksComplete(buffer));
}

TEST_CASE("checksum is unchanged across body-reader implementations",
          "[legionparser][checksum]") {
    // Captured against the pre-TeeDevice body reader (parseBody's
    // readRemainingChunked + QCryptographicHash). The checksum is a
    // cross-path replay identity key - ReplayStore joins replay_players and
    // replay_external_paths on it - so any drift here would silently orphan
    // every replay already indexed under its old checksum. test_torn_footer
    // is excluded: it never reaches a checksum, see the "torn read" test.
    static const struct {
        const char* filename;
        const char* expectedHex;
    } fixtures[] = {
        {"4-player different versions.KWReplay",
         "a2ca205834b51286d32654dcb0244f6945b00405cba36968d097b0985883fef6"},
        {"4-player ffa.KWReplay",
         "2673a90896b4c76b125d2e88926c9f907609b7888fed6bc01a24ffe37d6f6d02"},
        {"4-player skirmish.KWReplay",
         "3d4fedb745189a6e3f0345be50f877306ff586d4a8ec60978e475904944396f4"},
        {"4-player team 4.KWReplay",
         "c749076559f87271ca6badd404ac073a4f0fd9f2185f7a7d0f32e1e4045ef290"},
        {"8-player all random ffa.KWReplay",
         "2c9e397bf646d11c56ac12a8f5f9191093e4b640197a3d1d06f3f1981d9acdcf"},
        {"muonic v branston game 1.KWReplay",
         "7cac36e6d548cad7c4a50c726eaeac71d3f2811eef19b3e1d6e2497aa84f2f47"},
        {"muonic v branston game 2.KWReplay",
         "86818c436dadc108f2f98f42e6906bdd179ffe867ad33478fbd532b1c0021ed0"},
        {"muonic v branston game 3.KWReplay",
         "c9fcce425fa470117993061e7bbb01e249381931955251c970945d92fee00eac"},
        {"muonic v branston game 4.KWReplay",
         "464a0a40450d87271b85b972edfbf13ba2e8094930f32cc39dc04d627d62765d"},
        {"muonic v branston game 5.KWReplay",
         "936de4db8ca28174606ea59e3b8d8b9f571bf500200644398af691525f5936ea"},
        {"test_bhs.KWReplay",
         "a4c065b83206c9ce00b399cdc41b04fe541c8b60d5669b11ea76467da33eaed8"},
        {"test_gdis.KWReplay",
         "f7acd175821314e173b7efdf954a05ef68e6a145695880ddbd4c7bd347d0ae8e"},
        {"test_moks.KWReplay",
         "919280bf2d2575f472c72e7bb8c2f1bb8b1d5ba68fb01f1ab84448d45320afac"},
        {"test_nods.KWReplay",
         "85e3d21a588226e321d9231f76de12a79111cc1ba1340aa244213e960f9dfb91"},
        {"test_reapers.KWReplay",
         "517881988a80d98c5d3eea9e548311d2e7bfa21c7228cae54ca07f01d7935b4c"},
        {"test_scrins.KWReplay",
         "59f023a151f7a0d5cc9d93153e0e43746e981cb81a5529fcd535b1c904d84e33"},
        {"test_sts.KWReplay",
         "4a9cd7e00860771e1ffd7cee721b0cfc1fbdb193d3e4d78e03e52b351f0a8a91"},
        {"test_travelers.KWReplay",
         "fea536a932459eb45064b0ba18cfa44027a356558c0451bf93b2751412759ae5"},
        {"test_zocoms.KWReplay",
         "e9c564e3285b93cea11feaed787a07995b96b04d2c3a3f1145faebd811ac75e0"},
    };

    for (const auto& fixture : fixtures) {
        INFO("fixture: " << fixture.filename);
        const ReplaySynopsis metadata =
            parseReplay(QString::fromUtf8(fixture.filename));
        CHECK(metadata.checksum.toHex() == QByteArray(fixture.expectedHex));
    }
}

TEST_CASE("parses muonic v branston game 1", "[legionparser][metadata]") {
    const ReplaySynopsis metadata =
        parseReplay(QString::fromUtf8("muonic v branston game 1.KWReplay"));

    CHECK(metadata.gameType == GameType::Multiplayer);
    CHECK(metadata.matchTitle == QLatin1String("scrub"));
    CHECK(metadata.matchDescription == QLatin1String("No Match Description"));
    CHECK(metadata.mapName == QLatin1String("[WEC] Matter of Time"));
    CHECK(metadata.mapId == QLatin1String("FakeMapID"));

    REQUIRE(metadata.players.size() == 3);
    CHECK(metadata.players.at(0).name == QLatin1String("Scrub"));
    CHECK(metadata.players.at(0).faction == Faction::BH);
    CHECK(metadata.players.at(1).name == QLatin1String("Muonic"));
    CHECK(metadata.players.at(1).faction == Faction::Nod);
    CHECK(metadata.players.at(0).teamNumber !=
          metadata.players.at(1).teamNumber);
    CHECK(metadata.players.at(1).isReplaySaver);
    CHECK(metadata.players.at(2).faction == Faction::Unknown);

    // 7/8/2026 4:31:09 PM GMT
    QDateTime timestamp(QDate(2026, 7, 8), QTime(16, 31, 9),
                        QTimeZone(QTimeZone::UTC));
    CHECK(metadata.timestamp == timestamp);
}

TEST_CASE("faction: GDI mirror match", "[legionparser][faction]") {
    checkMirrorMatchFaction(QString::fromUtf8("test_gdis.KWReplay"),
                            Faction::GDI);
}

TEST_CASE("faction: ST mirror match", "[legionparser][faction]") {
    checkMirrorMatchFaction(QString::fromUtf8("test_sts.KWReplay"),
                            Faction::ST);
}

TEST_CASE("faction: ZOCOM mirror match", "[legionparser][faction]") {
    checkMirrorMatchFaction(QString::fromUtf8("test_zocoms.KWReplay"),
                            Faction::ZOCOM);
}

TEST_CASE("faction: Nod mirror match", "[legionparser][faction]") {
    checkMirrorMatchFaction(QString::fromUtf8("test_nods.KWReplay"),
                            Faction::Nod);
}

TEST_CASE("faction: BH mirror match", "[legionparser][faction]") {
    checkMirrorMatchFaction(QString::fromUtf8("test_bhs.KWReplay"),
                            Faction::BH);
}

TEST_CASE("faction: MoK mirror match", "[legionparser][faction]") {
    checkMirrorMatchFaction(QString::fromUtf8("test_moks.KWReplay"),
                            Faction::MoK);
}

TEST_CASE("faction: Scrin mirror match", "[legionparser][faction]") {
    checkMirrorMatchFaction(QString::fromUtf8("test_scrins.KWReplay"),
                            Faction::Scrin);
}

TEST_CASE("faction: Reaper mirror match", "[legionparser][faction]") {
    checkMirrorMatchFaction(QString::fromUtf8("test_reapers.KWReplay"),
                            Faction::Reaper);
}

TEST_CASE("faction: Traveler mirror match", "[legionparser][faction]") {
    checkMirrorMatchFaction(QString::fromUtf8("test_travelers.KWReplay"),
                            Faction::Traveler);
}

// Skirmish replays carry no binary team_number field, so team membership is
// inferred from the S= slot text instead (see
// SynopsisParser::parsePlayerSlots). These replays were purpose-built to pin
// that inference down: each sets up a known, deliberately configured team split
// so the inferred grouping can be checked against ground truth.

TEST_CASE("team inference: implicit skirmish alliance",
          "[legionparser][team]") {
    // 4-player skirmish.KWReplay: 1 human + 3 Brutal AI, allied via the
    // in-game diplomacy UI (no explicit numeric team chosen). Muonic and the
    // second bot are on one side; the first and third bots are on the other.
    const ReplaySynopsis metadata =
        parseReplay(QString::fromUtf8("4-player skirmish.KWReplay"));

    REQUIRE(metadata.players.size() == 5);
    CHECK(metadata.players.at(0).name == QLatin1String("Muonic"));
    CHECK(metadata.players.at(0).teamNumber ==
          metadata.players.at(2).teamNumber);
    CHECK(metadata.players.at(1).teamNumber ==
          metadata.players.at(3).teamNumber);
    CHECK(metadata.players.at(0).teamNumber !=
          metadata.players.at(1).teamNumber);
}

TEST_CASE("team inference: alliance across different AI difficulties",
          "[legionparser][team]") {
    // 4-player different versions.KWReplay: Easy/Medium/Hard AI bots give
    // each slot an unambiguous identity independent of S= ordering. Muonic
    // is allied with the Easy bot; Medium and Hard are allied with each
    // other.
    const ReplaySynopsis metadata =
        parseReplay(QString::fromUtf8("4-player different versions.KWReplay"));

    REQUIRE(metadata.players.size() == 5);
    CHECK(metadata.players.at(0).name == QLatin1String("Muonic"));
    CHECK(metadata.players.at(1).name == QLatin1String("Easy AI"));
    CHECK(metadata.players.at(2).name == QLatin1String("Medium AI"));
    CHECK(metadata.players.at(3).name == QLatin1String("Hard AI"));

    CHECK(metadata.players.at(0).teamNumber ==
          metadata.players.at(1).teamNumber);
    CHECK(metadata.players.at(2).teamNumber ==
          metadata.players.at(3).teamNumber);
    CHECK(metadata.players.at(0).teamNumber !=
          metadata.players.at(2).teamNumber);
}

TEST_CASE("team inference: explicit numeric team assignment",
          "[legionparser][team]") {
    // 4-player team 4.KWReplay: same lineup as above, but Muonic and the
    // Easy bot were explicitly put on lobby "team 4", and Medium/Hard on
    // lobby "team 2". The S= value is 0-based (UI team - 1), so this also
    // pins down that +1 conversion back to the number shown in the UI.
    const ReplaySynopsis metadata =
        parseReplay(QString::fromUtf8("4-player team 4.KWReplay"));

    REQUIRE(metadata.players.size() == 5);
    CHECK(metadata.players.at(0).name == QLatin1String("Muonic"));
    CHECK(metadata.players.at(1).name == QLatin1String("Easy AI"));
    CHECK(metadata.players.at(2).name == QLatin1String("Medium AI"));
    CHECK(metadata.players.at(3).name == QLatin1String("Hard AI"));

    CHECK(metadata.players.at(0).teamNumber == 4);
    CHECK(metadata.players.at(1).teamNumber == 4);
    CHECK(metadata.players.at(2).teamNumber == 2);
    CHECK(metadata.players.at(3).teamNumber == 2);
}

TEST_CASE("team inference: FFA slots are never treated as allied",
          "[legionparser][team]") {
    // 4-player ffa.KWReplay: same lineup again, but nobody was assigned a
    // team - every slot's S= value is the -1 "no team" sentinel. Naively
    // grouping by equal teamNumber would wrongly merge all four into one
    // team, since they'd all share that -1; each must instead get a
    // distinct placeholder so ReplayModel's equality-based grouping doesn't
    // merge unrelated FFA players.
    const ReplaySynopsis metadata =
        parseReplay(QString::fromUtf8("4-player ffa.KWReplay"));

    REQUIRE(metadata.players.size() == 5);
    QSet<std::uint32_t> teamNumbers;
    // Excludes the trailing commentator slot, which is a distinct real
    // player as far as this invariant is concerned, but is not a
    // participant we're asserting names/positions for here.
    for (qsizetype i = 0; i < 4; i++) {
        teamNumbers.insert(metadata.players.at(i).teamNumber);
    }
    CHECK(teamNumbers.size() == 4);
}

TEST_CASE(
    "team inference: a completely full lobby leaves the commentator slot "
    "at its defaults",
    "[legionparser][team]") {
    // 8-player all random ffa.KWReplay: all 8 real slots are filled, so
    // there's no leftover ;S= entry for the trailing synthetic commentator
    // player - this is the replay that originally surfaced
    // SynopsisParser::parsePlayerSlots() indexing into an empty slot view once
    // the header ran out of slot text before the player list did.
    const ReplaySynopsis metadata =
        parseReplay(QString::fromUtf8("8-player all random ffa.KWReplay"));

    REQUIRE(metadata.players.size() == 9);
    CHECK(metadata.players.at(7).name == QLatin1String("Muonic"));

    QSet<std::uint32_t> teamNumbers;
    for (qsizetype i = 0; i < 8; i++) {
        CHECK_FALSE(metadata.players.at(i).isComputer);
        teamNumbers.insert(metadata.players.at(i).teamNumber);
    }
    CHECK(teamNumbers.size() == 8);

    // parsePlayerSlots stops before ever reaching the commentator, so it's
    // left at Player's documented "parsing hasn't reached this player yet"
    // defaults rather than a crash or fabricated data.
    const Player& commentator = metadata.players.at(8);
    CHECK(commentator.teamNumber == std::numeric_limits<std::uint32_t>::max());
    CHECK(commentator.faction == Faction::Unknown);
    CHECK_FALSE(commentator.isComputer);
}
