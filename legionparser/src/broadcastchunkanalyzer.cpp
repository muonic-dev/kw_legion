/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <legionparser/broadcastchunkanalyzer.h>
#include <legionparser/chunkanalyzer.h>

#include <QtTypes>
#include <functional>

namespace LegionParser {
BroadcastChunkAnalyzer::BroadcastChunkAnalyzer(
    std::initializer_list<std::reference_wrapper<ChunkAnalyzer>> analyzers)
    : m_analyzers(analyzers) {}

void BroadcastChunkAnalyzer::begin() {
    // If begin called multiple times comes up, we may need to track the
    // unloaded analyzers so we can re-begin
    for (auto analyzer : m_analyzers) {
        analyzer.get().begin();
    }
}

bool BroadcastChunkAnalyzer::chunk(qsizetype chunkStartOffset, quint32 timecode,
                                   ChunkType type,
                                   std::span<const std::byte> buf) {
    for (auto it = m_analyzers.begin(); it != m_analyzers.end();) {
        if (!it->get().chunk(chunkStartOffset, timecode, type, buf)) {
            it->get().finalize();
            it = m_analyzers.erase(it);
        } else {
            it++;
        }
    }
    return !m_analyzers.empty();
}

void BroadcastChunkAnalyzer::finalize() {
    for (auto analyzer : m_analyzers) {
        analyzer.get().finalize();
    }
}
}  // namespace LegionParser