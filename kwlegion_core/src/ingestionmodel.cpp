/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <kwlegion_core/ingestionmodel.h>

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
      },
      m_mockData{
          {{static_cast<int>(Roles::PathRole),
            QVariant("C:\\Replays\\GDI_vs_Nod_settling.KWReplay")},
           {static_cast<int>(Roles::TypeRole),
            QVariant(static_cast<int>(InboxType::PENDING))},
           {static_cast<int>(Roles::ObservedAtRole),
            QVariant(QDateTime::currentDateTime().addSecs(-12))}},
          {{static_cast<int>(Roles::PathRole),
            QVariant("C:\\Replays\\Scrin_vs_ZOCOM_torn.KWReplay")},
           {static_cast<int>(Roles::TypeRole),
            QVariant(static_cast<int>(InboxType::TORN))},
           {static_cast<int>(Roles::ObservedAtRole),
            QVariant(QDateTime::currentDateTime().addSecs(-3120))}},
          {{static_cast<int>(Roles::PathRole),
            QVariant("C:\\Replays\\corrupted_header.KWReplay")},
           {static_cast<int>(Roles::TypeRole),
            QVariant(static_cast<int>(InboxType::CORRUPT))},
           {static_cast<int>(Roles::ObservedAtRole),
            QVariant(QDateTime::currentDateTime().addSecs(-9))}},
      } {}

int IngestionModel::ingestionCount() const { return m_mockData.size(); }

QHash<int, QByteArray> IngestionModel::roleNames() const { return m_roles; }

int IngestionModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_mockData.size();
}

QVariant IngestionModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) {
        return {};
    }
    if (std::cmp_greater_equal(index.row(), m_mockData.size())) {
        return {};
    }
    const auto& row = m_mockData.at(index.row());
    const auto it = row.find(role);
    if (it == row.end()) {
        return {};
    }
    return (*it);
}

}  // namespace KWLegionCore
