// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include <legionparser/analysisparser.h>
#include <legionparser/chunkanalyzer.h>
#include <legionparser/synopsisparser.h>

#include <QDir>
#include <QFile>
#include <catch2/catch_test_macros.hpp>
#include <map>

using namespace LegionParser;

namespace {

QString replayPath(const QString& filename) {
    return QDir(QString::fromUtf8(REPLAY_TEST_DATA_DIR)).filePath(filename);
}

// Tallies every chunk analyzeReplay hands it, by ChunkType. Doesn't decode
// anything inside the payload - this only exercises the walker/framing
// (analyzeReplay + Reader), not command-level decoding.
class CountingAnalyzer : public ChunkAnalyzer {
   public:
    bool chunk(qsizetype /*chunkStart*/, qint32 /*timecode*/, ChunkType type,
               QByteArrayView /*payload*/) override {
        ++m_counts[type];
        return true;
    }

    [[nodiscard]] int count(ChunkType type) const {
        const auto it = m_counts.find(type);
        return it == m_counts.end() ? 0 : it->second;
    }

    [[nodiscard]] int total() const {
        int sum = 0;
        for (const auto& [type, count] : m_counts) {
            sum += count;
        }
        return sum;
    }

   private:
    std::map<ChunkType, int> m_counts;
};

void countChunkTypes(const QString& filename, ChunkAnalyzer& analyzer) {
    QFile replayFile(replayPath(filename));
    REQUIRE(replayFile.open(QIODevice::ReadOnly));
    const ReplaySynopsis synopsis = SynopsisParser::parse(replayFile);
    analyzeReplay(replayFile, synopsis.bodyOffset, analyzer);
}

// Counts how many chunks it's handed before stopping - used to verify
// analyzeReplay honors an analyzer's early-termination (chunk() returning
// false) contract rather than always walking the whole body.
class StoppingAnalyzer : public ChunkAnalyzer {
   public:
    explicit StoppingAnalyzer(int stopAfter) : m_stopAfter(stopAfter) {}

    bool chunk(qsizetype /*chunkStart*/, qint32 /*timecode*/,
               ChunkType /*type*/, QByteArrayView /*payload*/) override {
        ++m_seen;
        return m_seen < m_stopAfter;
    }

    [[nodiscard]] int seen() const { return m_seen; }

   private:
    int m_stopAfter;
    int m_seen = 0;
};

}  // namespace

// Expected counts below came from walking each replay's body with the
// legionparser_chunkdump scratch tool and tallying ChunkType per chunk -
// see legionparser_test/chunkdump_main.cpp's "chunk type counts" output.
// None of these replays carry commentary, so Type3/Type4 are expected to be
// zero throughout - a non-zero count there would mean either a replay with
// hasCommentary showed up unexpectedly, or a real framing bug.

TEST_CASE("analyzeReplay counts chunk types for a 1v1 match",
          "[legionparser][analysis]") {
    CountingAnalyzer counter;
    countChunkTypes(QString::fromUtf8("muonic v branston game 1.KWReplay"),
                    counter);

    CHECK(counter.count(ChunkType::Command) == 837);
    CHECK(counter.count(ChunkType::Camera) == 2166);
    CHECK(counter.count(ChunkType::Type3) == 0);
    CHECK(counter.count(ChunkType::Type4) == 0);
    CHECK(counter.total() == 3003);
}

TEST_CASE("analyzeReplay counts chunk types for a short 4-player ffa",
          "[legionparser][analysis]") {
    CountingAnalyzer counter;
    countChunkTypes(QString::fromUtf8("4-player ffa.KWReplay"), counter);

    CHECK(counter.count(ChunkType::Command) == 2);
    CHECK(counter.count(ChunkType::Camera) == 4);
    CHECK(counter.count(ChunkType::Type3) == 0);
    CHECK(counter.count(ChunkType::Type4) == 0);
    CHECK(counter.total() == 6);
}

TEST_CASE("analyzeReplay counts chunk types for an 8-player ffa",
          "[legionparser][analysis]") {
    CountingAnalyzer counter;
    countChunkTypes(QString::fromUtf8("8-player all random ffa.KWReplay"),
                    counter);

    CHECK(counter.count(ChunkType::Command) == 2570);
    CHECK(counter.count(ChunkType::Camera) == 12044);
    CHECK(counter.count(ChunkType::Type3) == 0);
    CHECK(counter.count(ChunkType::Type4) == 0);
    CHECK(counter.total() == 14614);
}

TEST_CASE("analyzeReplay stops as soon as the analyzer returns false",
          "[legionparser][analysis]") {
    StoppingAnalyzer stopper{5};
    QFile replayFile(
        replayPath(QString::fromUtf8("muonic v branston game 1.KWReplay")));
    REQUIRE(replayFile.open(QIODevice::ReadOnly));
    const ReplaySynopsis synopsis = SynopsisParser::parse(replayFile);

    analyzeReplay(replayFile, synopsis.bodyOffset, stopper);

    CHECK(stopper.seen() == 5);
}
