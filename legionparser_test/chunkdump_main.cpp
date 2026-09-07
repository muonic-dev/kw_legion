// SPDX-License-Identifier: GPL-3.0-or-later
// Scratch tool: not part of the shipped product. Walks a replay's body
// chunk-by-chunk and decodes Type-1 (command) chunks using the KW
// command_id -> layout table ground-truthed from cnc3reader_impl.cpp, to
// look for a plausible victory/defeat signal near the end of the stream.

#include <legionparser/exception.h>
#include <legionparser/replay.h>
#include <legionparser/synopsisparser.h>

#include "reader.h"

#include <QByteArrayView>
#include <QCoreApplication>
#include <QFile>
#include <algorithm>
#include <cstdio>
#include <map>
#include <vector>

using namespace LegionParser;

namespace {

struct CommandRecord {
    qint32 timeCode;
    quint8 rawPlayer;
    int mangledPlayer;
    quint8 cmdId;
    qsizetype length;
    QByteArray raw;
};

const std::map<int, int> kKwCommands = {
    {0x01, -2}, {0x02, -2}, {0x03, -2}, {0x04, -2}, {0x05, -2}, {0x06, -2},
    {0x07, -2}, {0x08, -2}, {0x09, -2}, {0x0B, -2}, {0x0C, -2}, {0x0D, -2},
    {0x0F, -2}, {0x10, -2}, {0x11, -2}, {0x12, -2}, {0x17, -2},

    {0x27, -34}, {0x29, 28}, {0x2B, -11}, {0x2C, 17}, {0x2E, 22}, {0x2F, 17},
    {0x30, 17}, {0x34, 8}, {0x35, 12}, {0x36, 13}, {0x3C, 21}, {0x3E, 16},
    {0x3D, 21}, {0x43, 12}, {0x44, 8}, {0x45, 21}, {0x46, 16}, {0x47, 16},
    {0x48, 16}, {0x5B, 16}, {0x61, 20}, {0x72, -2}, {0x73, -2}, {0x77, 3},
    {0x7A, 29}, {0x7E, 12}, {0x7F, 12}, {0x87, 8}, {0x89, 8}, {0x8C, 45},
    {0x8D, 1049}, {0x8E, 16}, {0x8F, 40}, {0x90, 16}, {0x91, 10},

    {0x28, 0}, {0x2D, 0}, {0x31, 0}, {0x8B, 0},

    {0x26, -15}, {0x4C, -2}, {0x4D, -2}, {0x92, -2}, {0x93, -2}, {0xF5, -4},
    {0xF6, -4}, {0xF8, -4}, {0xF9, -2}, {0xFA, -2}, {0xFB, -2}, {0xFC, -2},
    {0xFD, -2}, {0xFE, -2}, {0xFF, -2},
};

int mangle(quint8 raw) { return static_cast<int>(raw) / 8 - 3; }

void decodeType1(qint32 timeCode, QByteArrayView payload,
                 std::vector<CommandRecord>& out) {
    const auto* buf = reinterpret_cast<const unsigned char*>(payload.data());
    const qsizetype chunklen = payload.size();
    if (chunklen < 5 || buf[0] != 1) {
        return;
    }
    qsizetype pos = 5;
    while (pos < chunklen) {
        const qsizetype opos = pos;
        if (opos + 2 > chunklen) {
            break;
        }
        const quint8 cmdId = buf[opos];
        const quint8 rawPlayer = buf[opos + 1];
        const auto it = kKwCommands.find(cmdId);
        if (it == kKwCommands.end()) {
            std::printf("    [unknown cmd 0x%02X at pos %lld, timecode %d]\n",
                       cmdId, static_cast<long long>(opos), timeCode);
            break;
        }
        // Bounds-checked probe: special-length formulas below index several
        // bytes ahead of opos before they know the command's real length, so
        // every such index must be checked against chunklen first - an
        // out-of-bounds probe here previously produced a bogus "extra
        // command" made of garbage heap bytes past the real payload.
        auto inBounds = [&](qsizetype idx) { return idx < chunklen; };
        bool oob = false;

        const int entry = it->second;
        if (entry > 0) {
            const qsizetype cmdLen = entry;
            if (opos + cmdLen > chunklen || buf[opos + cmdLen - 1] != 0xFF) {
                std::printf("    [panic fixed len cmd 0x%02X]\n", cmdId);
                break;
            }
            pos = opos + cmdLen;
        } else if (entry < 0) {
            const int k = -entry;
            pos = opos + k;
            while (pos < chunklen && buf[pos] != 0xFF) {
                const int adv = (buf[pos] >> 4) + 1;
                pos += 4 * adv + 1;
            }
            pos += 1;
        } else {
            if (cmdId == 0x31) {
                if (!inBounds(opos + 12)) {
                    oob = true;
                } else {
                    const int l = buf[opos + 12];
                    pos = opos + l * 18 + 17;
                }
            } else if (cmdId == 0x28) {
                if (!inBounds(opos + 17)) {
                    oob = true;
                } else {
                    pos = opos + (buf[opos + 17] + 1) * 4 + 32;
                }
            } else if (cmdId == 0x2D) {
                if (!inBounds(opos + 7)) {
                    oob = true;
                } else {
                    pos = opos + (buf[opos + 7] == 0xFF ? 8 : 26);
                }
            } else if (cmdId == 0x8B) {
                qsizetype p = opos;
                if (!inBounds(p + 3)) {
                    oob = true;
                } else {
                    const int l1 = buf[p + 3];
                    p += l1 + 5;
                    if (!inBounds(p)) {
                        oob = true;
                    } else {
                        const int l2 = buf[p];
                        p += 2 * l2 + 2;
                        p += 5;
                        pos = p;
                    }
                }
            }
        }
        if (oob || pos > chunklen) {
            std::printf(
                "    [out-of-bounds special cmd 0x%02X at pos %lld, "
                "timecode %d, chunklen %lld]\n",
                cmdId, static_cast<long long>(opos), timeCode,
                static_cast<long long>(chunklen));
            break;
        }
        QByteArray raw;
        if (cmdId == 0x2D) {
            raw = QByteArray(reinterpret_cast<const char*>(buf + opos),
                             static_cast<int>(pos - opos));
        }
        out.push_back({timeCode, rawPlayer, mangle(rawPlayer), cmdId,
                       pos - opos, raw});
        if (pos <= opos) {
            break;
        }
    }
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

    file.seek(syn.bodyOffset);
    Reader reader(file);
    std::vector<CommandRecord> commands;
    qint32 maxTimeCode = 0;
    std::vector<std::tuple<qint32, ChunkType, qsizetype>> chunkLog;
    std::vector<std::tuple<qint32, QByteArray>> lastCommandChunks;
    while (true) {
        auto chunk = reader.readBodyChunk();
        if (!chunk) {
            break;
        }
        chunkLog.emplace_back(chunk->timeCode, chunk->type,
                              chunk->data.size());
        if (chunk->type == ChunkType::Command) {
            decodeType1(chunk->timeCode, QByteArrayView(chunk->data),
                       commands);
            lastCommandChunks.emplace_back(chunk->timeCode, chunk->data);
            if (lastCommandChunks.size() > 5) {
                lastCommandChunks.erase(lastCommandChunks.begin());
            }
        }
        if (chunk->timeCode > maxTimeCode) {
            maxTimeCode = chunk->timeCode;
        }
    }

    std::printf("  -- raw bytes of last 5 Type-1 chunks --\n");
    for (const auto& [tc, data] : lastCommandChunks) {
        std::printf("    tc=%d len=%lld hex=", tc,
                   static_cast<long long>(data.size()));
        for (unsigned char b : data) {
            std::printf("%02X ", b);
        }
        std::printf("\n");
    }
    std::printf("  total chunks=%zu, total commands decoded=%zu, maxTimeCode=%d\n",
               chunkLog.size(), commands.size(), maxTimeCode);

    std::map<int, qint32> lastSeen;
    std::map<int, int> counts;
    for (const auto& c : commands) {
        auto& seen = lastSeen[c.mangledPlayer];
        seen = std::max(seen, c.timeCode);
        counts[c.mangledPlayer]++;
    }
    for (const auto& [pid, tc] : lastSeen) {
        std::printf(
            "  player[mangled=%d]: lastCommandTimeCode=%d, totalCommands=%d\n",
            pid, tc, counts[pid]);
    }

    std::printf("  -- all 0x2D commands --\n");
    for (const auto& c : commands) {
        if (c.cmdId != 0x2D) {
            continue;
        }
        std::printf("    tc=%d player_raw=%u(mangled=%d) len=%lld hex=",
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
        std::printf("    tc=%d player_raw=%u(mangled=%d) cmd=0x%02X len=%lld\n",
                   c.timeCode, c.rawPlayer, c.mangledPlayer, c.cmdId,
                   static_cast<long long>(c.length));
    }

    std::printf("  -- tail chunks (any type) --\n");
    const size_t cstart = chunkLog.size() > 20 ? chunkLog.size() - 20 : 0;
    for (size_t i = cstart; i < chunkLog.size(); ++i) {
        const auto& [tc, type, len] = chunkLog[i];
        std::printf("    tc=%d type=%d len=%lld\n", tc,
                   static_cast<int>(type), static_cast<long long>(len));
    }

    return 0;
}
