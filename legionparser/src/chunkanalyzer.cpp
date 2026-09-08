/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <legionparser/chunkanalyzer.h>

#include <QByteArray>
#include <QtTypes>

namespace LegionParser {
void ChunkAnalyzer::begin() {}

void ChunkAnalyzer::finalize() {}

bool CommandChunkParser::chunk(qsizetype chunkOffset, qint32 timecode,
                               ChunkType type, QByteArrayView payload) {
    if (type == ChunkType::Command) {
        commandChunk(timecode, payload);
    }
    return true;
}

void CommandChunkParser::finalize() {}

void CommandChunkParser::commandChunk(qint32 timecode, QByteArrayView payload) {
    // Immediately copy since we are going to be making lots of views into it
    QByteArray chunkBs = payload.toByteArray();
}
}  // namespace LegionParser
