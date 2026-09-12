/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QList>
#include <QSpan>
#include <QtTypes>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <type_traits>

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
     * Prepare for receive chunk calls
     */
    virtual void begin();

    /**
     * @brief Analyze a single chunk
     *
     * @return false when the analyzer is uninterested in more data
     * @param chunkStart the starting offset of the chunk within the file
     * @param timecode the chunk timecode
     * @param type the chunk type
     * @param buf the body bytes of the chunk
     */
    virtual bool chunk(qsizetype chunkStartOffset, quint32 timecode,
                       ChunkType type, std::span<const std::byte> buf) = 0;

    /**
     * Complete reduction. This is the signal that all information is complete
     */
    virtual void finalize();

    // TODO: How do we express a failed decode
};

struct Command {
    // The command id
    quint8 commandId;
    // The mangledPlayerIdx. You should validate this against the player list in
    // the cast of corruption
    quint8 mangledPlayerIdx;
    // stored as const std::bytes instead of QByteArrayView to prevent
    // char/unsigned char promotion issues
    std::span<const std::byte> bytes;
};

// Until we get to c++23
template <typename F, typename R, typename... Args>
concept invocable_r = std::invocable<F, Args...> &&
                      std::convertible_to<std::invoke_result_t<F, Args...>, R>;

enum class DesyncTrigger : quint8 {
    IncompleteFixedLengthCommand,
    IncompleteStandardCommand,
    IncompleteBespokeCommand,
    UnrecognizedCommand,
    // Internal issues
    InvalidScanResult  // scanner consumed 0 or too much
};

struct DesyncDetails {
    DesyncTrigger cause;
    qsizetype chunkOffset;
    qsizetype
        relativeCommandOffset;  // Relative offset of the command in the chunk
    quint8 command;
};

/**
 * Scan/lex the framing structure of commands in a chunk and delegate to a
 * sub element
 *
 * Note, Command's hold a non-owning view into underlying bytes. They should
 * not be held following the return.
 */
class CommandFrameAnalyzer : public ChunkAnalyzer {
   public:
    template <invocable_r<bool, quint32, QSpan<Command>> ChunkFn,
              std::invocable<DesyncDetails> ErrorFn>
    explicit CommandFrameAnalyzer(ChunkFn&& chunkCallback,
                                  ErrorFn&& errorCallback)
        : m_chunk{std::forward<ChunkFn>(chunkCallback)},
          m_error{std::forward<ErrorFn>(errorCallback)} {}

    template <invocable_r<bool, quint32, QSpan<Command>> ChunkFn>
    explicit CommandFrameAnalyzer(ChunkFn&& chunkCallback)

        : m_chunk{std::forward<ChunkFn>(chunkCallback)},
          m_error{[](DesyncDetails /* offset */) {}} {}

    CommandFrameAnalyzer(const CommandFrameAnalyzer& other);
    CommandFrameAnalyzer(CommandFrameAnalyzer&& other) noexcept;

    CommandFrameAnalyzer& operator=(const CommandFrameAnalyzer& other);
    CommandFrameAnalyzer& operator=(CommandFrameAnalyzer&& other) noexcept;

    bool chunk(qsizetype chunkOffset, quint32 timecode, ChunkType type,
               std::span<const std::byte> buf) override;

   private:
    std::function<bool(quint32, QSpan<Command>)> m_chunk;
    std::function<void(DesyncDetails)> m_error;
};

constexpr int KW_UNMANGLE_K = 3;
inline std::optional<uint> unmanglePlayerIdx(quint8 mangledPlayerIdx) {
    const quint8 divided = mangledPlayerIdx / 8;
    if (divided < 3) {
        return std::nullopt;
    }
    return divided - KW_UNMANGLE_K;
}

}  // namespace LegionParser