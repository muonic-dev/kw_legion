// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#pragma once

#include <QString>
#include <stdexcept>

namespace LegionParser {

class ReplayParseException : public std::runtime_error {
   protected:
    explicit ReplayParseException(const QString& msg);
};

class LimitExceededException : public ReplayParseException {
   public:
    LimitExceededException(const QString& what, qsizetype offset,
                           qsizetype limit, qsizetype actual);

   private:
    QString m_what;
    qsizetype m_offset;
    qsizetype m_limit;
    qsizetype m_actual;
};

class CorruptDataException : public ReplayParseException {
   public:
    CorruptDataException(const QString& what, qsizetype offset);
};

class TornDataException : public ReplayParseException {
   public:
    TornDataException(qsizetype offset);
};

class IOException : public ReplayParseException {
   public:
    explicit IOException(const QString& errorString);
};

}  // namespace LegionParser