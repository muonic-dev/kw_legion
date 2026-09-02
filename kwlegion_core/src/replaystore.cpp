// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include <kwlegion_core/inboxitem.h>
#include <kwlegion_core/replaystore.h>
#include <legionparser/parser.h>

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <stdexcept>

#include "deferred.h"
#include "exception.h"
#include "queries.h"
#include "transaction.h"

Q_LOGGING_CATEGORY(logStore, "kwlegion.store");

namespace KWLegionCore {

// The poll itself is only a stat per deferred path, but every poll that sees
// a change spends a full parse - and a torn parse hashes the whole file
// before verifyFooter rejects it. During a match the file changes constantly,
// so this interval really does set the re-read rate.
constexpr int RECHECK_INTERVAL_MS = 15000;

namespace {
// Inbox items are derived from what we just observed on disk rather than
// read back from storage, so the observation time is simply now.
InboxItem makeInboxItem(const QString& path, InboxType type) {
    return InboxItem{
        .path = path,
        .type = type,
        .observedAt = QDateTime::currentDateTimeUtc(),
    };
}
}  // namespace

ReplayStore::ReplayStore(QString replayDir, const QString& statePath,
                         QObject* parent)
    : QObject(parent),
      m_dbPath(statePath + "/replays.db"),
      m_storageDir(statePath + "/replays"),
      m_replayDir(std::move(replayDir)),
      m_deferred(new Deferred(this)) {
    m_deferred->setRecheckIntervalMs(RECHECK_INTERVAL_MS);
    // Send it back throught the analyze replay file
    QObject::connect(m_deferred, &Deferred::pathChanged, this,
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

        const QString matchTitle = replay->overrideMatchTitle.isEmpty()
                                       ? replay->matchTitle
                                       : replay->overrideMatchTitle;

        const QString name =
            QString("%1 - %2.KWReplay")
                .arg(matchTitle, QString(checksum.toHex()).slice(0, 8));
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

void ReplayStore::clearOverrideTitle(const QByteArray& checksum) {
    setOverrideTitle(checksum, QStringLiteral(""));
}
void ReplayStore::setOverrideTitle(const QByteArray& checksum,
                                   const QString& title) {
    try {
        std::optional<Replay> replay;
        SqlTransactionGuard guard(m_db);
        Queries queries{QSqlQuery(m_db)};
        queries.updateOverrideTitle(checksum, title);
        replay = queries.selectReplay(checksum);
        guard.commit();
        if (replay.has_value()) {
            emit replaysChanged(QList{replay.value()});
        }  // else unlikely but not forbidden by types
    } catch (StorageException& ex) {
        qCritical(logStore) << "Failed to store override title for "
                            << QString(checksum.toHex());
    }
}

void ReplayStore::receiveInitialReplayPaths(const QList<QString>& paths) {
    // Perform initial setup operation on the startup signal
    ensureDirectories();
    ensureDb();

    // Inbox contents are derived from what is on disk rather than persisted,
    // so clear whatever the UI is holding before the sweep below repopulates
    // it from the paths we actually find.
    emit inboxReset();

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

void ReplayStore::removeReplayFile(const QString& path) {
    // Nothing left to watch for - otherwise the entry would keep getting
    // polled and re-reported as an empty pending file.
    m_deferred->removeWaitForChange(path);
    removeReplayFileLink(path);
    emit inboxItemRemoved(path);
}

void ReplayStore::analyzeReplayFile(const QString& path) {
    // We were waiting but also a file notification happened
    m_deferred->removeWaitForChange(path);

    // Sampled before the parse attempt deliberately: if the file grows while
    // we are reading it, a sample taken afterwards would record bytes we
    // never parsed and we would then wait for a change that already happened.
    const Watermark observed = Deferred::sample(path);

    if (observed.size == 0) {
        // Nothing to hand the parser yet. Wait for the file to grow rather
        // than guessing at how long a writer needs.
        qDebug(logStore) << "path " << path << " is empty, deferring";
        removeReplayFileLink(path);
        m_deferred->waitForChange(path, observed);
        emit inboxItemObserved(makeInboxItem(path, InboxType::PENDING));
        return;
    }

    try {
        performReplayAnalysis(path);
        // Success = not pending
        emit inboxItemRemoved(path);
    } catch (const LegionParser::TornDataException& ex) {
        handleTornFailure(path, observed);
    } catch (const LegionParser::ReplayParseException& ex) {
        handleParseFailure(ex, path);
    } catch (StorageException& ex) {
        // TODO: This should eventually be noisy through the ingestionmodel
        // signalling
        // TODO: Think about under what circumstances we can try again later or
        // determine if we should give up
        qCritical(logStore) << "db or i/o error occurred " << ex.what();
    }
}

void ReplayStore::handleTornFailure(const QString& path,
                                    const Watermark& observed) noexcept {
    // If the replay cannot be parsed then whatever then
    // the link to an existing replay needs to be broken
    qDebug(logStore) << "path " << path << " is incomplete";
    // No throw since we get external calls
    removeReplayFileLink(path);

    // The parser is the authority on whether the file is complete, and it
    // just said no - so there is nothing to learn until the bytes on disk
    // actually move.
    m_deferred->waitForChange(path, observed);
    emit inboxItemObserved(makeInboxItem(path, InboxType::TORN));
}

void ReplayStore::handleParseFailure(
    const LegionParser::ReplayParseException& ex,
    const QString& path) noexcept {
    // The replay is terminally invalid, so just remove it. Unlike a torn
    // replay this doesn't go back into the deferred set - nothing is going
    // to un-corrupt the file, so retrying only burns reads.
    // TODO: ReplayParseException includes potential IO failures which may
    // be transient in addition to CorruptDataException
    qInfo(logStore) << "unable to parse " << ex.what();
    // No throw since we get external calls
    removeReplayFileLink(path);
    emit inboxItemObserved(makeInboxItem(path, InboxType::CORRUPT));
}

void ReplayStore::removeReplayFileLink(const QString& path) {
    qDebug(logStore) << "Removing valid replay link: " << path;
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

void ReplayStore::acknowledgeItem(const QString& path) {
    // Dismissal is scoped to this session. Inbox state is derived from what
    // is on disk, and the paths that reach the inbox include the game's
    // rolling "Last Replay.KWReplay" - a persisted, path-keyed dismissal
    // would permanently silence the single most frequently rewritten path in
    // the folder. A corrupt file left in place is reported again next launch.
    emit inboxItemRemoved(path);
}

void ReplayStore::performReplayAnalysis(const QString& path) {
    qDebug(logStore) << "Analyzing replay: " << path;
    QFile replayFile(path);
    if (!replayFile.open(QIODevice::ReadOnly)) {
        // We may need to figure out how to recover from this such as by locked
        // files
        qWarning(logStore) << "Unable to open " << path << " for reading";
        throw StorageException(replayFile.errorString());
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

    tx.commit();

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

    tx.commit();
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
    try {
        // Outside a transaction so that we do as much as we can
        // if we ever ship a broken migration this means there is less to do
        Queries queries{QSqlQuery(m_db)};
        if (!queries.migrate()) {
            qCritical(logStore)
                << "Failed to migrate database: " << m_db.lastError().text();
        }
    } catch (const StorageException& ex) {
        qCritical(logStore) << "Failed to migrate database: " << ex.what();
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