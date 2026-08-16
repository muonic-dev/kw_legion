/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <kwlegion_core/storemodel.h>

#include <algorithm>
#include <utility>

namespace KWLegionCore {
StoreModel::StoreModel(QObject* parent) : QAbstractListModel(parent) {
    m_roleNames.insert(static_cast<int>(Roles::ChecksumRole),
                       QByteArrayLiteral("checksum"));
    m_roleNames.insert(static_cast<int>(Roles::TimestampRole),
                       QByteArrayLiteral("timestamp"));
    m_roleNames.insert(static_cast<int>(Roles::MapNameRole),
                       QByteArrayLiteral("mapName"));
    m_roleNames.insert(static_cast<int>(Roles::HasExternalPathRole),
                       QByteArrayLiteral("hasExternalPath"));
}

StoreModel::~StoreModel() = default;

StoreModel* StoreModel::create(QQmlEngine* /*qmlEngine*/,
                               QJSEngine* /*jsEngine*/) {
    // Signature is Qt's QML_SINGLETON factory contract - must return T*, not
    // gsl::owner<T*>. Ownership transfers to the QML engine at the call site.
    return new StoreModel();  // NOLINT(cppcoreguidelines-owning-memory)
}

void StoreModel::replaysLoaded(QList<Replay> replays) {
    beginResetModel();
    m_replays = std::move(replays);
    endResetModel();
}

void StoreModel::replayDiscovered(const Replay& replay) {
    const int size = static_cast<int>(m_replays.size());
    beginInsertRows(QModelIndex(), size, size);
    m_replays.append(replay);
    endInsertRows();
}

void StoreModel::replayChanged(const Replay& replay) {
    auto it = std::ranges::find_if(m_replays, [&replay](const Replay& r) {
        return r.checksum == replay.checksum;
    });

    if (it != m_replays.end()) {
        *it = replay;
        const int row = static_cast<int>(it - m_replays.begin());
        const QModelIndex idx = index(row);
        emit dataChanged(idx, idx);
    }
}

QHash<int, QByteArray> StoreModel::roleNames() const { return m_roleNames; }

int StoreModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_replays.size());
}

QVariant StoreModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_replays.size()) {
        return {};
    }
    const Replay& replay = m_replays.at(index.row());
    switch (static_cast<Roles>(role)) {
        case Roles::ChecksumRole:
            return replay.checksum;
        case Roles::TimestampRole:
            return replay.timestamp;
        case Roles::MapNameRole:
            return replay.mapName;
        case Roles::HasExternalPathRole:
            return replay.hasExternalPath;
    }
    return {};
}
}  // namespace KWLegionCore