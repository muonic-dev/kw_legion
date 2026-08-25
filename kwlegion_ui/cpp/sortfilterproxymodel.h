// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#pragma once

#include <QSortFilterProxyModel>

// QSortFilterProxyModel only exposes sort order through the sort(column,
// order) method, which isn't invokable from QML either. This adds a real
// sortOrder property so QML can bind/set it directly.
class SortFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
    Q_PROPERTY(Qt::SortOrder sortOrder READ sortOrder WRITE setSortOrder
                   NOTIFY sortOrderChanged)

   public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    [[nodiscard]] Qt::SortOrder sortOrder() const { return m_sortOrder; }
    void setSortOrder(Qt::SortOrder order);

   signals:
    void sortOrderChanged();

   private:
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
};
