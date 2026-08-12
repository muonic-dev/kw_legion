// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#pragma once

#include <QString>
#include <exception>
#include <stdexcept>

namespace LegionParser {

class ReplayParseException : public std::runtime_error {
   protected:
    explicit ReplayParseException(const QString& msg);
};

class LimitExceededException : public ReplayParseException {
   public:
    LimitExceededException(const QString& what, size_t offset, size_t limit,
                           size_t actual);

   private:
    QString m_what;
    size_t m_offset;
    size_t m_limit;
    size_t m_actual;
};

class CorruptDataException : public ReplayParseException {
   public:
    CorruptDataException(const QString& what, size_t offset);
};

class IOException : public ReplayParseException {
   public:
    explicit IOException(const QString& errorString);
};

}  // namespace LegionParser