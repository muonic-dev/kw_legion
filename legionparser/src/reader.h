// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#pragma once

#include <legionparser/analyzer.h>
#include <legionparser/exception.h>

#include <QByteArray>
#include <QByteArrayView>
#include <QIODevice>
#include <QString>
#include <Qt>
#include <QtTypes>
#include <array>
#include <concepts>
#include <utility>

namespace LegionParser {

template <typename T>
concept ByteSized = std::is_trivially_copyable_v<T> && sizeof(T) == 1;

constexpr qsizetype MAX_STRING_LENGTH = static_cast<qsizetype>(4096 * 16);
constexpr qsizetype MAX_CHUNK_SIZE = static_cast<qsizetype>(4096 * 16);

struct BodyChunk {
    qint32 timeCode = 0;
    ChunkType type = ChunkType::Command;  // Make init happy
    QByteArray data;
};

/**
 * @brief A utility for reading various primitive structures from the file
 *
 * Additionally, provides some basic offset tracking for consistency.
 */
class Reader {
   public:
    Reader(QIODevice& replayFile);

    Reader(const Reader&) = delete;
    Reader(Reader&&) = delete;
    Reader operator=(const Reader&) = delete;
    Reader operator=(Reader&&) = delete;

    virtual ~Reader() = default;

    [[nodiscard]] qsizetype offset() const { return m_offsetMgr.offset(); }

    // The offset prior to the last read operation
    // Useful for when you want to throw an exception based on the value of
    // something you have already read
    [[nodiscard]] qsizetype lastOffset() const {
        return m_offsetMgr.lastOffset();
    }

    [[nodiscard]] qsizetype mark() const { return m_offsetMgr.mark(); }

    [[nodiscard]] qsizetype mark() { return m_offsetMgr.mark(); }

    void setMark() { m_offsetMgr.setMark(); }

    QString readUtf16String();

    QString readFixedUtf16String(qsizetype length);

    template <std::integral T = quint32>
    QString readFixedUtf16String() {
        const auto length = readIntegral<T>();
        if (std::cmp_greater(length, MAX_STRING_LENGTH)) {
            throw LimitExceededException(QLatin1String("utf16 string"),
                                         m_offsetMgr.lastOffset(),
                                         MAX_STRING_LENGTH, length);
        }
        return readFixedUtf16String(length);
    }

    // Read a fixed length string where the length is known ahead of time
    QString readFixedCharString(qsizetype length);

    // Read a fixed string prefixed by a specific length prefix
    template <std::integral T = quint32>
    QString readFixedCharString() {
        const auto length = readIntegral<T>();
        if (std::cmp_greater(length, MAX_STRING_LENGTH)) {
            throw LimitExceededException(QLatin1String("ascii string"),
                                         m_offsetMgr.lastOffset(),
                                         MAX_STRING_LENGTH, length);
        }
        return readFixedCharString(length);
    }

    QByteArray readBlock(qsizetype length);

    // Reads everything remaining in the device in a single call, bounded by
    // maxSize - an absolute ceiling on this Reader's total offset, matching
    // the convention offset()-based limit checks already use elsewhere in
    // this class (e.g. readBodyChunk's caller checking offset() against
    // MAX_BODY_SIZE per iteration). Guards against buffering an unbounded
    // amount of memory for a corrupt/malicious tail without needing to page
    // through it the way readRemainingChunked does.
    QByteArray readRemaining(qsizetype maxSize);

    // Read a single integral value via QDataStream
    template <std::integral T>
    T readIntegral() {
        QDataStream stream(&m_replayFile);
        stream.setByteOrder(QDataStream::LittleEndian);
        return readIntegral<T>(stream);
    }

    // Read a single integral value via a shared QDataStream
    template <std::integral T>
    T readIntegral(QDataStream& stream) {
        {
            T value{};
            stream >> value;
            if (stream.status() != QDataStream::Ok) {
                throw TornDataException(m_offsetMgr.offset());
            }
            m_offsetMgr.increment(sizeof(T));
            return value;
        }
    }

    // Construct a DataStream and pass it to the lambda for re-use
    // Can subsequently call multiple readIntegrals
    decltype(auto) withDataStream(std::invocable<QDataStream&> auto&& fn) {
        QDataStream stream(&m_replayFile);
        stream.setByteOrder(QDataStream::LittleEndian);
        return std::forward<decltype(fn)>(fn)(stream);
    }

    template <ByteSized T = std::byte>
    T readByte() {
        char value = 0x0;
        if (!m_replayFile.getChar(&value)) {
            throw TornDataException(m_offsetMgr.offset());
        }
        m_offsetMgr.increment(1);
        return std::bit_cast<T>(value);
    }

    template <std::integral T>
    void discardZero() {
        const auto zero = readIntegral<T>();
        if (std::cmp_not_equal(zero, 0)) {
            throw CorruptDataException(QLatin1String("Expected 0"),
                                       m_offsetMgr.lastOffset());
        }
    }

    std::optional<BodyChunk> readBodyChunk();

   private:
    // TODO: Deal with the fact that size_t disagrees with qsizetype (which
    // is signed) A utility class for managing the offsets This exists here
    // for inline so that there is no risk of mismanaging offsets by
    // forgetting to do the entire set of offset manipulatino somewhere
    class OffsetManager {
       public:
        OffsetManager() = default;

        // Get the current offset, the furthest that the reader has read
        [[nodiscard]] qsizetype offset() const { return m_offset; }
        // Get the last offset. The position of the offset prior to the last
        // increment call This is useful for callers to query when they have
        // completed a read and the resulting data does not pass validation
        // checks for formatting exceptions.
        [[nodiscard]] qsizetype lastOffset() const { return m_lastOffset; }
        // Get the current mark, useful for tracking specific offsets
        [[nodiscard]] qsizetype mark() const { return m_mark; }

        void increment(qsizetype delta) {
            m_lastOffset = m_offset;
            m_offset += delta;
        }

        void setMark() { m_mark = m_offset; }

       private:
        qsizetype m_offset = 0;
        qsizetype m_lastOffset = 0;
        qsizetype m_mark = 0;
    };

    QIODevice& m_replayFile;
    OffsetManager m_offsetMgr;
};

}  // namespace LegionParser