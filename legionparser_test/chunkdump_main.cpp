// SPDX-License-Identifier: GPL-3.0-or-later
// Scratch tool: not part of the shipped product. Walks a replay's body
// chunk-by-chunk and decodes Type-1 (command) chunks via
// CommandFrameAnalyzer / BroadcastChunkAnalyzer / BookkeepingChunkAnalyzer,
// to look for a plausible victory/defeat signal near the end of the stream.

#include <legionparser/analysisparser.h>
#include <legionparser/bookkeepingchunkanalyzer.h>
#include <legionparser/broadcastchunkanalyzer.h>
#include <legionparser/chunkanalyzer.h>
#include <legionparser/exception.h>
#include <legionparser/replay.h>
#include <legionparser/synopsisparser.h>

#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <algorithm>
#include <cstdio>
#include <map>
#include <vector>

using namespace LegionParser;

namespace {

struct CommandRecord {
    quint32 timeCode;
    quint8 rawPlayer;
    int mangledPlayer;
    quint8 cmdId;
    qsizetype length;
    QByteArray raw;
};

const char* desyncTriggerName(DesyncTrigger cause) {
    switch (cause) {
        case DesyncTrigger::IncompleteFixedLengthCommand:
            return "incomplete fixed-length cmd";
        case DesyncTrigger::IncompleteStandardCommand:
            return "incomplete standard-layout cmd";
        case DesyncTrigger::IncompleteBespokeCommand:
            return "incomplete special cmd";
        case DesyncTrigger::UnrecognizedCommand:
            return "unknown cmd";
        case DesyncTrigger::InvalidScanResult:
            return "internal: invalid scan result";
    }
    return "?";
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        std::printf("usage: chunkdump <file>\n");
        return 1;
    }
    const QString path = QString::fromLocal8Bit(argv[1]);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        std::printf("open failed\n");
        return 1;
    }

    ReplaySynopsis syn;
    try {
        syn = SynopsisParser::parse(file);
    } catch (const std::exception& e) {
        std::printf("parse failed: %s\n", e.what());
        return 1;
    }

    std::printf("== %s ==\n", argv[1]);
    for (qsizetype i = 0; i < syn.players.size(); ++i) {
        const auto& p = syn.players[i];
        std::printf(
            "  slot %lld: id=%u name=\"%s\" faction=%d computer=%d\n",
            static_cast<long long>(i), p.id, p.name.toUtf8().constData(),
            static_cast<int>(p.faction), p.isComputer);
    }
    std::printf("  engineTicks(maxTimeCode)=%u\n", syn.engineTicks);

    std::vector<CommandRecord> commands;

    auto chunkFn = [&commands](quint32 timecode, QSpan<Command> cmds) -> bool {
        for (const auto& cmd : cmds) {
            QByteArray raw;
            if (cmd.commandId == 0x2D) {
                // cmd.bytes excludes the 2-byte commandId/player header;
                // prepend it back so the hex dump is the full raw command.
                raw.append(static_cast<char>(cmd.commandId));
                raw.append(static_cast<char>(cmd.mangledPlayerIdx));
                raw.append(reinterpret_cast<const char*>(cmd.bytes.data()),
                          static_cast<qsizetype>(cmd.bytes.size()));
            }
            // cmd.bytes excludes the 2-byte commandId/player header, so the
            // total consumed length is the payload size plus that header.
            const qsizetype length =
                static_cast<qsizetype>(cmd.bytes.size()) + 2;
            // -1 is a display-only sentinel for "not a valid player index"
            // (raw byte below 24) - chunkdump just prints it, no decoding
            // logic depends on this value.
            const auto ordinal = unmanglePlayerIdx(cmd.mangledPlayerIdx);
            commands.push_back(
                {timecode, cmd.mangledPlayerIdx,
                 ordinal ? static_cast<int>(*ordinal) : -1, cmd.commandId,
                 length, raw});
        }
        return true;
    };

    auto errorFn = [](DesyncDetails d) {
        std::printf(
            "    [%s 0x%02X at chunkOffset %lld, relOffset %lld]\n",
            desyncTriggerName(d.cause), d.command,
            static_cast<long long>(d.chunkOffset),
            static_cast<long long>(d.relativeCommandOffset));
    };

    BookkeepingChunkAnalyzer bookkeeping(5, ChunkType::Command);
    CommandFrameAnalyzer commandFrame(chunkFn, errorFn);
    BroadcastChunkAnalyzer broadcast{bookkeeping, commandFrame};

    try {
        analyzeReplay(file, syn.bodyOffset, broadcast);
    } catch (const std::exception& e) {
        std::printf("chunk walk failed: %s\n", e.what());
        return 1;
    }

    const auto& chunkLog = bookkeeping.chunkLog();

    std::printf("  -- raw bytes of last 5 Type-1 chunks --\n");
    for (const auto& [tc, data] : bookkeeping.recentRawChunks()) {
        std::printf("    tc=%u len=%lld hex=", tc,
                   static_cast<long long>(data.size()));
        for (unsigned char b : data) {
            std::printf("%02X ", b);
        }
        std::printf("\n");
    }

    quint32 maxTimeCode = 0;
    for (const auto& record : chunkLog) {
        maxTimeCode = std::max(maxTimeCode, record.timecode);
    }
    std::printf(
        "  total chunks=%lld, total commands decoded=%zu, maxTimeCode=%u\n",
        static_cast<long long>(chunkLog.size()), commands.size(),
        maxTimeCode);

    std::map<int, quint32> lastSeen;
    std::map<int, int> counts;
    for (const auto& c : commands) {
        auto& seen = lastSeen[c.mangledPlayer];
        seen = std::max(seen, c.timeCode);
        counts[c.mangledPlayer]++;
    }
    for (const auto& [pid, tc] : lastSeen) {
        std::printf(
            "  player[mangled=%d]: lastCommandTimeCode=%u, totalCommands=%d\n",
            pid, tc, counts[pid]);
    }

    std::printf("  -- all 0x2D commands --\n");
    for (const auto& c : commands) {
        if (c.cmdId != 0x2D) {
            continue;
        }
        std::printf("    tc=%u player_raw=%u(mangled=%d) len=%lld hex=",
                   c.timeCode, c.rawPlayer, c.mangledPlayer,
                   static_cast<long long>(c.length));
        for (unsigned char b : c.raw) {
            std::printf("%02X ", b);
        }
        std::printf("\n");
    }

    std::printf("  -- tail commands --\n");
    const size_t start = commands.size() > 80 ? commands.size() - 80 : 0;
    for (size_t i = start; i < commands.size(); ++i) {
        const auto& c = commands[i];
        std::printf("    tc=%u player_raw=%u(mangled=%d) cmd=0x%02X len=%lld\n",
                   c.timeCode, c.rawPlayer, c.mangledPlayer, c.cmdId,
                   static_cast<long long>(c.length));
    }

    std::map<ChunkType, int> typeCounts;
    for (const auto& record : chunkLog) {
        typeCounts[record.type]++;
    }
    std::printf("  -- chunk type counts --\n");
    for (const auto& [type, count] : typeCounts) {
        std::printf("    type=%d count=%d\n", static_cast<int>(type), count);
    }

    std::printf("  -- tail chunks (any type) --\n");
    const qsizetype cstart =
        chunkLog.size() > 20 ? chunkLog.size() - 20 : 0;
    for (qsizetype i = cstart; i < chunkLog.size(); ++i) {
        const auto& record = chunkLog[i];
        std::printf("    tc=%u type=%d len=%lld\n", record.timecode,
                   static_cast<int>(record.type),
                   static_cast<long long>(record.size));
    }

    return 0;
}
