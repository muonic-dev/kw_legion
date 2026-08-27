// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include "sortfilterproxymodel.h"

void SortFilterProxyModel::setSortOrder(Qt::SortOrder order) {
    if (order == sortOrder()) {
        return;
    }
    sort(0, order);
    emit sortOrderChanged();
}

void SortFilterProxyModel::setFilterRoles(const QList<int>& roles) {
    if (roles == m_filterRoles) {
        return;
    }
    m_filterRoles = roles;
    emit filterRolesChanged();
    beginFilterChange();
    endFilterChange();
}

bool SortFilterProxyModel::filterAcceptsRow(
    int sourceRow, const QModelIndex& sourceParent) const {
    if (m_filterRoles.isEmpty()) {
        return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
    }

    const QRegularExpression re = filterRegularExpression();
    if (re.pattern().isEmpty()) {
        return true;
    }

    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    for (int role : m_filterRoles) {
        if (re.match(sourceModel()->data(idx, role).toString()).hasMatch()) {
            return true;
        }
    }
    return false;
}
