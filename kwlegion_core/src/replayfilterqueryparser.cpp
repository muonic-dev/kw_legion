/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include "replayfilterqueryparser.h"

#include <QObject>
#include <QString>
#include <QStringView>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

#include "filterquery.h"
#include "replayfilterquery.h"
#include "replaystoremodel.h"

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
// Memory is handled by QObject ownership
// NOLINTBEGIN(cppcoreguidelines-owning-memory)
// QStringView iterator are pointers so iterator math becomes pointer math
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)

class ParseError : public std::runtime_error {
   public:
    ParseError(const QString& what) : std::runtime_error(what.toStdString()) {}
};
// Return the next relevant token as [word, rest]. This will stop at either
// `:`, whitespace, or the quote boundary if current starts with a quote.
// You should eatWhitespace before calling this.
//
// Throws ParseError rather than ever handing back an empty word. That is
// load bearing on two counts: an empty word would leave rest == the input,
// so parseField's raw-word path could not advance and the caller would spin;
// and it is what makes a half-typed clause (`map:`, a bare `""`) fail the
// parse, so the last good query stays live instead of the empty needle
// matching every row.
std::tuple<QStringView, QStringView> nextWord(QStringView current) {
    if (current.isEmpty()) {
        throw ParseError(QStringLiteral("expected a word"));
    }

    QStringView word;
    QStringView rest;

    if (current.at(0) == QChar('"')) {
        // Advance past the '"'
        const QStringView quoted = current.sliced(1);
        const auto* const it = std::ranges::find(quoted, QChar('"'));
        // Unclosed `"`
        if (it == quoted.end()) {
            throw ParseError(QStringLiteral("unclosed quote"));
        }
        word = QStringView(quoted.begin(), it);
        rest = QStringView(it + 1, quoted.end());
    } else {
        const auto* const it =
            std::ranges::find_if(current, [](const QChar& c) {
                return c.isSpace() || c == QChar(':') || c == QChar('"');
            });
        word = QStringView(current.begin(), it);
        rest = QStringView(it, current.end());
    }

    if (word.isEmpty()) {
        throw ParseError(QStringLiteral("expected a word"));
    }

    return {word, rest};
}

QStringView eatWhitespace(QStringView current) {
    const auto* const it = std::ranges::find_if(
        current, [](const QChar& c) { return !c.isSpace(); });

    return {it, current.end()};
}

class FieldQueryParser {
   public:
    FieldQueryParser(QString fieldLabel)
        : m_fieldLabel(std::move(fieldLabel)) {}
    FieldQueryParser(const FieldQueryParser&) = default;
    FieldQueryParser(FieldQueryParser&&) = default;

    FieldQueryParser& operator=(const FieldQueryParser&) = delete;
    FieldQueryParser& operator=(FieldQueryParser&&) = delete;

    virtual ~FieldQueryParser() = default;

    bool matches(QStringView fieldLabel) { return m_fieldLabel == fieldLabel; }

    // Parse the field's value out of input, returning the constructed query
    // alongside the remaining unconsumed input.
    [[nodiscard]] virtual std::tuple<FilterQuery*, QStringView> parse(
        QStringView input) = 0;

   private:
    QString m_fieldLabel;
};

class TextQueryParser : public FieldQueryParser {
   public:
    TextQueryParser(QString fieldLabel, ReplayStoreModel::Roles role)
        : FieldQueryParser(std::move(fieldLabel)), m_role(role) {}

    [[nodiscard]] std::tuple<FilterQuery*, QStringView> parse(
        QStringView input) override {
        const auto [word, rest] = nextWord(input);
        return {new TextFieldReplayFilterQuery(m_role, word.toString()), rest};
    }

   private:
    ReplayStoreModel::Roles m_role;
};

class StringListQueryParser : public FieldQueryParser {
   public:
    StringListQueryParser(QString fieldLabel, ReplayStoreModel::Roles role)
        : FieldQueryParser(std::move(fieldLabel)), m_role(role) {}

