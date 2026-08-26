/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <QDateTime>
#include <QObject>

namespace KWLegionCore {
class Replay;
/**
 * The ReplayStore works in terms of Replay objects which describe a cut-down
 * pod structure. To make the view work we want do do things like break apart
 * the replay into things like team models and similar. This is a conversion
 * from the storage structure to what is functionally a view-model
 *
 *
 */
class ReplayModel : public QObject {
    // ReplayModel is a QObject so that m_teams can participate in cleanup on
    // deletion We need to return a QList<QObject*> for the nice binding
    // properties of the view Its easier to just QList<Qobject *> here so it can
    // be returned and then allow the teams to be deleted
    Q_OBJECT

   public:
    // No default argument because we want to force parent to be the storemodel
    ReplayModel(const Replay& replay, QObject* parent);

    ~ReplayModel() override = default;

    ReplayModel(const ReplayModel&) = delete;
    ReplayModel(ReplayModel&&) = delete;

    ReplayModel& operator=(const ReplayModel&) = delete;
    ReplayModel& operator=(ReplayModel&&) = delete;

    // Update the replay from a given different input
    // Writes the roles that changes for emission
    QList<int> updateFromReplay(const Replay& replay);

    [[nodiscard]] QByteArray checksum() const { return m_checksum; }
    [[nodiscard]] QDateTime timestamp() const { return m_timestamp; }
    [[nodiscard]] QString matchTitle() const { return m_matchTitle; }
    [[nodiscard]] QString matchDescription() const {
        return m_matchDescription;
    }
    [[nodiscard]] QString mapName() const { return m_mapName; }
    [[nodiscard]] QString mapReference() const { return m_mapReference; }
    [[nodiscard]] bool hasExternalPath() const { return m_hasExternalPath; }

    [[nodiscard]] int teamCount() const {
        return static_cast<int>(m_teams.size());
    }

    [[nodiscard]] QList<QObject*> teams() const { return m_teams; }

    [[nodiscard]] QString inferPatch() const;

    static QString inferPatch(QStringView);

   private:
    QByteArray m_checksum;
    QDateTime m_timestamp;
    QString m_matchTitle;
    QString m_matchDescription;
    QString m_mapName;
    QString m_mapReference;
    bool m_hasExternalPath;
    QList<QObject*> m_teams;
};
}  // namespace KWLegionCore