#pragma once

#include <legionparser/legionparser_export.h>

#include <QString>
#include <exception>
#include <stdexcept>

namespace LegionParser {

class LEGIONPARSER_EXPORT ReplayParseException : public std::runtime_error {
   protected:
    explicit ReplayParseException(const QString& msg);
};

class LEGIONPARSER_EXPORT LimitExceededException : public ReplayParseException {
   public:
    LimitExceededException(const QString& what, size_t offset, size_t limit,
                           size_t actual);

   private:
    QString m_what;
    size_t m_offset;
    size_t m_limit;
    size_t m_actual;
};

class LEGIONPARSER_EXPORT CorruptDataException : public ReplayParseException {
   public:
    CorruptDataException(const QString& what, size_t offset);
};

class LEGIONPARSER_EXPORT IOException : public ReplayParseException {
   public:
    explicit IOException(const QString& errorString);
};

}  // namespace LegionParser