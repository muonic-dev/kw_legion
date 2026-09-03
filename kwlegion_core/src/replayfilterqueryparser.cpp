/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include "replayfilterqueryparser.h"

#include "filterquery.h"
#include "replayfilterquery.h"

namespace KWLegionCore {
ReplayFilterQueryParser::ReplayFilterQueryParser(QObject* parent)
    : QObject(parent), m_current(new TautologyFilterQuery(this)) {}

QString ReplayFilterQueryParser::queryText() const { return m_text; }

void ReplayFilterQueryParser::setQueryText(const QString& value) {
    m_text = value;
    FilterQuery* previous = m_current;
    m_current = parse(QStringView(m_text));
    if (m_current == nullptr) {
        // The parse failed, flip back
        m_current = previous;
    } else {
        m_current->setParent(this);
        emit queryTextChanged();
        emit queryChanged();
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        delete previous;
    }
}

QObject* ReplayFilterQueryParser::query() const { return m_current; }

namespace {

class CompoundQueryParser final {
   public:
    CompoundQueryParser(QStringView text)
        : m_text(text), m_conj(new ConjunctionFilterQuery()) {}

    CompoundQueryParser(const CompoundQueryParser&) = delete;
    CompoundQueryParser(CompoundQueryParser&&) = delete;
    CompoundQueryParser& operator=(const CompoundQueryParser&) = delete;
    CompoundQueryParser& operator=(CompoundQueryParser&&) = delete;

    ~CompoundQueryParser() { delete m_conj; }

    ConjunctionFilterQuery* parse() {
        try {
            parseField();

            // We are done so we release ownership
            ConjunctionFilterQuery* v = m_conj;
            m_conj = nullptr;
            return v;
        } catch (const ParseError& err) {
            return nullptr;
        }
    }

    void eatWhitespace() {
        m_text = QStringView(
            std::ranges::find_if(m_text, [](QChar c) { return !c.isSpace(); }),
            m_text.end());
    }

    void parseField() {
        eatWhitespace();

        const auto* const colon = std::ranges::find(m_text, QChar(':'));
        const QStringView field = QStringView(m_text.begin(), colon - 1);

        // Find the closest match filterSwitch
    }

   private:
    class ParseError : public std::runtime_error {
       public:
        ParseError(const QString& what)
            : std::runtime_error(what.toStdString()) {}
    };

    QStringView m_text;
    // We always need a conjunction, however, on failure we need to free
    // So, we store and then if m_conj hasn't been taken from us by the time we
    // are destroyed we free it
    ConjunctionFilterQuery* m_conj;
};
}  // namespace

// Memory management by QObject semantics
// NOLINTBEGIN(cppcoreguidelines-owning-memory)
FilterQuery* ReplayFilterQueryParser::parse(QStringView text) {
    if (text.isEmpty()) {
        return new TautologyFilterQuery(this);
    }
    // Is there a `:` indicating an attempt to filter
    if (!text.contains(QChar(':'))) {
        return new AnyTextReplayFilterQuery(text.toString(), this);
    }
    CompoundQueryParser parser{text};
    return parser.parse();
}
// NOLINTEND(cppcoreguidelines-owning-memory)

}  // namespace KWLegionCore