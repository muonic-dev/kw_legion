// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include <kwlegion_core/store.h>
#include <legionparser/parser.h>

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <stdexcept>

#include "deferred.h"
#include "exception.h"
#include "problems.h"
#include "queries.h"
#include "transaction.h"

Q_LOGGING_CATEGORY(logStore, "kwlegion.store");

namespace KWLegionCore {

constexpr int FIVE_SECONDS_MS = 5000;

ReplayStore::ReplayStore(QString replayDir, const QString& statePath,
                         QObject* parent)
    : QObject(parent),
      m_dbPath(statePath + "/replays.db"),
      m_storageDir(statePath + "/replays"),
      m_replayDir(std::move(replayDir)),
      m_deferred(new Deferred(this)) {
    m_deferred->setRecheckIntervalMs(FIVE_SECONDS_MS);
    // Send it back throught the analyze replay file
    QObject::connect(m_deferred, &Deferred::pathReady, this,
                     &ReplayStore::analyzeReplayFile);
}

void ReplayStore::stop() {
    // Silence a warning about stopping the time
    // Trigger from stopping on the thread
    m_deferred->stop();
}

void ReplayStore::saveReplayAs(const QByteArray& checksum, const QUrl& path) {
    // TODO: There is no i/o failure diagnostic here. We need some way of
    // propogating the result back to the ui thread...
    Queries queries{QSqlQuery(m_db)};
    if (!queries.selectReplay(checksum)) {
        qWarning(logStore) << "Replay " << QLatin1String(checksum.toHex())
                           << " doesn't exist";
    }
    const QString expectedLocation = computeIngestionPath(checksum);
    const QString outputPath = path.toLocalFile();
    if (!QFile::copy(expectedLocation, outputPath)) {
        qCritical(logStore)
            << "Failed to copy " << expectedLocation << " to " << outputPath;
    } else {
        qInfo(logStore) << "Copied " << expectedLocation << " to "
                        << outputPath;
    }
}

void ReplayStore::exportReplaysAs(const QList<QByteArray>& checksums,
                                  const QUrl& folderPath) {
    Queries queries{QSqlQuery(m_db)};
    for (const auto& checksum : checksums) {
        const auto replay = queries.selectReplay(checksum);
        if (!replay.has_value()) {
            qWarning(logStore) << "Replay " << QLatin1String(checksum.toHex())
                               << " doesn't exist";
            continue;
        }

        const QString name =
            QString("%1 - %2.KWReplay")
                .arg(replay->matchTitle, QString(checksum.toHex()).slice(0, 8));
        const QString outputPath = folderPath.toLocalFile() + "/" + name;

        const QString expectedLocation = computeIngestionPath(checksum);
        if (!QFile::copy(expectedLocation, outputPath)) {
            qCritical(logStore) << "Failed to copy " << expectedLocation
                                << " to " << outputPath;
        } else {
            qInfo(logStore)
                << "Copied " << expectedLocation << " to " << outputPath;
        }
    }
}

void ReplayStore::receiveInitialReplayPaths(const QList<QString>& paths) {
    // Perform initial setup operation on the startup signal
    ensureDirectories();
    ensureDb();
    // Bracket analyzeReplayFile so that we don't constantly emit the individual
    // load action
    const auto guard = m_initialSweep.enter();
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
        QList<Replay> replays = queries.selectReplays();

        for (Replay& replay : replays) {
            replay.players = queries.selectReplayPlayers(replay.checksum);
        }

        emit replaysLoaded(replays);
    } catch (StorageException& ex) {
        qCritical(logStore) << "Unable to access the replays " << ex.what();
    }
}

