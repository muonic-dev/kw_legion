// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include <legionparser/exception.h>

namespace LegionParser {

ReplayParseException::ReplayParseException(const QString& msg)
    : std::runtime_error(msg.toStdString()) {}

LimitExceededException::LimitExceededException(const QString& what,
                                               size_t offset, size_t limit,
                                               size_t allowed)
    : ReplayParseException(QString("%1 exceeded limit of %2 (was %3) at %4")
                               .arg(what)
                               .arg(limit)
                               .arg(allowed)
                               .arg(offset)),
      m_what(what),
      m_offset(offset),
      m_limit(limit),
      m_actual(allowed) {}

CorruptDataException::CorruptDataException(const QString& what, size_t offset)
    : ReplayParseException(QString("%1 at %2").arg(what).arg(offset)) {}

TornDataException::TornDataException(size_t offset)
    : ReplayParseException(QString("Torn at %1").arg(offset)) {}
}  // namespace LegionParser