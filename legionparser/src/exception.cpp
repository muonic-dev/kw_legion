// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include <legionparser/exception.h>

#include <stdexcept>

namespace LegionParser {

ReplayParseException::ReplayParseException(const QString& msg)
    : std::runtime_error(msg.toStdString()) {}

LimitExceededException::LimitExceededException(const QString& what,
                                               qsizetype offset,
                                               qsizetype limit,
                                               qsizetype allowed)
    : ReplayParseException(QString("%1 exceeded limit of %2 (was %3) at %4")
                               .arg(what)
                               .arg(limit)
                               .arg(allowed)
                               .arg(offset)),
      m_what(what),
      m_offset(offset),
      m_limit(limit),
      m_actual(allowed) {}

CorruptDataException::CorruptDataException(const QString& what,
                                           qsizetype offset)
    : ReplayParseException(QString("%1 at %2").arg(what).arg(offset)) {}

TornDataException::TornDataException(qsizetype offset)
    : ReplayParseException(QString("Torn at %1").arg(offset)) {}
}  // namespace LegionParser