void ReplayStore::analyzeReplayFile(const QString& path) {
    try {
        if (Deferred::readyForParsing(path)) {
            performReplayAnalysis(path);
        } else {
            // If the replay cannot be parsed then whatever then
            // the link to an existing replay needs to be broken
            qDebug(logStore)
                << "path " << path << " has not settled, deferring";
            removeReplayFileLink(path);
            m_deferred->waitForReady(path);
        }
    } catch (const LegionParser::TornDataException& ex) {
        handleTornFailure(path);
    } catch (const LegionParser::ReplayParseException& ex) {
        handleParseFailure(ex, path);
    } catch (StorageException& ex) {
        // TODO: This should eventually be noisy
        // TODO: If we catch a StorageException we should also retry in the hope
        // that whatever happened is transient. Need finer grained on what
        // errors happened There's really no point in trying to
        // removeReplayAtPath that we couldn't ingest if we just failed a sqlite
        // commit.
        qCritical(logStore) << "db or i/o error occurred " << ex.what();
    }
}

void ReplayStore::handleTornFailure(const QString& path) noexcept {
    // If the replay cannot be parsed then whatever then
    // the link to an existing replay needs to be broken
    // Also send this back through the deferred logic
    qDebug(logStore) << "path " << path << " is incomplete, deferring";
    // No throw since we get external calls
    removeReplayFileLink(path);

    try {
        auto now = QDateTime::currentDateTimeUtc();
        const ProblemRecord actual = handleProblem(ProblemRecord{
            .path = path, .noticedAt = now, .type = ProblemType::TORN});
        if (shouldGiveUp(actual, now)) {
            qWarning(logStore) << "giving up on repeatedly torn path " << path;
            return;
        }
    } catch (const StorageException& ex) {
        qWarning(logStore) << "unable to write problem marker: " << ex.what();
        // fall through to retry again
    }

    m_deferred->waitForReady(path);
}
void ReplayStore::handleParseFailure(
    const LegionParser::ReplayParseException& ex,
    const QString& path) noexcept {
    // The replay is terminally invalid, so just remove it
    // TODO: Surface this in the UI
    // TODO: ReplayParseException includes potential IO failures which may
    // be transient in addition to CorruptDataException
    qInfo(logStore) << "unable to parse " << ex.what();
    // No throw since we get external calls
    removeReplayFileLink(path);
    try {
        auto now = QDateTime::currentDateTimeUtc();
        handleProblem(ProblemRecord{
            .path = path, .noticedAt = now, .type = ProblemType::CORRUPT});
    } catch (const StorageException& ex) {
        qWarning(logStore) << "unable to write problem marker: " << ex.what();
    }
}

ProblemRecord ReplayStore::handleProblem(const ProblemRecord& problem) {
    SqlTransactionGuard tx(m_db);
    Queries queries{QSqlQuery(m_db)};

    ProblemRecord result = queries.insertPathProblem(problem);

    if (!tx.commit()) {
        throw StorageException("commit failed: " + m_db.lastError().text());
    }

    return result;
}

void ReplayStore::removeReplayFileLink(const QString& path) noexcept {
    qDebug(logStore) << "Removing replay: " << path;
    try {
        forwardChangedReplays(removeReplayAtPath(path));
    } catch (StorageException& ex) {
        qCritical(logStore) << "Unable to remove invalid replay " << ex.what();
    }
}

void ReplayStore::toggleReplayExposed(const QByteArray& checksum) {
    qInfo(logStore) << "toggleReplayExposed "
                    << QLatin1String(checksum.toHex());
    // We are toggling, so we need to determine whether or not the replay has
    // external paths We are going to do this whole thing in a transaction so
    // nothing else changes underneath us
    try {
        SqlTransactionGuard tx(m_db);
        Queries queries{QSqlQuery(m_db)};

        const std::optional<Replay> replay = queries.selectReplay(checksum);
        if (!replay.has_value()) {
            qWarning(logStore) << "cannot toggle exposed for unknown replay "
                               << QLatin1String(checksum.toHex());
            return;
        }
        if (replay->hasExternalPath) {
            hideReplay(queries, checksum);
        } else {
            exposeReplay(checksum);
        }

        // We just want to make sure nothing is changing underneath, we allow
        // updates to flow in via signals
        tx.rollback();

    } catch (std::runtime_error& ex) {
        qCritical(logStore) << "failed to toggle: " << ex.what();
    }
}

