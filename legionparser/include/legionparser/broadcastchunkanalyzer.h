/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <QtTypes>

#include "chunkanalyzer.h"

namespace LegionParser {
/**
 * A chunk analyzer that multicasts to multiple internal analyzers
 *
 * Allows multiple analyzers to be driven in 1 pass
 */
class BroadcastChunkAnalyzer : public ChunkAnalyzer {
   public:
    explicit BroadcastChunkAnalyzer(
        std::initializer_list<std::reference_wrapper<ChunkAnalyzer>> analyzers);

    void begin() override;

    bool chunk(qsizetype chunkStartOffset, quint32 timecode, ChunkType type,
               std::span<const std::byte> buf) override;

    void finalize() override;

   private:
    std::vector<std::reference_wrapper<ChunkAnalyzer>> m_analyzers;
};
}  // namespace LegionParser