// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include <QSignalSpy>
#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

#include "filterquery.h"
#include "replayfilterqueryparser.h"

using namespace KWLegionCore;

namespace {

// These all hand back std::string rather than QString deliberately. QString
// satisfies Catch2's is_range (it iterates QChars), so Catch2 renders one as
// "{ {?}, {?}, ... }" and a failed comparison tells you nothing about what
// actually differed. std::string it prints natively.

// AnyTextReplayFilterQuery is a DisjunctionFilterQuery built over
// matchTitle/mapName/patch in that order, so this mirrors its repr() exactly.
// Centralized here so a future change to that ordering only needs updating
// in one place instead of in every test that touches a bare-word clause.
std::string anyTextRepr(const QString& needle) {
    return QStringLiteral("(OR MatchTitleRole=%1 MapNameRole=%1 PatchRole=%1 )")
        .arg(needle)
        .toStdString();
}

std::string reprOf(const ReplayFilterQueryParser& parser) {
    return static_cast<FilterQuery*>(parser.query())->repr().toStdString();
}

// TextFieldReplayFilterQuery::repr() names its field by the Roles enum key,
// via QMetaEnum, rather than by the DSL prefix that selected it.
std::string fieldRepr(const QString& roleKey, const QString& needle) {
    return QStringLiteral("%1=%2").arg(roleKey, needle).toStdString();
}

// Every non-empty parse always goes through CompoundQueryParser, which
// always produces a ConjunctionFilterQuery at the top level - even a single
// bare word ends up as a one-child AND, per FilterParser's original
// "the root is always a conjunction" design.
std::string conjRepr(const std::vector<std::string>& parts) {
    std::string joined;
    for (const std::string& part : parts) {
        if (!joined.empty()) {
            joined += ' ';
        }
        joined += part;
    }
    return "(AND " + joined + " )";
}

}  // namespace

TEST_CASE("ReplayFilterQueryParser defaults to a tautology query") {
    ReplayFilterQueryParser parser;
    REQUIRE(reprOf(parser) == "TRUE");
}

TEST_CASE(
    "a colon-less bare phrase splits into per-word AnyField clauses, ANDed "
    "together") {
    ReplayFilterQueryParser parser;
    parser.setQueryText("atacama canyon");
    REQUIRE(reprOf(parser) ==
            conjRepr({anyTextRepr("atacama"), anyTextRepr("canyon")}));
}

TEST_CASE(
    "a colon inside a quoted bare word is not treated as a field separator") {
    ReplayFilterQueryParser parser;
    parser.setQueryText(R"(atacama "canyon:overlook" ridge)");
    REQUIRE(reprOf(parser) ==
            conjRepr({anyTextRepr("atacama"), anyTextRepr("canyon:overlook"),
                      anyTextRepr("ridge")}));
}

TEST_CASE("a known field prefix dispatches to that field's query") {
    ReplayFilterQueryParser parser;

    parser.setQueryText("map:atacama");
    REQUIRE(reprOf(parser) == conjRepr({fieldRepr("MapNameRole", "atacama")}));

    parser.setQueryText("title:showmatch");
    REQUIRE(reprOf(parser) ==
            conjRepr({fieldRepr("MatchTitleRole", "showmatch")}));
}

TEST_CASE("field clauses and bare words combine into one conjunction") {
    ReplayFilterQueryParser parser;
    parser.setQueryText("atacama map:canyon");
    REQUIRE(reprOf(parser) == conjRepr({anyTextRepr("atacama"),
                                        fieldRepr("MapNameRole", "canyon")}));
}

TEST_CASE("a quoted field value keeps its whitespace as one needle") {
    ReplayFilterQueryParser parser;
    parser.setQueryText(R"(map:"Atacama Road")");
    REQUIRE(reprOf(parser) ==
            conjRepr({fieldRepr("MapNameRole", "Atacama Road")}));
}

TEST_CASE(
    "a field prefix with no value fails to parse rather than matching "
    "everything") {
    // An empty needle would make QString::contains() true for every row, so
    // half-typing "map:" would briefly widen the filter to all replays.
    // nextWord throws instead, which keeps the last good query live.
    ReplayFilterQueryParser parser;
    parser.setQueryText("atacama");

    QSignalSpy spy(&parser, &ReplayFilterQueryParser::queryChanged);
    parser.setQueryText("map:");

    REQUIRE(spy.count() == 0);
    REQUIRE(reprOf(parser) == conjRepr({anyTextRepr("atacama")}));
}

TEST_CASE(
    "an unterminated quote fails to parse and leaves the previous query in "
    "place") {
    ReplayFilterQueryParser parser;
    parser.setQueryText("atacama");
    REQUIRE(reprOf(parser) == conjRepr({anyTextRepr("atacama")}));

    QSignalSpy spy(&parser, &ReplayFilterQueryParser::queryChanged);
    parser.setQueryText(R"("no closing quote: here)");

    REQUIRE(spy.count() == 0);
    REQUIRE(reprOf(parser) == conjRepr({anyTextRepr("atacama")}));
}

TEST_CASE(
    "an unrecognized field name fails to parse and leaves the previous "
    "query in place") {
    ReplayFilterQueryParser parser;
    parser.setQueryText("atacama");

    QSignalSpy spy(&parser, &ReplayFilterQueryParser::queryChanged);
    parser.setQueryText("bogus:value");

    REQUIRE(spy.count() == 0);
    REQUIRE(reprOf(parser) == conjRepr({anyTextRepr("atacama")}));
}

TEST_CASE(
    "setQueryText emits queryTextChanged and queryChanged on a successful "
    "parse") {
    ReplayFilterQueryParser parser;
    QSignalSpy textSpy(&parser, &ReplayFilterQueryParser::queryTextChanged);
    QSignalSpy querySpy(&parser, &ReplayFilterQueryParser::queryChanged);

    parser.setQueryText("atacama");

    REQUIRE(textSpy.count() == 1);
    REQUIRE(querySpy.count() == 1);
}

TEST_CASE("a successful reparse replaces the previous query") {
    ReplayFilterQueryParser parser;
    parser.setQueryText("atacama");
    parser.setQueryText("canyon");

    REQUIRE(reprOf(parser) == conjRepr({anyTextRepr("canyon")}));
}

TEST_CASE("clearing the query text reverts to a tautology") {
    ReplayFilterQueryParser parser;
    parser.setQueryText("atacama");
    parser.setQueryText("");

    REQUIRE(reprOf(parser) == "TRUE");
}
