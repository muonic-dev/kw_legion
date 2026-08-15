/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <kwlegion_core/replay.h>

#include <QAbstractListModel>

namespace KWLegionCore {

class StoreModel : public QAbstractListModel {
    Q_OBJECT

   public:
    enum class Roles : std::uint16_t {
        ChecksumRole = Qt::UserRole + 1,
        TimestampRole,
        MapNameRole,
        HasExternalPathRole,
    };

    Q_ENUM(Roles);

    StoreModel(QObject* parent = nullptr);

    ~StoreModel() override;

    StoreModel(const StoreModel&) = delete;
    StoreModel(StoreModel&&) = delete;

    StoreModel& operator=(const StoreModel&) = delete;
    StoreModel& operator=(StoreModel&&) = delete;

    void replaysLoaded(QList<Replay>);
    void replayDiscovered(Replay);
    void replayChanged(Replay);

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] QVariant data(const QModelIndex& index,
                                int role = Qt::DisplayRole) const override;
    [[nodiscard]] int rowCount(
        const QModelIndex& parent = QModelIndex()) const override;

   private:
    QHash<int, QByteArray> m_roleNames;

    QList<Replay> m_replays;
};
}  // namespace KWLegionCore