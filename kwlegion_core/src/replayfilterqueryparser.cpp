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
#include <optional>
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

// TODO: Efficiency regarding QLocal::system
// This can change, so I think what we actually want to do is build a
// parse context object that we can thread though the dispatch so things
// we know are static for the parse can be held that way.

class ParseError : public std::runtime_error {
   public:
    ParseError(const QString& what) : std::runtime_error(what.toStdString()) {}
};
// Return the next relevant token as [word, rest]. This will stop at either
// `:`, whitespace, or the quote boundary if current starts with a quote.
// You should eatWhitespace before calling this.
//
// Returns std::nullopt rather than ever handing back an empty word -- an
// empty word would leave rest == the input, so parseField's raw-word path
// could not advance and the caller would spin. nullopt covers only "there is
// no word to read here" (current is empty, or a delimiter sits at the very
// start with nothing before it). That is deliberately not an exception:
// whether an absent word is an error is caller-dependent -- a half-typed
// clause like `map:` should fail the parse so the last good query stays
// live, but a lookahead probing for an optional second word (e.g. a
// date-time's time component) should treat the same absence as "there isn't
// one" and fall back quietly. Only the caller knows which, so callers that
// require a word must check the result and throw ParseError themselves.
//
// An unclosed quote (`"foo` with no closing `"`) indicates input is present but
// malformed and always throws ParseError regardless of caller, since no caller
// should treat a dangling quote as merely "no word here".
std::optional<std::tuple<QStringView, QStringView>> nextWord(
    QStringView current) {
    if (current.isEmpty()) {
        return std::nullopt;
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
        return std::nullopt;
    }

    return std::make_tuple(word, rest);
}

QStringView eatWhitespace(QStringView current) {
    const auto* const it = std::ranges::find_if(
        current, [](const QChar& c) { return !c.isSpace(); });

    return {it, current.end()};
}

// Like nextWord, but ignores `:` as a delimiter -- vim's "W" motion versus
// nextWord's "w": nextWord is small/strict (stops at `:`, needed to detect
// `field:value` prefixes), nextLongWord is big/permissive (stops only at
// whitespace), needed for date/time value words where `:` is part of the
// value itself (e.g. "14:30"), not a separator. You should eatWhitespace
// before calling this, same as nextWord. Unlike nextWord this never throws:
// a run of non-whitespace characters is always well-formed, so there is no
// malformed-input case to reject -- nullopt covers only "no word here".
std::optional<std::tuple<QStringView, QStringView>> nextLongWord(
    QStringView current) {
    if (current.isEmpty()) {
        return std::nullopt;
    }
    const auto* const it = std::ranges::find_if(
        current, [](const QChar& c) { return c.isSpace(); });
    const QStringView word(current.begin(), it);
    if (word.isEmpty()) {
        return std::nullopt;
    }
    return std::make_tuple(word, QStringView(it, current.end()));
}

// Counts the whitespace-delimited long-words in text, using the exact same
// whitespace notion as nextLongWord/eatWhitespace. Used to count words in a
// QLocale format *pattern* (e.g. "M/d/yy h:mm AP" -> 3), not a rendered
// value -- Qt format patterns render their literal separators (including
// whitespace) verbatim, so the pattern's word count is exactly the word
// count any valid rendering under it will have.
int countWords(QStringView text) {
    int count = 0;
    QStringView rest = text;
    while (const auto next = nextLongWord(eatWhitespace(rest))) {
        ++count;
        rest = std::get<1>(*next);
    }
    return count;
}

// Consumes exactly `count` long-words from the start of current and returns
// the *original* span covering them (not a reconstructed/rejoined string --
// some locales separate date/time components with something other than a
// plain ASCII space, e.g. a narrow no-break space before an AM/PM marker;
// nextLongWord's whitespace-based tokenization already handles that
// correctly without needing to know which character it was, and slicing the
// original text preserves it exactly rather than risking a mismatch against
// whatever QLocale::toDate/toDateTime expects there). nullopt if current
// doesn't contain `count` words.
std::optional<std::tuple<QStringView, QStringView>> consumeWords(
    QStringView current, int count) {
    // Defensively guard missed eatWhitespace to ensure that the start anchor
    // position is correct
    current = eatWhitespace(current);
    QStringView rest = current;
    for (int i = 0; i < count; ++i) {
        // Also eat whitespace so that we can find the tail correctly
        const auto next = nextLongWord(eatWhitespace(rest));
        if (!next) {
            return std::nullopt;
        }
        rest = std::get<1>(*next);
    }
    return std::make_tuple(QStringView(current.begin(), rest.begin()), rest);
}

std::tuple<QStringView, QStringView> requireWord(QStringView current) {
    const auto next = nextWord(current);
    if (!next) {
        throw ParseError("expected a word");
    }
    return *next;
}

bool isNextQuoted(QStringView current) {
    current = eatWhitespace(current);
    return !current.isEmpty() && current.at(0) == QChar('"');
}

