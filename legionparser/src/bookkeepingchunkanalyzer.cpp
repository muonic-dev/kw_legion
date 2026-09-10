/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <legionparser/bookkeepingchunkanalyzer.h>

namespace LegionParser {

BookkeepingChunkAnalyzer::BookkeepingChunkAnalyzer(
    qsizetype retainedRawChunkCount, ChunkType retainedType)
    : m_retainedRawChunkCount(retainedRawChunkCount),
      m_retainedType(retainedType) {}

bool BookkeepingChunkAnalyzer::chunk(qsizetype chunkStartOffset,
                                     quint32 timecode, ChunkType type,
                                     std::span<const std::byte> buf) {
    m_chunkLog.append(ChunkRecord{
        .timecode = timecode,
        .type = type,
        .size = static_cast<qsizetype>(buf.size()),
    });

    if (m_retainedRawChunkCount > 0 && type == m_retainedType) {
        m_recentRaw.append(
            {timecode, QByteArray(reinterpret_cast<const char*>(buf.data()),
                                  static_cast<qsizetype>(buf.size()))});
        while (m_recentRaw.size() > m_retainedRawChunkCount) {
            m_recentRaw.removeFirst();
        }
    }

    return true;
}

const QList<ChunkRecord>& BookkeepingChunkAnalyzer::chunkLog() const {
    return m_chunkLog;
}

const QList<std::pair<quint32, QByteArray>>&
BookkeepingChunkAnalyzer::recentRawChunks() const {
    return m_recentRaw;
}

}  // namespace LegionParser