    [[nodiscard]] std::tuple<FilterQuery*, QStringView> parse(
        QStringView input) override {
        const auto [word, rest] = nextWord(input);
        return {
            new StringListContainsReplayFilterQuery(m_role, word.toString()),
            rest};
    }

   private:
    ReplayStoreModel::Roles m_role;
};

// The field dispatch table, shared by every CompoundQueryParser instead of
// rebuilt per parse. Safe only because TextQueryParser (and any future
// subparser added here) is stateless after construction, and dispatch runs
// synchronously with no reentrancy on the single GUI thread -- a subparser
// that needed to stash state across its own parse() call would have to own
// that state some other way (e.g. locally within parse()) rather than as a
// member, since it is now a long-lived shared instance.
const std::vector<std::unique_ptr<FieldQueryParser>>& fieldParsers() {
    static const std::vector<std::unique_ptr<FieldQueryParser>> PARSERS = [] {
        std::vector<std::unique_ptr<FieldQueryParser>> v;
        v.emplace_back(std::make_unique<TextQueryParser>(
            "map", ReplayStoreModel::Roles::MapNameRole));
        v.emplace_back(std::make_unique<TextQueryParser>(
            "title", ReplayStoreModel::Roles::MatchTitleRole));
        v.emplace_back(std::make_unique<TextQueryParser>(
            "patch", ReplayStoreModel::Roles::PatchRole));
        v.emplace_back(std::make_unique<StringListQueryParser>(
            "player", ReplayStoreModel::Roles::PlayersRole));
        return v;
    }();
    return PARSERS;
}

class CompoundQueryParser final {
   public:
    CompoundQueryParser(QStringView text)
        : m_text(text),
          m_conj(new ConjunctionFilterQuery()),
          m_subparsers(fieldParsers()) {}

    CompoundQueryParser(const CompoundQueryParser&) = delete;
    CompoundQueryParser(CompoundQueryParser&&) = delete;
    CompoundQueryParser& operator=(const CompoundQueryParser&) = delete;
    CompoundQueryParser& operator=(CompoundQueryParser&&) = delete;

    ~CompoundQueryParser() { delete m_conj; }

    ConjunctionFilterQuery* parse() {
        try {
            while (parseField()) {
            }

            // We are done so we release ownership
            ConjunctionFilterQuery* v = m_conj;
            m_conj = nullptr;
            return v;
        } catch (const ParseError& err) {
            return nullptr;
        }
    }

    bool parseField() {
        m_text = eatWhitespace(m_text);

        // Empty after whitespace, we are done
        if (m_text.isEmpty()) {
            return false;
        }

        const auto [word, rest] = nextWord(m_text);
        // We are doing a field
        if (!rest.isEmpty() && rest.at(0) == QChar(':')) {
            m_text = rest.sliced(1);
            for (const auto& subparser : m_subparsers) {
                if (subparser->matches(word)) {
                    auto [query, remaining] = subparser->parse(m_text);
                    m_text = remaining;
                    m_conj->addQuery(query);
                    return true;
                }
            }
            throw ParseError(QStringLiteral("unrecognized field %1").arg(word));
        }
        // Raw word
        m_text = rest;
        m_conj->addQuery(new AnyTextReplayFilterQuery(word.toString()));
        return true;
    }

    QStringView m_text;
    // We always need a conjunction, however, on failure we need to free
    // So, we store and then if m_conj hasn't been taken from us by the time we
    // are destroyed we free it
    ConjunctionFilterQuery* m_conj;

    const std::vector<std::unique_ptr<FieldQueryParser>>& m_subparsers;
};

// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
// NOLINTEND(cppcoreguidelines-owning-memory)
}  // namespace

// Memory management by QObject semantics
// NOLINTBEGIN(cppcoreguidelines-owning-memory)
FilterQuery* ReplayFilterQueryParser::parse(QStringView text) {
    if (text.isEmpty()) {
        return new TautologyFilterQuery(this);
    }
    CompoundQueryParser parser{text};
    return parser.parse();
}
// NOLINTEND(cppcoreguidelines-owning-memory)

}  // namespace KWLegionCore