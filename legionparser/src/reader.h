#pragma once

#include <legionparser/exception.h>

#include <QByteArray>
#include <QIODevice>
#include <QString>
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
            throw LimitExceededException(QString("utf16 string"),
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
            throw LimitExceededException(QString("ascii string"),
                                         m_offsetMgr.lastOffset(),
                                         MAX_STRING_LENGTH, length);
        }
        return readFixedCharString(length);
    }

    QByteArray readBlock(size_t length);

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
                throw CorruptDataException(QString("Unexpected EOF"),
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
            throw CorruptDataException(QString("Unexpected EOF"),
                                       m_offsetMgr.offset());
        }
        m_offsetMgr.increment(1);
        return std::bit_cast<T>(value);
    }

    template <std::integral T>
    void discardZero() {
        const auto zero = readIntegral<T>();
        if (std::cmp_not_equal(zero, 0)) {
            throw CorruptDataException(QString("Expected 0"),
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