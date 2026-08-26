/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <kwlegion_core/storemodel.h>

#include <algorithm>
#include <utility>

#include "replaymodel.h"

namespace KWLegionCore {
StoreModel::StoreModel(QObject* parent)
    : QAbstractListModel(parent),
      m_roleNames{
          {static_cast<int>(Roles::ChecksumRole),
           QByteArrayLiteral("checksum")},
          {static_cast<int>(Roles::TimestampRole),
           QByteArrayLiteral("timestamp")},
          {static_cast<int>(Roles::MatchTitleRole),
           QByteArrayLiteral("matchTitle")},
          {static_cast<int>(Roles::MatchDescriptionRole),
           QByteArrayLiteral("matchDescription")},
          {static_cast<int>(Roles::MapNameRole), QByteArrayLiteral("mapName")},
          {static_cast<int>(Roles::MapReferenceRole),
           QByteArrayLiteral("mapReference")},
          {static_cast<int>(Roles::HasExternalPathRole),
           QByteArrayLiteral("hasExternalPath")},
          {static_cast<int>(Roles::TeamsRole), QByteArrayLiteral("teams")},
          {static_cast<int>(Roles::PatchRole), QByteArrayLiteral("patch")},
          {static_cast<int>(Roles::SelectedRole),
           QByteArrayLiteral("selected")},
      } {}

StoreModel::~StoreModel() = default;

StoreModel* StoreModel::create(QQmlEngine* /*qmlEngine*/,
                               QJSEngine* /*jsEngine*/) {
    // Signature is Qt's QML_SINGLETON factory contract - must return T*, not
    // gsl::owner<T*>. Ownership transfers to the QML engine at the call site.
    return new StoreModel();  // NOLINT(cppcoreguidelines-owning-memory)
}

void StoreModel::replaysLoaded(const QList<Replay>& replays) {
    beginResetModel();
    m_replays.clear();
    for (const auto& replay : replays) {
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        m_replays.append(new ReplayModel(replay, this));
    }
    endResetModel();
}

void StoreModel::replaysChanged(const QList<Replay>& replays) {
    for (const auto& replay : replays) {
        auto it =
            std::ranges::find_if(m_replays, [&replay](const ReplayModel* r) {
                return r->checksum() == replay.checksum;
            });

        if (it != m_replays.end()) {
            // Update the existing replay proxy with the new data
            (*it)->updateFromReplay(replay);
            dataChangedByIter(it);
        } else {
            const int row = static_cast<int>(m_replays.size());
            beginInsertRows(QModelIndex(), row, row);
            // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
            m_replays.append(new ReplayModel(replay, this));
            endInsertRows();
        }
    }
}

void StoreModel::toggleReplayExposed(const QByteArray& checksum) {
    emit shouldToggleReplayExposed(checksum);
}

void StoreModel::setReplaySelected(const QByteArray& checksum) {
    auto it = std::ranges::find_if(m_replays, [&checksum](ReplayModel* replay) {
        return replay->checksum() == checksum;
    });
    if (it != m_replays.end()) {
        m_selections.insert(checksum);
        dataChangedByIter(it);
    }
}

void StoreModel::extendReplaySelection(const QList<QByteArray>& checksums) {
    for (const auto& checksum : checksums) {
        auto it =
            std::ranges::find_if(m_replays, [&checksum](ReplayModel* replay) {
                return replay->checksum() == checksum;
            });
        if (it != m_replays.end()) {
            m_selections.insert(checksum);
            dataChangedByIter(it);
        }
    }
}

bool StoreModel::toggleReplaySelected(const QByteArray& checksum) {
    auto it = std::ranges::find_if(m_replays, [&checksum](ReplayModel* replay) {
        return replay->checksum() == checksum;
    });
    bool active{false};
    if (it != m_replays.end()) {
        if (m_selections.contains(checksum)) {
            m_selections.remove(checksum);
        } else {
            m_selections.insert(checksum);
            active = true;
        }
        dataChangedByIter(it);
    }
    return active;
}

void StoreModel::clearSelected() {
    const QSet<QByteArray> checksums = std::move(m_selections);
    for (auto it = m_replays.cbegin(); it < m_replays.cend(); ++it) {
        if (checksums.contains((*it)->checksum())) {
            dataChangedByIter(it);
        }
    }
}

void StoreModel::saveReplayAs(const QByteArray& checksum, const QString& path) {

}

QHash<int, QByteArray> StoreModel::roleNames() const { return m_roleNames; }

int StoreModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_replays.size());
}

QVariant StoreModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() ||
        std::cmp_greater_equal(index.row(), m_replays.size())) {
        return {};
    }
    const ReplayModel* replay = m_replays.at(index.row());
    switch (static_cast<Roles>(role)) {
        case Roles::ChecksumRole:
            return replay->checksum();
        case Roles::TimestampRole:
            return replay->timestamp();
        case Roles::MatchTitleRole:
            return replay->matchTitle();
        case Roles::MatchDescriptionRole:
            return replay->matchDescription();
        case Roles::MapNameRole:
            return replay->mapName();
        case Roles::HasExternalPathRole:
            return replay->hasExternalPath();
        case Roles::TeamsRole:
            return QVariant::fromValue(replay->teams());
        case Roles::SelectedRole:
            return m_selections.contains(replay->checksum());
        case Roles::PatchRole:
            return replay->inferPatch();
        default:
            return {};
    }
}

}  // namespace KWLegionCore