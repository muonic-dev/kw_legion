/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <kwlegion_core/ingestionmodel.h>
#include <kwlegion_core/replaystore.h>

#include <QAbstractItemModel>
#include <QDateTime>
#include <QHash>
#include <QHashFunctions>
#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QVariant>
#include <algorithm>
#include <utility>

namespace KWLegionCore {
IngestionModel* IngestionModel::create(QQmlEngine* /*qmlEngine*/,
                                       QJSEngine* /*jsEngine*/) {
    // Signature is Qt's QML_SINGLETON factory contract - must return T*, not
    // gsl::owner<T*>. Ownership transfers to the QML engine at the call site.
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    return new IngestionModel();
}

IngestionModel::IngestionModel(QObject* parent)
    : QAbstractListModel(parent),
      m_roles{
          {static_cast<int>(Roles::PathRole), QByteArrayLiteral("path")},
          {static_cast<int>(Roles::TypeRole), QByteArrayLiteral("type")},
          {static_cast<int>(Roles::ObservedAtRole),
           QByteArrayLiteral("observedAt")},
      } {}

int IngestionModel::ingestionCount() const {
    return static_cast<int>(m_inbox.size());
}

QHash<int, QByteArray> IngestionModel::roleNames() const { return m_roles; }

int IngestionModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_inbox.size());
}

QVariant IngestionModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) {
        return {};
    }
    if (std::cmp_greater_equal(index.row(), m_inbox.size())) {
        return {};
    }
    const auto& row = m_inbox.at(index.row());

    switch (static_cast<Roles>(role)) {
        case Roles::PathRole:
            return row.path;
        case Roles::TypeRole:
            return static_cast<int>(row.type);
        case Roles::ObservedAtRole:
            return row.observedAt;
    }
    return {};
}

void IngestionModel::setStore(ReplayStore* store) const {
    QObject::connect(store, &ReplayStore::inboxReset, this,
                     &IngestionModel::inboxReset);
    QObject::connect(store, &ReplayStore::inboxItemObserved, this,
                     &IngestionModel::inboxItemObserved);
    QObject::connect(store, &ReplayStore::inboxItemRemoved, this,
                     &IngestionModel::inboxItemRemoved);

    // Reverse connect
    QObject::connect(this, &IngestionModel::shouldAcknowledgeItem, store,
                     &ReplayStore::acknowledgeItem);
}

void IngestionModel::acknowledgeItem(const QString& path) {
    emit shouldAcknowledgeItem(path);
}

void IngestionModel::inboxReset() {
    const bool wasEmpty = m_inbox.isEmpty();
    beginResetModel();
    m_inbox.clear();
    endResetModel();
    if (!wasEmpty) {
        emit ingestionCountChanged();
    }
}

void IngestionModel::inboxItemObserved(const InboxItem& item) {
    auto it = std::ranges::find_if(
        m_inbox, [&item](const InboxItem& i) { return item.path == i.path; });
    if (it != m_inbox.end()) {
        // Assumes that items will not typically have a changed observed at
        *it = item;
        const QModelIndex idx = index(static_cast<int>(it - m_inbox.begin()));
        emit dataChanged(idx, idx);
    } else {
        const int row = static_cast<int>(m_inbox.size());
        beginInsertRows(QModelIndex(), row, row);
        m_inbox.append(item);
        endInsertRows();
        emit ingestionCountChanged();
    }
}

void IngestionModel::inboxItemRemoved(const QString& path) {
    auto it = std::ranges::find_if(
        m_inbox, [&path](const InboxItem& i) { return path == i.path; });
    if (it != m_inbox.end()) {
        const int row = static_cast<int>(it - m_inbox.begin());
        beginRemoveRows(QModelIndex(), row, row);
        m_inbox.erase(it);
        endRemoveRows();
        emit ingestionCountChanged();
    }
}

}  // namespace KWLegionCore
