/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include "filterquery.h"

#include <QObject>
#include <algorithm>
#include <ranges>

namespace KWLegionCore {
FilterQuery::FilterQuery(QObject* parent) : QObject(parent) {}

TautologyFilterQuery::TautologyFilterQuery(QObject* parent)
    : FilterQuery(parent) {}

bool TautologyFilterQuery::acceptRow(const QAbstractItemModel& /* source */,
                                     int /* row */,
                                     const QModelIndex& /* parent */) const {
    return true;
}

QString TautologyFilterQuery::repr() const { return QStringLiteral("TRUE"); }

ContradictionFilterQuery::ContradictionFilterQuery(QObject* parent)
    : FilterQuery(parent) {}

bool ContradictionFilterQuery::acceptRow(
    const QAbstractItemModel& /* source */, int /* row */,
    const QModelIndex& /* parent */) const {
    return false;
}

QString ContradictionFilterQuery::repr() const {
    return QStringLiteral("FALSE");
}

ConjunctionFilterQuery::ConjunctionFilterQuery(QObject* parent)
    : FilterQuery(parent) {}

void ConjunctionFilterQuery::addQuery(FilterQuery* query) {
    if (m_conjuctionOf.contains(query)) {
        return;
    }
    query->setParent(this);
    m_conjuctionOf.append(query);
}

bool ConjunctionFilterQuery::acceptRow(const QAbstractItemModel& source,
                                       int row,
                                       const QModelIndex& parent) const {
    auto falsified = std::ranges::find_if(
        m_conjuctionOf, [&source, &row, &parent](const auto& filter) {
            return !filter->acceptRow(source, row, parent);
        });
    return falsified == std::ranges::end(m_conjuctionOf);
}

QString ConjunctionFilterQuery::repr() const {
    QString repr("(AND ");
    for (const auto* const conj : m_conjuctionOf) {
        repr += conj->repr();
        repr += " ";
    }
    repr += ")";
    return repr;
}

DisjunctionFilterQuery::DisjunctionFilterQuery(QObject* parent)
    : FilterQuery(parent) {}

void DisjunctionFilterQuery::addQuery(FilterQuery* query) {
    if (m_disjunctionOf.contains(query)) {
        return;
    }
    query->setParent(this);
    m_disjunctionOf.append(query);
}

bool DisjunctionFilterQuery::acceptRow(const QAbstractItemModel& source,
                                       int row,
                                       const QModelIndex& parent) const {
    auto verified = std::ranges::find_if(
        m_disjunctionOf, [&source, &row, &parent](const auto& filter) {
            return filter->acceptRow(source, row, parent);
        });
    return verified != std::ranges::end(m_disjunctionOf);
}

QString DisjunctionFilterQuery::repr() const {
    QString repr("(OR ");
    for (const auto* const disj : m_disjunctionOf) {
        repr += disj->repr();
        repr += " ";
    }
    repr += ")";
    return repr;
}

}  // namespace KWLegionCore