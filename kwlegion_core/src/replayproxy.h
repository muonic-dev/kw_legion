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
class ReplayProxy : public QObject {
    // ReplayProxy is a QObject so that m_teams can participate in cleanup on
    // deletion We need to return a QList<QObject*> for the nice binding
    // properties of the view Its easier to just QList<Qobject *> here so it can
    // be returned and then allow the teams to be deleted
    Q_OBJECT

   public:
    // No default argument because we want to force parent to be the storemodel
    ReplayProxy(const Replay& replay, QObject* parent);

    ~ReplayProxy() override = default;

    ReplayProxy(const ReplayProxy&) = delete;
    ReplayProxy(ReplayProxy&&) = delete;

    ReplayProxy& operator=(const ReplayProxy&) = delete;
    ReplayProxy& operator=(ReplayProxy&&) = delete;

    void updateFromReplay(const Replay& replay);

    [[nodiscard]] QByteArray checksum() const { return m_checksum; }
    [[nodiscard]] QDateTime timestamp() const { return m_timestamp; }
    [[nodiscard]] QString mapName() const { return m_mapName; }
    [[nodiscard]] bool hasExternalPath() const { return m_hasExternalPath; }

    [[nodiscard]] int teamCount() const {
        return static_cast<int>(m_teams.size());
    }

    [[nodiscard]] QList<QObject*> teams() const { return m_teams; }

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