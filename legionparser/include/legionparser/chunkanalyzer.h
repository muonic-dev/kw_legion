/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QList>
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
enum class ChunkType : std::uint8_t { Command = 1, Camera, Type3, Type4 };

/**
 * Interface for receiving chunks
 *
 */
class ChunkAnalyzer {
   public:
    ChunkAnalyzer() = default;
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
     * @param chunkStart the starting offset of the chunk within the file
     * @param timecode the chunk timecode
     * @param type the chunk type
     * @param QByteArrayView the body bytes of the chunk
     */
    virtual bool chunk(qsizetype chunkStart, qint32 timecode, ChunkType type,
                       QByteArrayView payload) = 0;

    /**
     * Complete reduction. This is the signal that all information is complete
     */
    virtual void finalize();

    // TODO: How do we express a failed decode
};

struct RawCommand {};

// Once we have more ability to decode commands this will be a variant
using Command = RawCommand;

// A chunk of commands (type 1 chunk)
struct CommandChunk {
    // The timecode
    qint32 timecode;
    // The raw payload
    QByteArray payload;
    // The decoded commands in the chunk
    // All framed commands will have have their
    // views reference payload
    QList<Command> commands;
};

class CommandChunkParser : public ChunkAnalyzer {
   public:
    bool chunk(qsizetype chunkOffset, qint32 timecode, ChunkType type,
               QByteArrayView payload) override;

    void finalize() override;

    void commandChunk(qint32 timecode, QByteArrayView payload);
};
}  // namespace LegionParser