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
