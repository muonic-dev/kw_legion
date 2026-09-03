// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include "sortfilterproxymodel.h"

#include <QObject>
#include <Qt>
#include <QtLogging>

#include "filterquery.h"

namespace KWLegionCore {

void SortFilterProxyModel::setSortOrder(Qt::SortOrder order) {
    if (order == sortOrder()) {
        return;
    }
    sort(0, order);
    emit sortOrderChanged();
}

QObject* SortFilterProxyModel::filterQuery() const { return m_filterQuery; }

void SortFilterProxyModel::setFilterQuery(QObject* value) {
    m_filterQuery = value;
    emit filterQueryChanged();
    beginFilterChange();
    endFilterChange();
}

void SortFilterProxyModel::refilter() {
    beginFilterChange();
    endFilterChange();
}

bool SortFilterProxyModel::filterAcceptsRow(
    int sourceRow, const QModelIndex& sourceParent) const {
    const FilterQuery* predicate = qobject_cast<FilterQuery*>(m_filterQuery);
    if (predicate == nullptr) {
        qCritical() << "filterQuery property of incorrect type";
        // Returning false so its very obvious?
        return false;
    }
    return predicate->acceptRow(*sourceModel(), sourceRow, sourceParent);
}

}  // namespace KWLegionCore
