// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include "reader.h"

#include <legionparser/exception.h>

#include <QLatin1StringView>
#include <QObject>
#include <QtTypes>
#include <optional>
#include <utility>

namespace LegionParser {

Reader::Reader(QIODevice& replayFile) : m_replayFile(replayFile) {}

QString Reader::readUtf16String() {
    // Match metadata strings are null-terminated sequences of 2-byte
    // (UTF-16LE) code units
    QDataStream stream(&m_replayFile);
    stream.setByteOrder(QDataStream::LittleEndian);
    // Explicitly non-null so an empty string doesn't decay into a null
    // QString, which QSqlQuery::bindValue would bind as SQL NULL.
    QString result{QLatin1String("")};
    quint16 codeUnit = 0;
    // We must always read at least 1 byte
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while)
    do {
        codeUnit = readIntegral<quint16>(stream);
        if (codeUnit != 0) {
            if (std::cmp_greater_equal(result.size(), MAX_STRING_LENGTH)) {
                throw LimitExceededException(
                    QLatin1String("match metadata string"),
                    m_offsetMgr.lastOffset(), MAX_STRING_LENGTH,
                    MAX_STRING_LENGTH + 1);
            }
            result.append(QChar(codeUnit));
        }
    } while (codeUnit != 0);
    return result;
}

QString Reader::readFixedUtf16String(qsizetype length) {
    QDataStream stream(&m_replayFile);
    stream.setByteOrder(QDataStream::LittleEndian);
    QString result;

    for (qsizetype i = 0; i < length; i++) {
        const auto codeUnit = readIntegral<quint16>(stream);
        result.append(QChar(codeUnit));
    }
    return result;
}

QString Reader::readFixedCharString(qsizetype length) {
    return QString::fromLatin1(readBlock(length));
}

// Magic value signalling the final chunk
constexpr qint32 END_TIME_CODE = 0x7FFFFFFF;

std::optional<BodyChunk> Reader::readBodyChunk() {
    const auto timeCode = readIntegral<qint32>();
    // The end-of-chunks marker is a bare time code with no type/size/data/
    // zero framing following it - the footer starts immediately after it -
    // so it must be recognized before trying to read the rest of a frame.
    if (timeCode == END_TIME_CODE) {
        return std::nullopt;
    }
    const auto typeCode = readByte<char>();
    if (typeCode < 1 || 4 < typeCode) {
        throw CorruptDataException(QStringLiteral("invalid chunk type"),
                                   lastOffset());
    }
    const auto type = static_cast<ChunkType>(typeCode);
    const auto chunkSize = readIntegral<qint32>();
    if (std::cmp_less(MAX_CHUNK_SIZE, chunkSize)) {
        throw LimitExceededException(QStringLiteral("oversized chunk"),
                                     m_offsetMgr.lastOffset(), MAX_CHUNK_SIZE,
                                     chunkSize);
    }
    QByteArray data = readBlock(chunkSize);
    discardZero<qint32>();
    return BodyChunk{
        .timeCode = timeCode, .type = type, .data = std::move(data)};
}

QByteArray Reader::readBlock(qsizetype length) {
    const QByteArray raw = m_replayFile.read(static_cast<qint64>(length));
    if (std::cmp_not_equal(raw.size(), length)) {
        throw TornDataException(m_offsetMgr.offset());
    }
    m_offsetMgr.increment(length);
    return raw;
}

QByteArray Reader::readRemaining(qsizetype maxSize) {
    const qsizetype budget = maxSize - m_offsetMgr.offset();
    if (budget < 0) {
        throw LimitExceededException(QLatin1String("replay payload"),
                                     m_offsetMgr.offset(), maxSize,
                                     m_offsetMgr.offset());
    }
    // Ask for one more byte than the budget allows - getting exactly that
    // many back means there was more left than the limit permits, without
    // ever having to buffer more than budget + 1 bytes to find out.
    const QByteArray raw = m_replayFile.read(static_cast<qint64>(budget) + 1);
    if (std::cmp_greater(raw.size(), budget)) {
        throw LimitExceededException(QLatin1String("replay payload"),
                                     m_offsetMgr.offset(), maxSize,
                                     m_offsetMgr.offset() + raw.size());
    }
    m_offsetMgr.increment(raw.size());
    return raw;
}

}  // namespace LegionParser