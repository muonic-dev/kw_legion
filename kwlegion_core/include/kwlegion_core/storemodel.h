/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QSet>

#include "replay.h"

namespace KWLegionCore {

class ReplayModel;

class StoreModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

   public:
    enum class Roles : std::uint16_t {
        ChecksumRole = Qt::UserRole + 1,
        TimestampRole,
        MatchTitleRole,
        MatchDescriptionRole,
        MapNameRole,
        MapReferenceRole,
        HasExternalPathRole,
        TeamsRole,
        PatchRole,  // We guess the patch based on the suffix of the
                    // map_reference
        SelectedRole
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

    // Toggle a replays exposed state. This proxies to shouldTottleReplayExposed
    Q_INVOKABLE void toggleReplayExposed(const QByteArray& checksum);

    // Set a replay selected
    Q_INVOKABLE void setReplaySelected(const QByteArray& checksum);
    // Toggle a replays selection state.
    // Returns whether the last selection activated or deactivated
    Q_INVOKABLE bool toggleReplaySelected(const QByteArray& checksum);
    // Bulk add to the selection.
    Q_INVOKABLE void extendReplaySelection(const QList<QByteArray>& checksums);
    Q_INVOKABLE void clearSelected();

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] QVariant data(const QModelIndex& index,
                                int role = Qt::DisplayRole) const override;
    [[nodiscard]] int rowCount(
        const QModelIndex& parent = QModelIndex()) const override;

   signals:
    // The proxy signal going to store
    void shouldToggleReplayExposed(const QByteArray& checksum);

   private:
    template <typename Iter>
    void dataChangedByIter(Iter it) {
        const QList<ReplayModel*>::const_iterator cit(it);
        const int row = static_cast<int>(cit - m_replays.cbegin());
        const QModelIndex idx = index(row);
        emit dataChanged(idx, idx);
    }

    QHash<int, QByteArray> m_roleNames;

    QList<ReplayModel*> m_replays;
    QSet<QByteArray> m_selections;
};
}  // namespace KWLegionCore