/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include "teammodel.h"

#include <QVariant>

#include "replay.h"

namespace KWLegionCore {

TeamModel::TeamModel(std::uint32_t number, QObject* parent)
    : QAbstractListModel(parent),
      m_number(number),
      m_roleNames{
          {static_cast<int>(Roles::NameRole), "name"},
          {static_cast<int>(Roles::FactionRole), "faction"},
      } {}

QHash<int, QByteArray> TeamModel::roleNames() const { return m_roleNames; }

int TeamModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_players.size());
}

QVariant TeamModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() ||
        std::cmp_greater_equal(index.row(), m_players.size())) {
        return {};
    }
    const Player& player = m_players.at(index.row());
    switch (static_cast<Roles>(role)) {
        case Roles::NameRole:
            return player.name;
        case Roles::FactionRole:
            return QVariant::fromValue(player.faction);
        default:
            return {};
    }
}

void TeamModel::addPlayer(const Player& player) { m_players.append(player); }
}  // namespace KWLegionCore