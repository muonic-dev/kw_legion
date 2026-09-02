// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#pragma once

#include <QtQmlIntegration/qqmlintegration.h>

#include <QSortFilterProxyModel>

namespace KWLegionCore {

// QSortFilterProxyModel only exposes sort order through the sort(column,
// order) method, which isn't invokable from QML either. This adds a real
// sortOrder property so QML can bind/set it directly.
class SortFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
    Q_PROPERTY(Qt::SortOrder sortOrder READ sortOrder WRITE setSortOrder NOTIFY
                   sortOrderChanged)

    Q_PROPERTY(QObject* filterQuery READ filterQuery WRITE setFilterQuery
                   NOTIFY filterQueryChanged)

    QML_ELEMENT
   public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    // TODO: Does this actually need to exist?
    Q_INVOKABLE void refilter();

    void setSortOrder(Qt::SortOrder order);

    [[nodiscard]] QObject* filterQuery() const;
    void setFilterQuery(QObject* filterQuery);

   signals:
    void sortOrderChanged();
    void filterQueryChanged();

   protected:
    [[nodiscard]] bool filterAcceptsRow(
        int sourceRow, const QModelIndex& sourceParent) const override;

   private:
    QObject* m_filterQuery;
};

}  // namespace KWLegionCore
