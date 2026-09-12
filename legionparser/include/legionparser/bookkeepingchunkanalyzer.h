/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <legionparser/chunkanalyzer.h>

#include <QByteArray>
#include <QList>
#include <QtTypes>
#include <cstddef>
#include <span>
#include <utility>

namespace LegionParser {

// One entry per chunk observed by BookkeepingChunkAnalyzer.
struct ChunkRecord {
    quint32 timecode;
    ChunkType type;
    qsizetype size;
};

/**
 * A generic diagnostic ChunkAnalyzer: records every chunk it sees (type,
 * timecode, size) and, for one chosen chunk type, retains raw copies of the
 * most recent N chunks of that type - useful for tools that want to inspect
 * or dump replay traffic without decoding it.
 *
 * The span passed to chunk() is only valid for the duration of that call;
 * anything retained here is an explicit copy.
 */
class BookkeepingChunkAnalyzer : public ChunkAnalyzer {
   public:
    // retainedRawChunkCount: how many of the most recent chunks of
    // retainedType to keep raw copies of (0 disables raw retention).
    explicit BookkeepingChunkAnalyzer(
        qsizetype retainedRawChunkCount = 0,
        ChunkType retainedType = ChunkType::Command);

    bool chunk(qsizetype chunkStartOffset, quint32 timecode, ChunkType type,
               std::span<const std::byte> buf) override;

    [[nodiscard]] const QList<ChunkRecord>& chunkLog() const;
    // (timecode, raw bytes) pairs for the most recent retainedRawChunkCount
    // chunks of retainedType, oldest first.
    [[nodiscard]] const QList<std::pair<quint32, QByteArray>>&
    recentRawChunks() const;

   private:
    QList<ChunkRecord> m_chunkLog;
    QList<std::pair<quint32, QByteArray>> m_recentRaw;
    qsizetype m_retainedRawChunkCount;
    ChunkType m_retainedType;
};

}  // namespace LegionParser
