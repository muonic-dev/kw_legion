// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include "sortfilterproxymodel.h"

#include <QJSEngine>

void SortFilterProxyModel::setSortOrder(Qt::SortOrder order) {
    if (order == sortOrder()) {
        return;
    }
    sort(0, order);
    emit sortOrderChanged();
}

QJSValue SortFilterProxyModel::filterPredicate() const {
    return m_filterPredicate;
}

void SortFilterProxyModel::setFilterPredicate(QJSValue value) {
    m_filterPredicate = std::move(value);
    emit filterPredicateChanged();
    beginFilterChange();
    endFilterChange();
}

void SortFilterProxyModel::refilter() {
    beginFilterChange();
    endFilterChange();
}

bool SortFilterProxyModel::filterAcceptsRow(
    int sourceRow, const QModelIndex& sourceParent) const {
    QJSEngine* engine = qjsEngine(this);
    if (engine == nullptr) {
        qWarning("SortFilterProxyModel is not a QML owned value");
        return QSortFilterProxyModel::filterAcceptsColumn(sourceRow,
                                                          sourceParent);
    }

    if (!m_filterPredicate.isCallable()) {
        qWarning("filterPredict is not a callable");
        return QSortFilterProxyModel::filterAcceptsColumn(sourceRow,
                                                          sourceParent);
    }

    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);

    QVariantMap rowData;
    for (auto [role, name] : sourceModel()->roleNames().asKeyValueRange()) {
        rowData.insert(name, sourceModel()->data(idx, role));
    }

    QJSValue arg = engine->toScriptValue(rowData);

    QJSValue result = m_filterPredicate.call({arg});
    if (result.isError()) {
        qWarning() << "predicate failed" << result.toString();
    }
    return result.toBool();
}
