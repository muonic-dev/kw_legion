// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#pragma once

#include <kwlegion_core/actionscope.h>
#include <kwlegion_core/replay.h>
#include <legionparser/replay.h>

#include <QDir>
#include <QLoggingCategory>
#include <QObject>
#include <QSet>
#include <QSqlDatabase>
#include <QStandardPaths>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <tuple>

Q_DECLARE_LOGGING_CATEGORY(logStore);

// TODO: Ideally we wouldn't leak forward declarations into the public interface
// but for now this is what I've got
namespace LegionParser {
class ReplayParseException;
class TornDataException;
}  // namespace LegionParser

namespace KWLegionCore {
class Queries;
class Deferred;
struct ProblemRecord;
class InboxItem;

class ReplayStore : public QObject {
    Q_OBJECT

    /* Replay store operation is as follows
     * Initially (at startup time) the set of initial paths in the replay
     * directory will be received from the prospector
     *
     * Subsequently, each new file will come in via an
     * analyzeReplayFile/removeReplayFileLink
     *
     * Internally, performReplayAnalysis does the parsing
     * This is used both on initial load and on periodic
     * updates. The m_initialSweep is a gate to control
     * whether periodic emission happens. This prevents
     * view jitter since at the end of receiveInitialPaths
     * there is a bulk emission of all replays anyway.
     */

   public:
    ReplayStore(QString replayDir,
                const QString& statePath = QStandardPaths::writableLocation(
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
     * A replay file has disappeared or is overwritten with incomplete data
     */
    void removeReplayFileLink(const QString& path) noexcept;

    /**
     * Expose a replay to the Kane's Wrath replay folder by checksum
     *
     * As a side efect this will trigger a replaysChanged
     */
    void toggleReplayExposed(const QByteArray& checksum);

    /**
     * Ensure a replay is exposed regardless of its current state.
     *
     * Unlike toggleReplayExposed, this is idempotent, which matters for bulk
     * actions: a selection can contain a mix of already-exposed and hidden
     * replays, and toggling the already-exposed ones would hide them instead.
     */
    void ensureReplayExposed(const QByteArray& checksum);

    /**
     * Ensure a replay is hidden regardless of its current state. See
     * ensureReplayExposed for why this needs to be distinct from toggling.
     */
    void ensureReplayHidden(const QByteArray& checksum);

    void saveReplayAs(const QByteArray& checksum, const QUrl& path);
    void exportReplaysAs(const QList<QByteArray>& checksums,
                         const QUrl& folderPath);

    void stop();

   signals:
    void replaysLoaded(const QList<Replay>&);
    // A replay was was discovered or updated
    void replaysChanged(const QList<Replay>&);
    // A replay that did exist disappeared
    void replayRemoved(const QByteArray&);

    // Listeners should purge anything they are currently tracking on
    // the inbox. Full data reload
    void inboxReset();
    // A new inbox item was observed. This path may already have been
    // observed with a different type. Will be emitted in a sequence during
    // startup to populate the list
    void inboxItemObserved(const InboxItem& item);
    // Any inbox items at this path should no longer be displayed.
    void inboxItemRemoved(const QString& path);

   private:
    void ensureDb();
    void ensureDirectories();

    // Perform the actual replay analysis
    // This is the happy path for parsing and ingestion
    // It can throw ReplayParseException or StorageException
    // It is provided here because we want a slot entrypoint
    // from the prospector (analyzeReplayFile)
    // But we also want an entrypoint for the deferred retry logic
    // that will share the logic
    void performReplayAnalysis(const QString& path);

    void handleTornFailure(const QString& path) noexcept;
    // Assumes that ReplayParseException is disjoint from TornDataException
    void handleParseFailure(const LegionParser::ReplayParseException& ex,
                            const QString& path) noexcept;

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

    void forwardChangedReplays(const std::optional<QByteArray>& checksum);

    [[nodiscard]] QString computeIngestionPath(
        const QByteArray& checksum) const;

    void exposeReplay(const QByteArray& checksum);

    ProblemRecord handleProblem(const ProblemRecord& problem);

    static void hideReplay(Queries& queries, const QByteArray& checksum);

    // Don't keep trying on a replay
    static bool shouldGiveUp(const ProblemRecord& problem,
                             const QDateTime& now);

    // We want to wait until full analysis is done on all replays before we
    // emit the first event instead of trickling them in with analyze
    // Allow suppressing the emission on the initial sweeep
    ActionScope m_initialSweep;

    QSqlDatabase m_db;
    // Path of the sqlite database
    QString m_dbPath;
    // Path of the internal replay storage
    QString m_storageDir;
    // Path to the Documents\Command &...\Replays dir
    QString m_replayDir;

    // The deferred path parsing process
    Deferred* m_deferred;
};
}  // namespace KWLegionCore