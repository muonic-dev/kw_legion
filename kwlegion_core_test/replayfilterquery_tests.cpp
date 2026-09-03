// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include <QAbstractListModel>
#include <QDate>
#include <QDateTime>
#include <QModelIndex>
#include <QTime>
#include <QVariant>
#include <catch2/catch_test_macros.hpp>
#include <utility>

#include "filterquery.h"
#include "replayfilterquery.h"
#include "replaystoremodel.h"

using namespace KWLegionCore;

namespace {

// A single-row, single-role model -- just enough for FilterQuery::acceptRow
// to read from via QAbstractItemModel::data(index, role). Avoids pulling in
// QStandardItemModel (Qt::Gui) for what's otherwise a QtCore-only test.
class SingleRowModel : public QAbstractListModel {
   public:
    SingleRowModel(int role, QVariant value)
        : m_role(role), m_value(std::move(value)) {}

    [[nodiscard]] int rowCount(
        const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : 1;
    }

    [[nodiscard]] QVariant data(const QModelIndex& index,
                                int role) const override {
        if (!index.isValid() || role != m_role) {
            return {};
        }
        return m_value;
    }

   private:
    int m_role;
    QVariant m_value;
};

bool acceptsTimestamp(const FilterQuery& query, const QDateTime& timestamp) {
    SingleRowModel model(
        static_cast<int>(ReplayStoreModel::Roles::TimestampRole),
        QVariant::fromValue(timestamp));
    return query.acceptRow(model, 0, QModelIndex());
}

}  // namespace

TEST_CASE(
    "RelativeDateTimeQuery BEFORE accepts strictly earlier timestamps only") {
    const QDateTime boundary(QDate(2026, 9, 3), QTime(12, 0));
    const RelativeDateTimeQuery query(
        ReplayStoreModel::Roles::TimestampRole, boundary,
        RelativeDateTimeQuery::Comparison::BEFORE);

    REQUIRE(acceptsTimestamp(query, boundary.addSecs(-1)));
    REQUIRE_FALSE(acceptsTimestamp(query, boundary));
    REQUIRE_FALSE(acceptsTimestamp(query, boundary.addSecs(1)));
}

TEST_CASE(
    "RelativeDateTimeQuery AFTER accepts strictly later timestamps only") {
    const QDateTime boundary(QDate(2026, 9, 3), QTime(12, 0));
    const RelativeDateTimeQuery query(
        ReplayStoreModel::Roles::TimestampRole, boundary,
        RelativeDateTimeQuery::Comparison::AFTER);

    REQUIRE_FALSE(acceptsTimestamp(query, boundary.addSecs(-1)));
    REQUIRE_FALSE(acceptsTimestamp(query, boundary));
    REQUIRE(acceptsTimestamp(query, boundary.addSecs(1)));
}

TEST_CASE(
    "RelativeDateTimeQuery rejects a row whose role value isn't a "
    "QDateTime") {
    const RelativeDateTimeQuery query(
        ReplayStoreModel::Roles::TimestampRole,
        QDateTime(QDate(2026, 9, 3), QTime(0, 0)),
        RelativeDateTimeQuery::Comparison::AFTER);

    SingleRowModel model(
        static_cast<int>(ReplayStoreModel::Roles::TimestampRole), QVariant());
    REQUIRE_FALSE(query.acceptRow(model, 0, QModelIndex()));
}

TEST_CASE(
    "RelativeDateTimeQuery::repr()'s comparison symbol matches its actual "
    "comparison direction") {
    const QDateTime boundary(QDate(2026, 9, 3), QTime(0, 0));
    const RelativeDateTimeQuery before(
        ReplayStoreModel::Roles::TimestampRole, boundary,
        RelativeDateTimeQuery::Comparison::BEFORE);
    const RelativeDateTimeQuery after(
        ReplayStoreModel::Roles::TimestampRole, boundary,
        RelativeDateTimeQuery::Comparison::AFTER);

    // BEFORE's acceptRow test is `date < compareTo`, AFTER's is
    // `compareTo < date` (i.e. `date > compareTo`) -- repr()'s symbol is
    // read as "TimestampRole <op> compareTo", so it must agree with those.
    REQUIRE(before.repr().toStdString() ==
            "TimestampRole<2026-09-03T00:00:00");
    REQUIRE(after.repr().toStdString() ==
            "TimestampRole>2026-09-03T00:00:00");
}

TEST_CASE(
    "the on: day-range shape (AFTER start-1ms, BEFORE start+1day) includes "
    "local midnight and excludes the following midnight") {
    // Mirrors OnDateQueryParser's exact construction directly, independent
    // of the parser and locale, to pin down the boundary-inclusivity
    // semantics this was fixed for: a replay timestamped exactly at local
    // midnight of the queried day must match; one at the very next local
    // midnight must not.
    const QDateTime start(QDate(2026, 9, 3), QTime(0, 0));
    const QDateTime end = start.addDays(1);

    ConjunctionFilterQuery onDay;
    onDay.addQuery(new RelativeDateTimeQuery(
        ReplayStoreModel::Roles::TimestampRole, start.addMSecs(-1),
        RelativeDateTimeQuery::Comparison::AFTER));
    onDay.addQuery(new RelativeDateTimeQuery(
        ReplayStoreModel::Roles::TimestampRole, end,
        RelativeDateTimeQuery::Comparison::BEFORE));

    REQUIRE(acceptsTimestamp(onDay, start));
    REQUIRE(acceptsTimestamp(onDay, start.addSecs(1)));
    REQUIRE_FALSE(acceptsTimestamp(onDay, start.addMSecs(-1)));
    REQUIRE_FALSE(acceptsTimestamp(onDay, end));
}
