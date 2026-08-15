// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#pragma once

#include <kwlegion_core/replay.h>
#include <legionparser/replay.h>

#include <QByteArrayView>
#include <QDir>
#include <QLoggingCategory>
#include <QObject>
#include <QSqlDatabase>

Q_DECLARE_LOGGING_CATEGORY(logStore);

namespace KWLegionCore {
class ReplayStore : public QObject {
    Q_OBJECT

   public:
    explicit ReplayStore(QObject* parent = nullptr);

    void startup();

    void analyzeReplayFile(const QString& path);
    void removeReplayFile(const QString& path);

   signals:
    void replaysLoaded(QList<Replay>);
    // A replay was was discovered for the first time
    void replayDiscovered(Replay);
    // A replay that was already known by checksum was found
    void replayChanged(Replay);
    // A replay that did exist disappeared
    void replayRemoved(QByteArray);

   private:
    void ensureDb();
    void ensureDirectories();

    // Perform all the steps to try and receive a replay
    // File should probably be an absolute path
    void ingestReplay(QFile& file,
                      const LegionParser::ReplayMetadata& metadata);

    [[nodiscard]] QString computeIngestionPath(
        const QByteArray& checksum) const;

    QSqlDatabase m_db;
    QString m_dbPath;
    QString m_replayDir;
};
}  // namespace KWLegionCore