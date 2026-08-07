#include <legionparser/parser.h>

#include <QDir>
#include <QFile>
#include <QTimeZone>
#include <catch2/catch_test_macros.hpp>

using namespace LegionParser;

namespace {

ReplayMetadata parseReplay(const QString& filename) {
    const QString replayPath =
        QDir(QString::fromUtf8(REPLAY_TEST_DATA_DIR)).filePath(filename);
    QFile replayFile(replayPath);
    REQUIRE(replayFile.open(QIODevice::ReadOnly));
    return LegionParser::Parser::parse(replayFile);
}

// Each test_<faction>s.KWReplay is a skirmish where Muonic played a mirror
// match against a Hard AI opponent, so both players' declared factions
// should agree.
void checkMirrorMatchFaction(const QString& filename, Faction faction) {
    const ReplayMetadata metadata = parseReplay(filename);

    REQUIRE(metadata.players.size() == 3);

    CHECK(metadata.players.at(0).playerName == QString("Muonic"));
    CHECK(metadata.players.at(0).isReplaySaver);
    CHECK(metadata.players.at(0).faction == faction);

    CHECK(metadata.players.at(1).playerName == QString("Hard AI"));
    CHECK(metadata.players.at(1).faction == faction);

    // The trailing commentator slot's faction field isn't a real faction ID.
    CHECK(metadata.players.at(2).faction == Faction::Unknown);
}

}  // namespace

TEST_CASE("parses muonic v branston game 1", "[legionparser][metadata]") {
    const ReplayMetadata metadata =
        parseReplay(QString::fromUtf8("muonic v branston game 1.KWReplay"));

    CHECK(metadata.gameType == GameType::Multiplayer);
    CHECK(metadata.matchTitle == QString("scrub"));
    CHECK(metadata.matchDescription == QString("No Match Description"));
    CHECK(metadata.mapName == QString("[WEC] Matter of Time"));
    CHECK(metadata.mapId == QString("FakeMapID"));

    REQUIRE(metadata.players.size() == 3);
    CHECK(metadata.players.at(0).playerName == QString("Scrub"));
    CHECK(metadata.players.at(0).faction == Faction::BH);
    CHECK(metadata.players.at(1).playerName == QString("Muonic"));
    CHECK(metadata.players.at(1).faction == Faction::Nod);
    CHECK(metadata.players.at(0).teamNumber !=
          metadata.players.at(1).teamNumber);
    CHECK(metadata.players.at(1).isReplaySaver);
    CHECK(metadata.players.at(2).faction == Faction::Unknown);

    // 7/8/2026 4:31:09 PM GMT
    QDateTime replayTimestamp(QDate(2026, 7, 8), QTime(16, 31, 9),
                              QTimeZone(QTimeZone::UTC));
    CHECK(metadata.replayTimestamp == replayTimestamp);
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
