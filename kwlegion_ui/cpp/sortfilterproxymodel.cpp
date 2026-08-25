// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include "sortfilterproxymodel.h"

void SortFilterProxyModel::setSortOrder(Qt::SortOrder order) {
    if (order == m_sortOrder) {
        return;
    }
    m_sortOrder = order;
    sort(0, order);
    emit sortOrderChanged();
}
