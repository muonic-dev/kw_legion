// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include <kwlegion_core/store.h>
#include <kwlegion_core/transaction.h>
#include <legionparser/parser.h>

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <stdexcept>

#include "exception.h"
#include "queries.h"

Q_LOGGING_CATEGORY(logStore, "kwlegion.store");

namespace KWLegionCore {

ReplayStore::ReplayStore(const QString& statePath, QObject* parent)
    : QObject(parent),
      m_dbPath(statePath + "/replays.db"),
      m_replayDir(statePath + "/replays") {}

void ReplayStore::receiveInitialReplayPaths(const QList<QString>& paths) {
    // Perform initial setup operation on the startup signal
    ensureDirectories();
    ensureDb();
    // Ingest every path that we have
    for (const auto& path : paths) {
        // TODO: Maybe we can avoid making this a ton of transactions
        // We ignore the return value here because its the initial startup.
        // We're emitting everythign at the end
        analyzeReplayFile(path);
    }

    // Now that we've done all the initial processing we will emit the query
    try {
        // What replays did we know about but seem to no longer exist.
        Queries queries{QSqlQuery(m_db)};
        queries.forgetMissingReplays(paths);
        emit replaysLoaded(queries.selectReplays());
    } catch (StorageException& ex) {
        qCritical(logStore) << "Unable to access the replays " << ex.what();
    }
}

void ReplayStore::analyzeReplayFile(const QString& path) {
    qDebug(logStore) << "Analyzing replay: " << path;
    // First thing we do is copy into the staging directory before we run it

    try {
        QFile replayFile(path);
        if (!replayFile.open(QIODevice::ReadOnly)) {
            // TODO: Look at what the failure causes are and see how we can
            // mitigate
            qWarning(logStore) << "Unable to open " << path << " for reading";
            return;
        }
        // TODO: In theory there is a short race condition here where the file
        // is overwritten before we can analyze and copy it, but meh
        const LegionParser::ReplayMetadata metadata =
            LegionParser::Parser::parse(replayFile);

        // Ingest the replay and determine all the checksums that changed
        const QList<QByteArray> impacted = ingestReplay(replayFile, metadata);
        forwardChangedReplays(impacted);

    } catch (LegionParser::ReplayParseException& ex) {
        qWarning(logStore) << "Unable to parse " << path << " " << ex.what();
        // If there was a replay file at this path it should be removed because
        // it was overwritten with nonsense
        try {
            std::optional checksum = removeReplayAtPath(path);
            if (checksum.has_value()) {
                forwardChangedReplays(QList{checksum.value()});
            }
        } catch (StorageException& ex) {
            qCritical(logStore)
                << "Unable to remove corrupt replay " << ex.what();
        }
    } catch (StorageException& ex) {
        // If it fails we should probably do something to mark the replay for
        // future processing
        qCritical(logStore) << "Unable to ingest the replay " << ex.what();
    }
}

void ReplayStore::removeReplayFile(const QString& path) {
    qDebug(logStore) << "Removing replay: " << path;

    try {
        std::optional checksum = removeReplayAtPath(path);
        if (checksum.has_value()) {
            forwardChangedReplays(QList{checksum.value()});
        }
    } catch (StorageException& ex) {
        qCritical(logStore) << "Unable to remove corrupt replay " << ex.what();
    }
}

QList<QByteArray> ReplayStore::ingestReplay(
    QFile& file, const LegionParser::ReplayMetadata& metadata) {
    SqlTransactionGuard tx(m_db);
    Queries queries{QSqlQuery(m_db)};

    QList<QByteArray> impactedChecksums{{metadata.checksum}};

    if (queries.isReplayKnown(metadata.checksum)) {
        // If the replay has been seen before then we need to add a path to it
        qDebug(logStore) << "Existing replay being ingested: "
                         << file.fileName();

        handleExistingReplayAtPath(queries, file.fileName(), impactedChecksums);

        if (!queries.insertExternalFilename(metadata.checksum,
                                            file.fileName())) {
            qDebug(logStore)
                << "Existing replay was already tracked" << file.fileName();
        }

    } else {
        qInfo(logStore) << "New replay being ingested: " << file.fileName();
        // This is the first time the replay has been seen so
        // we need to perform to insert everything
        queries.insertReplay(metadata);
        queries.insertReplayPlayers(metadata.checksum, metadata.players);

        handleExistingReplayAtPath(queries, file.fileName(), impactedChecksums);

        queries.insertExternalFilename(metadata.checksum, file.fileName());

        // Before committing we should copy to the canonical path
        if (!file.copy(computeIngestionPath(metadata.checksum))) {
            qCritical(logStore)
                << "Failed to copy the replay file to the store";
            throw StorageException("failed to copy");
        }
    }

    if (!tx.commit()) {
        qCritical(logStore) << "Failed to commit: " << m_db.lastError().text();
        throw StorageException("failed to commit");
    }

    return impactedChecksums;
}

void ReplayStore::handleExistingReplayAtPath(Queries& queries,
                                             const QString& path,
                                             QList<QByteArray>& checksums) {
    const std::optional<QByteArray> checksum =
        queries.checksumForExternalPath(path);
    if (checksum.has_value() && !checksums.contains(checksum.value())) {
        qDebug(logStore) << "Existing replay at path with different checksum: "
                         << path;
        checksums.append(checksum.value());
    }
}

void ReplayStore::forwardChangedReplays(const QList<QByteArray>& checksums) {
    Queries queries{QSqlQuery(m_db)};

    QList<Replay> replays;
    replays.reserve(checksums.size());
    for (const auto& checksum : checksums) {
        std::optional<Replay> replay = queries.selectReplay(checksum);
        if (!replay.has_value()) {
            qWarning(logStore)
                << "Replay with checksum: " << checksum.toHex()
                << " disappeared before it could be emitted as updated";
        } else {
            replays.append(replay.value());
        }
    }
    emit replaysChanged(replays);
}

QString ReplayStore::computeIngestionPath(const QByteArray& checksum) const {
    const QString filename =
        QString::fromLatin1(checksum.toHex()) + ".KWReplay";
    return QDir(m_replayDir).filePath(filename);
}

std::optional<QByteArray> ReplayStore::removeReplayAtPath(const QString& path) {
    SqlTransactionGuard tx(m_db);
    Queries queries{QSqlQuery(m_db)};

    std::optional result = queries.removeExternalFilename(path);

    if (!tx.commit()) {
        throw StorageException("failed to commit: " + m_db.lastError().text());
    }
    return result;
}

void ReplayStore::ensureDb() {
    if (m_db.isOpen()) {
        return;
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE", "kwlegion_store");
    m_db.setDatabaseName(m_dbPath);
    if (!m_db.open()) {
        qCCritical(logStore)
            << "Failed to open database: " << m_db.lastError().text();
        return;
    }

    // TODO: Do we need some kind of internally broken structure?
    SqlTransactionGuard tx(m_db);
    Queries queries{QSqlQuery(m_db)};
    if (!queries.migrate()) {
        qCritical(logStore)
            << "Failed to migrate database: " << m_db.lastError().text();
    }
    if (!tx.commit()) {
        qCritical(logStore)
            << "Failed to migrate database: " << m_db.lastError().text();
    }
}

void ReplayStore::ensureDirectories() {
    if (!QDir().mkpath(m_replayDir)) {
        if (QDir().exists(m_replayDir)) {
            qDebug(logStore)
                << "Replay directory already exists: " << m_replayDir;
        } else {
            qCritical(logStore)
                << "Unable to create replay storage directory: " << m_replayDir;
        }
    } else {
        qDebug(logStore) << "Replay directory created: " << m_replayDir;
    }
}
}  // namespace KWLegionCore