void ReplayStore::ensureReplayExposed(const QByteArray& checksum) {
    qInfo(logStore) << "ensureReplayExposed "
                    << QLatin1String(checksum.toHex());
    try {
        exposeReplay(checksum);
    } catch (std::runtime_error& ex) {
        qCritical(logStore) << "failed to expose: " << ex.what();
    }
}

void ReplayStore::ensureReplayHidden(const QByteArray& checksum) {
    qInfo(logStore) << "ensureReplayHidden " << QLatin1String(checksum.toHex());
    try {
        Queries queries{QSqlQuery(m_db)};
        hideReplay(queries, checksum);
    } catch (std::runtime_error& ex) {
        qCritical(logStore) << "failed to hide: " << ex.what();
    }
}

void ReplayStore::exposeReplay(const QByteArray& checksum) {
    if (!QDir(m_replayDir).mkpath("managed")) {
        throw StorageException("failed to create managed/ replay folder");
    }
    const QString ingestedPath = computeIngestionPath(checksum);
    const QString targetPath = QString("%1/managed/%2.KWReplay")
                                   .arg(m_replayDir, QString(checksum.toHex()));
    // Assume this exists
    if (!QFile::copy(ingestedPath, targetPath)) {
        throw StorageException("failed to copy into managed/ folder");
    }
}

void ReplayStore::hideReplay(Queries& queries, const QByteArray& checksum) {
    // Delete everything that we load as external
    auto paths = queries.selectExternalPaths(checksum);
    for (const auto& path : paths) {
        QFile file(path);
        if (!file.remove()) {
            qCritical(logStore)
                << "Failed to remove file: " << file.errorString();
        }
    }
}

bool ReplayStore::shouldGiveUp(const ProblemRecord& problem,
                               const QDateTime& now) {
    const QDateTime cutoff = now.addSecs(-60LL * 60LL);
    return problem.noticedAt < cutoff;
}

void ReplayStore::performReplayAnalysis(const QString& path) {
    qDebug(logStore) << "Analyzing replay: " << path;
    QFile replayFile(path);
    if (!replayFile.open(QIODevice::ReadOnly)) {
        // We may need to figure out how to recover from this such as by locked
        // files
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
}

QList<QByteArray> ReplayStore::ingestReplay(
    QFile& file, const LegionParser::ReplayMetadata& metadata) {
    SqlTransactionGuard tx(m_db);
    Queries queries{QSqlQuery(m_db)};

    // If we have a successful parse at this point we are in a transaction
    // so also clear any failed parses
    queries.clearPathProblems(file.fileName());

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
    // Block emission here since the initial sweep does one large thing at the
    // end
    if (m_initialSweep.isActive()) {
        return;
    }
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
            Replay r = replay.value();
            r.players = queries.selectReplayPlayers(r.checksum);
            replays.append(r);
        }
    }
    emit replaysChanged(replays);
}

void ReplayStore::forwardChangedReplays(
    const std::optional<QByteArray>& checksum) {
    if (checksum.has_value()) {
        forwardChangedReplays(QList{checksum.value()});
    }
}

QString ReplayStore::computeIngestionPath(const QByteArray& checksum) const {
    const QString filename =
        QString::fromLatin1(checksum.toHex()) + ".KWReplay";
    return QDir(m_storageDir).filePath(filename);
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
    if (!QDir().mkpath(m_storageDir)) {
        if (QDir().exists(m_storageDir)) {
            qDebug(logStore)
                << "Replay directory already exists: " << m_storageDir;
        } else {
            qCritical(logStore) << "Unable to create replay storage directory: "
                                << m_storageDir;
        }
    } else {
        qDebug(logStore) << "Replay directory created: " << m_storageDir;
    }
}
}  // namespace KWLegionCore