// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#pragma once

#include <QtQmlIntegration/qqmlintegration.h>

#include <QJSValue>
#include <QSortFilterProxyModel>

// QSortFilterProxyModel only exposes sort order through the sort(column,
// order) method, which isn't invokable from QML either. This adds a real
// sortOrder property so QML can bind/set it directly.
class SortFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
    Q_PROPERTY(Qt::SortOrder sortOrder READ sortOrder WRITE setSortOrder NOTIFY
                   sortOrderChanged)
    Q_PROPERTY(QJSValue filterPredicate READ filterPredicate WRITE
                   setFilterPredicate NOTIFY filterPredicateChanged)

    QML_ELEMENT
   public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    Q_INVOKABLE void refilter();

    void setSortOrder(Qt::SortOrder order);

    [[nodiscard]] QJSValue filterPredicate() const;
    void setFilterPredicate(QJSValue filterPredicate);

   signals:
    void sortOrderChanged();
    void filterPredicateChanged();

   protected:
    [[nodiscard]] bool filterAcceptsRow(
        int sourceRow, const QModelIndex& sourceParent) const override;

   private:
    QJSValue m_filterPredicate;
};
