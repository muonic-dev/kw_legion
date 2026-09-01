/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <kwlegion_core/ingestionmodel.h>
#include <kwlegion_core/replaystore.h>

#include <QDateTime>

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
}

void IngestionModel::inboxReset() {}

void IngestionModel::inboxItemObserved(const InboxItem& item) {}

void IngestionModel::inboxItemRemoved(const QString& path) {}

}  // namespace KWLegionCore
