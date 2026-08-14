// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#pragma once

#include <legionparser/exception.h>

#include <QByteArray>
#include <QByteArrayView>
#include <QIODevice>
#include <QString>
#include <array>
#include <utility>

namespace LegionParser {

template <typename T>
concept ByteSized = std::is_trivially_copyable_v<T> && sizeof(T) == 1;

constexpr std::size_t MAX_STRING_LENGTH = static_cast<std::size_t>(4096 * 16);

// Provides read utility methods while tracking offsets
class Reader {
   public:
    Reader(QIODevice& replayFile);

    Reader(const Reader&) = delete;
    Reader(Reader&&) = delete;
    Reader operator=(const Reader&) = delete;
    Reader operator=(Reader&&) = delete;

    virtual ~Reader() = default;

    [[nodiscard]] size_t offset() const { return m_offsetMgr.offset(); }

    // The offset prior to the last read operation
    // Useful for when you want to throw an exception based on the value of
    // something you have already read
    [[nodiscard]] size_t lastOffset() const { return m_offsetMgr.lastOffset(); }

    [[nodiscard]] size_t mark() const { return m_offsetMgr.mark(); }

    [[nodiscard]] size_t mark() { return m_offsetMgr.mark(); }

    void setMark() { m_offsetMgr.setMark(); }

    QString readUtf16String();

    QString readFixedUtf16String(std::size_t length);

    template <std::integral T = uint32_t>
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
    QString readFixedCharString(std::size_t length);

    // Read a fixed string prefixed by a specific length prefix
    template <std::integral T = uint32_t>
    QString readFixedCharString() {
        const auto length = readIntegral<T>();
        if (std::cmp_greater(length, MAX_STRING_LENGTH)) {
            throw LimitExceededException(QLatin1String("ascii string"),
                                         m_offsetMgr.lastOffset(),
                                         MAX_STRING_LENGTH, length);
        }
        return readFixedCharString(length);
    }

    QByteArray readBlock(size_t length);

    // Reads the remainder of the device in bounded chunks, invoking
    // fn(chunk) once per chunk read, until EOF. Lets callers process a
    // large/unbounded remaining payload without ever buffering the whole
    // thing in memory at once.
    //
    // Returns the last two chunks read, concatenated in file order (or
    // fewer/none, if the payload was smaller than that) - letting callers
    // validate trailing structure (e.g. a footer) that might straddle a
    // chunk boundary, without every chunk needing to be copied out on the
    // chance it turns out to be one of the final two.
    QByteArray readRemainingChunked(std::invocable<QByteArrayView> auto&& fn,
                                    qint64 chunkSize = 16384) {
        // Fixed at chunkSize bytes for their whole lifetime - a short read
        // doesn't shrink them, so buffers.at(i).size() is never the valid
        // byte count. Track that separately in sizes below; Uninitialized
        // avoids zero-filling bytes that a short read leaves untouched and
        // that we then never look at.
        std::array<QByteArray, 2> buffers{
            QByteArray(static_cast<qsizetype>(chunkSize), Qt::Uninitialized),
            QByteArray(static_cast<qsizetype>(chunkSize), Qt::Uninitialized)};
        std::array<qsizetype, 2> sizes{0, 0};
        // The slot that the next successful read will land in.
        int next = 0;

        for (;;) {
            const qint64 bytesRead =
                m_replayFile.read(buffers.at(next).data(), chunkSize);
            if (bytesRead < 0) {
                throw CorruptDataException(QLatin1String("Error reading payload"),
                                           m_offsetMgr.offset());
            }
            if (bytesRead == 0) {
                break;
            }
            m_offsetMgr.increment(static_cast<size_t>(bytesRead));
            sizes.at(next) = static_cast<qsizetype>(bytesRead);
            fn(QByteArrayView(buffers.at(next).constData(), bytesRead));
            next = 1 - next;
        }

        // `next` now names the slot holding the older of the last two
        // chunks (or an untouched, zero-sized slot if fewer than two
        // chunks were read); the other slot holds the most recent chunk.
        const int older = next;
        const int newer = 1 - next;
        QByteArray tail;
        tail.reserve(sizes.at(older) + sizes.at(newer));
        tail.append(buffers.at(older).constData(), sizes.at(older));
        tail.append(buffers.at(newer).constData(), sizes.at(newer));
        return tail;
    }

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
                throw CorruptDataException(QLatin1String("Unexpected EOF"),
                                           m_offsetMgr.offset());
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

    template <ByteSized T>
    T readByte() {
        char value = 0x0;
        if (!m_replayFile.getChar(&value)) {
            throw CorruptDataException(QLatin1String("Unexpected EOF"),
                                       m_offsetMgr.offset());
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

   private:
    // A utility class for managing the offsets
    // This exists here for inline so that there is no risk of mismanaging
    // offsets by forgetting to do the entire set of offset manipulatino
    // somewhere
    class OffsetManager {
       public:
        OffsetManager() = default;

        // Get the current offset, the furthest that the reader has read
        [[nodiscard]] size_t offset() const { return m_offset; }
        // Get the last offset. The position of the offset prior to the last
        // increment call This is useful for callers to query when they have
        // completed a read and the resulting data does not pass validation
        // checks for formatting exceptions.
        [[nodiscard]] size_t lastOffset() const { return m_lastOffset; }
        // Get the current mark, useful for tracking specific offsets
        [[nodiscard]] size_t mark() const { return m_mark; }

        void increment(size_t delta) {
            m_lastOffset = m_offset;
            m_offset += delta;
        }

        void setMark() { m_mark = m_offset; }

       private:
        size_t m_offset = 0;
        size_t m_lastOffset = 0;
        size_t m_mark = 0;
    };

    QIODevice& m_replayFile;
    OffsetManager m_offsetMgr;
};

}  // namespace LegionParser