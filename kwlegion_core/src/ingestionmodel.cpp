/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <kwlegion_core/ingestionmodel.h>

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
          {static_cast<int>(Roles::WaitingRole), QByteArrayLiteral("waiting")}},
      m_mockData{
          {{static_cast<int>(Roles::PathRole), QVariant("C:\\foo")},
           {static_cast<int>(Roles::WaitingRole), QVariant(true)}},
          {{static_cast<int>(Roles::PathRole), QVariant("C:\\foo")},
           {static_cast<int>(Roles::WaitingRole), QVariant(false)}},
      },
      m_toggle(new QTimer(this)),
      m_show(false) {
    m_toggle->callOnTimeout([this]() {
        emit beginResetModel();
        m_show = !m_show;
        emit ingestionCountChanged();
        emit endResetModel();
    });
    m_toggle->start(5000);
}

int IngestionModel::ingestionCount() const {
    if (m_show) {
        return m_mockData.size();
    }
    return 0;
}

QHash<int, QByteArray> IngestionModel::roleNames() const { return m_roles; }

int IngestionModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_show ? m_mockData.size() : 0;
}

QVariant IngestionModel::data(const QModelIndex& index, int role) const {
    if (!m_show) {
        return {};
    }
    if (index.isValid()) {
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
