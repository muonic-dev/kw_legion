#include <legionparser/parser.h>

#include <QDir>
#include <QFile>
#include <QTimeZone>
#include <catch2/catch_test_macros.hpp>

using namespace LegionParser;

namespace {

class HeaderCaptureListener : public ParserEventListener {
   public:
    void onHeaderParsed(const ReplayMetadata& metadata) override {
        m_metadata = metadata;
    }

    ReplayMetadata m_metadata;
};

}  // namespace
TEST_CASE("parses muonic v branston game 1", "[legionparser]") {
    const QString replayPath =
        QDir(QString::fromUtf8(REPLAY_TEST_DATA_DIR))
            .filePath(QString::fromUtf8("muonic v branston game 1.KWReplay"));
    QFile replayFile(replayPath);
    REQUIRE(replayFile.open(QIODevice::ReadOnly));

    HeaderCaptureListener listener;
    LegionParser::Parser::parse(replayFile, listener);

    CHECK(listener.m_metadata.gameType == GameType::Multiplayer);
    CHECK(listener.m_metadata.matchTitle == QString("scrub"));
    CHECK(listener.m_metadata.matchDescription ==
          QString("No Match Description"));
    CHECK(listener.m_metadata.mapName == QString("[WEC] Matter of Time"));
    CHECK(listener.m_metadata.mapId == QString("FakeMapID"));

    CHECK(listener.m_metadata.players.size() == 2);
    CHECK(listener.m_metadata.players.at(0).playerName == QString("Scrub"));
    CHECK(listener.m_metadata.players.at(1).playerName == QString("Muonic"));
    CHECK(listener.m_metadata.players.at(0).teamNumber !=
          listener.m_metadata.players.at(1).teamNumber);
    CHECK(listener.m_metadata.players.at(1).isReplaySaver);

    // 7/8/2026 4:31:09 PM GMT
    QDateTime replayTimestamp(QDate(2026, 7, 8), QTime(16, 31, 9),
                              QTimeZone(QTimeZone::UTC));
    CHECK(listener.m_metadata.replayTimestamp == replayTimestamp);
}
