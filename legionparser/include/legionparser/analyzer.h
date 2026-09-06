/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <QByteArrayView>
#include <QtTypes>
#include <cstdint>


namespace LegionParser {

/**
 * @brief The frame type of a replay chunk
 *
 * <ul>
 * <li>Type 1 - Player commands
 * <li>Type 2 - Player/observer/commentator camera motions
 * <li>Type 3 - Unknown
 * <li>Type 4 - Unknown
 * </ul>
 */
enum class ChunkType : std::uint8_t { Comman = 1, Camera, Type3, Type4 };

/**
 * Interface for receiving chunks
 *
 */
class ChunkAnalyzer {
   public:
    // Pure interface, no move/copy
    ChunkAnalyzer(const ChunkAnalyzer&) = delete;
    ChunkAnalyzer(ChunkAnalyzer&&) = delete;
    ChunkAnalyzer& operator=(const ChunkAnalyzer&) = delete;
    ChunkAnalyzer& operator=(ChunkAnalyzer&&) = delete;

    virtual ~ChunkAnalyzer() = default;

    /**
     * Prepare for receive calls.
     *
     * Implementors should ensure that begin being called multiple times is not
     * an error. If the stream desyncs because our fast path jump to body offset
     * logic is incorrect, then the fallback will call begin again.
     */
    virtual void begin();

    /**
     * @brief Analyze a single chunk
     *
     * @return false when the analyzer is uninterested in more data
     */
    virtual bool chunk(qint32 timecode, ChunkType type,
                       QByteArrayView payload) = 0;

    /**
     * Complete reduction. This is the signal that all information is complete
     */
    virtual void finalize();

    // TODO: How do we express a failed decode
};
}  // namespace LegionParser