/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <legionparser/analysisparser.h>

#include <QIODevice>
#include <QtTypes>

#include "reader.h"

namespace LegionParser {

void analyzeReplay(QIODevice& replayFile, qsizetype bodyOffset,
                   ChunkAnalyzer& analyzer) {
    replayFile.seek(bodyOffset);
    Reader reader{replayFile};

    analyzer.begin();

    std::optional<BodyChunk> chunk = reader.readBodyChunk();
    while (chunk) {
        if (!analyzer.chunk(reader.lastOffset(), chunk->timeCode, chunk->type,
                            chunk->data)) {
            break;
        }
        chunk = reader.readBodyChunk();
    }
    analyzer.finalize();
}

}  // namespace LegionParser