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

ReplayStore::ReplayStore(QObject* parent)
    : QObject(parent),
      m_dbPath(QStandardPaths::writableLocation(QStandardPaths::StateLocation) +
               "/replays.db"),
      m_replayDir(
          QStandardPaths::writableLocation(QStandardPaths::StateLocation) +
          "/replays") {}

void ReplayStore::startup() {
    ensureDirectories();
    ensureDb();
}

void ReplayStore::receiveReplay(const QString& path) {
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
        ingestReplay(replayFile, metadata);
    } catch (LegionParser::ReplayParseException& ex) {
        qWarning(logStore) << "Unable to parse " << path << " " << ex.what();
    } catch (IngestionException& ex) {
        // If it fails we should probably do something to mark the replay for
        // future processing
        qCritical(logStore) << "Unable to ingest the replay " << ex.what();
    }
}

void ReplayStore::ingestReplay(const QFile& file,
                               const LegionParser::ReplayMetadata& metadata) {
    SqlTransactionGuard tx(m_db);
    Queries queries{QSqlQuery(m_db)};

    if (queries.isReplayKnown(metadata.checksum)) {
        // If the replay has been seen before then we need to add a path to it
    } else {
        // This is the first time the replay has been seen so we need to perform
        // to insert everything
        queries.insertReplay(metadata);
        queries.insertReplayPlayers(metadata.checksum, metadata.players);
        queries.insertExternalFilename(metadata.checksum, file.fileName());

        // TODO: If it fails we should pro
    }

    tx.commit();
}

QString ReplayStore::computeIngestionPath(const QByteArray& checksum) const {
    const QString filename =
        QString::fromLatin1(checksum.toHex()) + ".KWReplay";
    return QDir(m_replayDir).filePath(filename);
}

void ReplayStore::ensureDb() {
    if (m_db.isOpen()) {
        return;
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE", "kwlegion_store");
    m_db.setDatabaseName(m_dbPath);
    if (!m_db.open()) {
        qCCritical(logStore)
            << "Failed to open database: " << m_db.lastError().driverText()
            << " " << m_db.lastError().databaseText();
        return;
    }

    Queries::migrate(m_db);
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