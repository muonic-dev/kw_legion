/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>

#include "replay.h"

namespace KWLegionCore {

class ReplayProxy;

class StoreModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

   public:
    enum class Roles : std::uint16_t {
        ChecksumRole = Qt::UserRole + 1,
        TimestampRole,
        MapNameRole,
        HasExternalPathRole,
        TeamsRole,
    };

    Q_ENUM(Roles);

    StoreModel(QObject* parent = nullptr);

    ~StoreModel() override;

    StoreModel(const StoreModel&) = delete;
    StoreModel(StoreModel&&) = delete;

    StoreModel& operator=(const StoreModel&) = delete;
    StoreModel& operator=(StoreModel&&) = delete;

    // Forces the engine to construct the singleton eagerly (rather than on
    // first QML access) so main.cpp can retrieve the instance and wire up
    // signals before Main.qml loads.
    static StoreModel* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);

    void replaysLoaded(const QList<Replay>&);
    void replaysChanged(const QList<Replay>&);

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] QVariant data(const QModelIndex& index,
                                int role = Qt::DisplayRole) const override;
    [[nodiscard]] int rowCount(
        const QModelIndex& parent = QModelIndex()) const override;

   private:
    QHash<int, QByteArray> m_roleNames;

    QList<ReplayProxy*> m_replays;
};
}  // namespace KWLegionCore