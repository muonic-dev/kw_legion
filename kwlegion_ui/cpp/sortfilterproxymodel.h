// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#pragma once

#include <QList>
#include <QSortFilterProxyModel>

// QSortFilterProxyModel only exposes sort order through the sort(column,
// order) method, which isn't invokable from QML either. This adds a real
// sortOrder property so QML can bind/set it directly.
class SortFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
    Q_PROPERTY(Qt::SortOrder sortOrder READ sortOrder WRITE setSortOrder NOTIFY
                   sortOrderChanged)
    // Roles to test filterRegularExpression against, e.g. [StoreModel.MatchTitleRole,
    // StoreModel.MapNameRole]. QML sets this using whatever roles its source model
    // exposes, so this class never needs to know about them itself. Falls back to
    // the base class's single-filterRole behavior when left empty.
    Q_PROPERTY(QList<int> filterRoles READ filterRoles WRITE setFilterRoles NOTIFY
                   filterRolesChanged)

   public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void setSortOrder(Qt::SortOrder order);

    [[nodiscard]] QList<int> filterRoles() const { return m_filterRoles; }
    void setFilterRoles(const QList<int>& roles);

   signals:
    void sortOrderChanged();
    void filterRolesChanged();

   protected:
    [[nodiscard]] bool filterAcceptsRow(
        int sourceRow, const QModelIndex& sourceParent) const override;

   private:
    QList<int> m_filterRoles;
};
