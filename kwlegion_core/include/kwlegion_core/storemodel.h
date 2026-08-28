/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QSet>
#include <QUrl>

#include "replay.h"

namespace KWLegionCore {

class ReplayModel;

class StoreModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(
        int selectionCount READ selectionCount NOTIFY selectionCountChanged)

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

    // Explicit (non-toggling) bulk expose/hide over the current selection.
    Q_INVOKABLE void showSelectedReplays();
    Q_INVOKABLE void hideSelectedReplays();

    // Selection manipulation
    Q_INVOKABLE void setReplaySelected(const QByteArray& checksum);
    Q_INVOKABLE bool toggleReplaySelected(const QByteArray& checksum);
    Q_INVOKABLE void extendReplaySelection(const QList<QByteArray>& checksums);
    Q_INVOKABLE void clearSelected();
    Q_INVOKABLE void selectAllReplays();

    Q_INVOKABLE void saveReplayAs(const QByteArray& checksum, const QUrl& path);
    // Export currently selected replays to the given path
    Q_INVOKABLE void exportSelectedReplaysTo(const QUrl& path);

    Q_INVOKABLE QString friendlySaveName(const QByteArray& checksum);

    [[nodiscard]] int selectionCount() const {
        return static_cast<int>(m_selections.size());
    }

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] QVariant data(const QModelIndex& index,
                                int role = Qt::DisplayRole) const override;
    [[nodiscard]] int rowCount(
        const QModelIndex& parent = QModelIndex()) const override;

   signals:
    // The proxy signal going to store
    void shouldToggleReplayExposed(const QByteArray& checksum);
    void shouldExposeReplay(const QByteArray& checksum);
    void shouldHideReplay(const QByteArray& checksum);
    void shouldSaveReplay(const QByteArray& checksum, const QUrl& path);
    void shouldExportReplays(const QList<QByteArray>& checksums,
                             const QUrl& path);

    void selectionCountChanged();

   private:
    template <typename Iter>
    void dataChangedByIter(Iter it, const QList<int>& roles) {
        const QList<ReplayModel*>::const_iterator cit(it);
        const int row = static_cast<int>(cit - m_replays.cbegin());
        const QModelIndex idx = index(row);
        emit dataChanged(idx, idx, roles);
    }

    QHash<int, QByteArray> m_roleNames;

    QList<ReplayModel*> m_replays;
    QSet<QByteArray> m_selections;
};
}  // namespace KWLegionCore