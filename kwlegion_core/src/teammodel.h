/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <kwlegion_core/replay.h>

#include <QAbstractListModel>
#include <QList>
#include <QObject>

#include "replaymodel.h"

namespace KWLegionCore {

// The model of a team. A team is a list of players and their factions
class TeamModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QStringList playerNames READ playerNames)
   public:
    enum class Roles : std::uint16_t {
        NameRole = Qt::UserRole + 1,
        FactionRole,
    };

    TeamModel(std::uint32_t number, QObject* parent = nullptr);

    [[nodiscard]] std::uint32_t number() const { return m_number; }

    void addPlayer(const Player& player);

    // Flat player-name list for JS-side searching (e.g. SortFilterProxyModel's
    // filterPredicate).
    [[nodiscard]] QStringList playerNames() const;

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int rowCount(
        const QModelIndex& parent = QModelIndex()) const override;

    [[nodiscard]] QVariant data(const QModelIndex& index,
                                int role = Qt::DisplayRole) const override;

   private:
    std::uint32_t m_number;
    QHash<int, QByteArray> m_roleNames;
    QList<Player> m_players;
};
}  // namespace KWLegionCore