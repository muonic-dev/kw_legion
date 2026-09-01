/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QQmlEngine>
#include <QTimer>


namespace KWLegionCore {
/**
 * Track the amount of items that are currently
 */
class IngestionModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(
        int ingestionCount READ ingestionCount NOTIFY ingestionCountChanged)

   public:
    enum class Roles : std::uint16_t {
        PathRole = Qt::UserRole + 1,
        WaitingRole,  // Is this a wait for settle or an error
    };

    Q_ENUM(Roles);

    static IngestionModel* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);

    explicit IngestionModel(QObject* parent = nullptr);

    [[nodiscard]] int ingestionCount() const;

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] QVariant data(const QModelIndex& index,
                                int role = Qt::DisplayRole) const override;
    [[nodiscard]] int rowCount(
        const QModelIndex& parent = QModelIndex()) const override;

   signals:
    void ingestionCountChanged();

   private:
    QHash<int, QByteArray> m_roles;
    QList<QHash<int, QVariant>> m_mockData;
    QTimer* m_toggle;
    bool m_show;
};
}  // namespace KWLegionCore