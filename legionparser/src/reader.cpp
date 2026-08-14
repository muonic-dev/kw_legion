// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include "reader.h"

#include <legionparser/exception.h>

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
                    QLatin1String("match metadata string"), m_offsetMgr.lastOffset(),
                    MAX_STRING_LENGTH, MAX_STRING_LENGTH + 1);
            }
            result.append(QChar(codeUnit));
        }
    } while (codeUnit != 0);
    return result;
}

QString Reader::readFixedUtf16String(std::size_t length) {
    QDataStream stream(&m_replayFile);
    stream.setByteOrder(QDataStream::LittleEndian);
    QString result;

    for (size_t i = 0; i < length; i++) {
        const auto codeUnit = readIntegral<quint16>(stream);
        result.append(QChar(codeUnit));
    }
    return result;
}

QString Reader::readFixedCharString(std::size_t length) {
    return QString::fromLatin1(readBlock(length));
}

QByteArray Reader::readBlock(size_t length) {
    const QByteArray raw = m_replayFile.read(static_cast<qint64>(length));
    if (std::cmp_not_equal(raw.size(), length)) {
        throw CorruptDataException(QLatin1String("Unexpected EOF"),
                                   m_offsetMgr.offset());
    }
    m_offsetMgr.increment(length);
    return raw;
}

}  // namespace LegionParser