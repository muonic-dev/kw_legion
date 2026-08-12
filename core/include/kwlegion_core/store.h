// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#pragma once

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
    void receiveReplay(const QString& path);

   signals:
    void ingestionError();

   private:
    void ensureDb();
    void ensureDirectories();
    // Run all of the relevant ddl to bring the database up to current version
    void executeDdl();

    // Perform all the steps to try and receive a replay
    void ingestReplay(QFile& file,
                      const LegionParser::ReplayMetadata& metadata);

    [[nodiscard]] QString computeIngestionPath(
        const QByteArray& checksum) const;

    QSqlDatabase m_db;
    QString m_dbPath;
    QString m_replayDir;
};
}  // namespace KWLegionCore