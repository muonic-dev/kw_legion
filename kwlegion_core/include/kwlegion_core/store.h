// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#pragma once

#include <kwlegion_core/replay.h>
#include <legionparser/replay.h>

#include <QDir>
#include <QLoggingCategory>
#include <QObject>
#include <QSqlDatabase>
#include <QStandardPaths>
#include <QString>

Q_DECLARE_LOGGING_CATEGORY(logStore);

namespace KWLegionCore {
class Queries;

class ReplayStore : public QObject {
    Q_OBJECT

   public:
    ReplayStore(const QString& statePath = QStandardPaths::writableLocation(
                    QStandardPaths::StateLocation),
                QObject* parent = nullptr);

    /**
     * The initial paths that exist in the replay folder
     *
     * The startup source of truth that will be used to determine replays
     * that exist in the replay folder. The store determines what is known,
     * what is new, and what has disappeared.
     *
     * This doubles as the startup trigger for migrations and setup and will
     * cause the emission of the replaysLoaded(QList<Replay>) so that we
     * don't flash stale content
     */
    void receiveInitialReplayPaths(const QList<QString>& paths);

    /**
     * A replay file definitely just appeared
     */
    void analyzeReplayFile(const QString& path);
    /**
     * A replay file has disappeared
     */
    void removeReplayFile(const QString& path);

   signals:
    void replaysLoaded(const QList<Replay>&);
    // A replay was was discovered or updated
    void replaysChanged(const QList<Replay>&);
    // A replay that did exist disappeared
    void replayRemoved(const QByteArray&);

   private:
    void ensureDb();
    void ensureDirectories();

    // Perform all the steps to try and receive a replay
    // File should probably be an absolute path
    // Returns the checksums that were impacted by the ingestion
    // This is guaranteed to contain metadata.checksum
    QList<QByteArray> ingestReplay(
        QFile& file, const LegionParser::ReplayMetadata& metadata);
    // The replay file at the path is gone or otherwise corrupt so we should
    // remove it
    std::optional<QByteArray> removeReplayAtPath(const QString& path);

    // Handles dealing with any existing checksums at a given path (which can
    // occur on multiple branches in ingestReplay)
    static void handleExistingReplayAtPath(Queries& queries,
                                           const QString& path,
                                           QList<QByteArray>& checksums);

    void forwardChangedReplays(const QList<QByteArray>& checksums);

    [[nodiscard]] QString computeIngestionPath(
        const QByteArray& checksum) const;

    QSqlDatabase m_db;
    QString m_dbPath;
    QString m_replayDir;
};
}  // namespace KWLegionCore