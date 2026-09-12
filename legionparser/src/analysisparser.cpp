/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <legionparser/analysisparser.h>

#include <QIODevice>
#include <QtTypes>
#include <cstddef>
#include <optional>
#include <span>

#include "reader.h"

namespace LegionParser {

void analyzeReplay(QIODevice& replayFile, qsizetype bodyOffset,
                   ChunkAnalyzer& analyzer) {
    replayFile.seek(bodyOffset);
    Reader reader{replayFile};

    analyzer.begin();

    qsizetype chunkStart = reader.offset();
    std::optional<BodyChunk> chunk = reader.readBodyChunk();
    while (chunk) {
        // We do all further analysis in terms of std::span<const std::byte> due
        // to the fact that we do things like rely on uchar = 0xFF, etc.
        // QByteArray[View] is in terms of char. It is easier to get correct by
        // construction algorithsm.
        std::span<const std::byte> bytes = std::as_bytes(std::span<const char>(
            chunk->data.data(), static_cast<std::size_t>(chunk->data.size())));
        if (!analyzer.chunk(chunkStart, chunk->timeCode, chunk->type, bytes)) {
            break;
        }
        chunkStart = reader.offset();
        chunk = reader.readBodyChunk();
    }
    analyzer.finalize();
}

}  // namespace LegionParser