// Consumes a date value from current -- either a quoted value taken
// verbatim as one unit (matching every other field's quoting convention),
// or an unquoted value consuming exactly as many words as locale's short
// date format needs. Not simply "read one word": some locales (Hungarian,
// Korean) render even their *short* date format as multiple
// space-separated groups rather than one contiguous token.
//
// A malformed quoted value is always a hard failure -- the user explicitly
// delimited a complete value and it still didn't parse -- so that case
// throws rather than returning nullopt. An unquoted value that doesn't
// parse returns nullopt instead, the same "didn't work, caller decides"
// contract used elsewhere in this file (e.g. before an unrecognized-field
// error, or a future fallback, even though nothing needs one today).
std::optional<std::tuple<QDate, QStringView>> nextDate(const QLocale& locale,
                                                       QStringView current) {
    if (isNextQuoted(current)) {
        const auto [word, rest] = requireWord(current);
        const auto date = locale.toDate(word.toString(), QLocale::ShortFormat);
        if (!date.isValid()) {
            throw ParseError(QStringLiteral("invalid quoted date"));
        }
        return std::make_tuple(date, rest);
    }

    const int wordCount =
        countWords(QStringView(locale.dateFormat(QLocale::ShortFormat)));
    const auto consumed = consumeWords(current, wordCount);
    if (!consumed) {
        return std::nullopt;
    }
    const auto [span, rest] = *consumed;
    const auto date = locale.toDate(span.toString(), QLocale::ShortFormat);
    if (!date.isValid()) {
        return std::nullopt;
    }
    return std::make_tuple(date, rest);
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
        const auto [word, rest] = requireWord(input);
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
        const auto [word, rest] = requireWord(input);
        return {
            new StringListContainsReplayFilterQuery(m_role, word.toString()),
            rest};
    }

   private:
    ReplayStoreModel::Roles m_role;
};

class OnDateQueryParser : public FieldQueryParser {
   public:
    OnDateQueryParser(QString fieldLabel, ReplayStoreModel::Roles role)
        : FieldQueryParser(std::move(fieldLabel)), m_role(role) {}
    [[nodiscard]] std::tuple<FilterQuery*, QStringView> parse(
        QStringView input) override {
        const auto parsed = nextDate(QLocale::system(), input);
        if (!parsed) {
            throw ParseError(QStringLiteral("invalid date"));
        }
        const auto [date, rest] = *parsed;
        const auto start = date.startOfDay(QTimeZone::LocalTime);
        if (!start.isValid()) {
            throw ParseError("invalid date");
        }

        // We have a valid date, turn it into the bracket
        const auto end = start.addDays(1);
        if (!end.isValid()) {
            throw ParseError("internal inconsistency");
        }

        auto* conj = new ConjunctionFilterQuery();
        conj->addQuery(new RelativeDateTimeQuery(
            m_role,
            // Sub 1 for poor mans >=
            start.addMSecs(-1), RelativeDateTimeQuery::Comparison::AFTER));
        conj->addQuery(new RelativeDateTimeQuery(
            m_role, end, RelativeDateTimeQuery::Comparison::BEFORE));
        return {conj, rest};
    }

   private:
    ReplayStoreModel::Roles m_role;
};

class ComparisonDateTimeQueryParser : public FieldQueryParser {
   public:
    ComparisonDateTimeQueryParser(QString fieldLabel,
                                  ReplayStoreModel::Roles role,
                                  RelativeDateTimeQuery::Comparison comparison)
        : FieldQueryParser(std::move(fieldLabel)),
          m_role(role),
          m_comparison(comparison) {}

    [[nodiscard]] std::tuple<FilterQuery*, QStringView> parse(
        QStringView input) override {
        const auto parsed = nextDate(QLocale::system(), input);
        if (!parsed) {
            throw ParseError(QStringLiteral("unparseable date"));
        }
        const auto [date, rest] = *parsed;
        auto* query = new RelativeDateTimeQuery(
            m_role, date.startOfDay(QTimeZone::LocalTime), m_comparison);
        return {query, rest};
    }

   private:
    ReplayStoreModel::Roles m_role;
    RelativeDateTimeQuery::Comparison m_comparison;
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
        v.emplace_back(std::make_unique<OnDateQueryParser>(
            "on", ReplayStoreModel::Roles::TimestampRole));
        v.emplace_back(std::make_unique<ComparisonDateTimeQueryParser>(
            "before", ReplayStoreModel::Roles::TimestampRole,
            RelativeDateTimeQuery::Comparison::BEFORE));
        v.emplace_back(std::make_unique<ComparisonDateTimeQueryParser>(
            "after", ReplayStoreModel::Roles::TimestampRole,
            RelativeDateTimeQuery::Comparison::AFTER));
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

        const auto [word, rest] = requireWord(m_text);
        // We are doing a field
        if (!rest.isEmpty() && rest.at(0) == QChar(':')) {
            m_text = eatWhitespace(rest.sliced(1));
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