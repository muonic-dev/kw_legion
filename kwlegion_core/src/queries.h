// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#pragma once

#include <kwlegion_core/replay.h>
#include <legionparser/replay.h>

#include <QList>
#include <QSqlQuery>
#include <optional>

#include "externalreplayfile.h"

namespace KWLegionCore {

class Persistence;

// SQL statements and migrations for Persistence's database connection.
class Queries final {
   public:
    // Requires successful Persistence::init(); valid until close or
    // destruction.
    static Queries& get(Persistence& persistence);

    explicit Queries(const QSqlDatabase& db);

    // Runs any pending migrations. Throws StorageException on failure.
    void migrate();

    // Return a list of all external replay files
    QList<ExternalReplayFile> selectKnownExternalReplayFiles();

    // Is an external replay known at the given path with the given
    // mtime and filesize
    bool isReplayKnown(const QString& path, const QDateTime& mtime,
                       qsizetype filesize);

    // Is a replay known internally via checksum (used to perform linking)
    bool isReplayKnown(const QByteArray& checksum);

    // Determine if a replay needs its body reanalyzed (for things like offset
    // and engine ticks)
    bool doesReplayNeedAnalysis(const QByteArray& checksum);

    // Select the checksum of all replays that need to be re-analyzed on a body
    // pass
    QList<QByteArray> selectReplaysNeedingAnalysis();

    void insertReplay(const LegionParser::ReplaySynopsis& metadata);

    // Handle when the replay is already known
    void insertReplayAnalysis(const LegionParser::ReplaySynopsis& metadata);

    void updateOverrideTitle(const QByteArray& checksum,
                             const QString& overrideTitle);

    // Insert the players of a replay
    // The order received will be tracked so that analysis can use it
    void insertReplayPlayers(const QByteArray& checksum,
                             const QList<LegionParser::Player>& players);

    // The checksum currently registered for path, if the path is tracked at
    // all.
    std::optional<QByteArray> checksumForExternalPath(const QString& path);

    // Insert an external filename, returns true when its fingerprint changed,
    // the file is new, or it was
    // reassigned from a different checksum (e.g. the game's rolling
    // "Last Replay.KWReplay" being overwritten with a new match) - a path
    // can only ever belong to one checksum, so this atomically reassigns it
    // rather than erroring or leaving the old registration in place.
    bool insertExternalFilename(const QByteArray& checksum, const QString& path,
                                const QDateTime& mtime, qsizetype size);

    // The content at path is not longer a valid replay so remove it
    // Returns the hash of the replay that was dropped (if any)
    std::optional<QByteArray> removeExternalFilename(const QString& path);

    // Forget replays that don't exist in current paths
    // These are replays that did exist in external paths but we should
    // dump
    void forgetMissingReplays(const QList<QString>& currentPaths);

    QList<Replay> selectReplays();

    std::optional<Replay> selectReplay(const QByteArray& checksum);

    // Select the players in a replay
    // This is guaranteed to maintain the same order as the player insert does
    // so that player ordering is stable for doing replay analysis
    QList<Player> selectReplayPlayers(const QByteArray& checksum);

    QList<QString> selectExternalPaths(const QByteArray& checksum);

    QList<QByteArray> selectReplaysNeedingRechecksum();

    void deleteReplay(const QByteArray& checksum);
    void deleteReplayAnalysis(const QByteArray& checksum);
    void deleteReplayOverrides(const QByteArray& checksum);
    void deleteReplayPlayers(const QByteArray& checksum);
    void deleteReplayExternalPaths(const QByteArray& checksum);

    // Assumes that there is no existing replay with the new checksum
    void migrateReplayChecksum(const QByteArray& oldChecksum,
                               const QByteArray& newChecksum);
    void migrateReplayAnalysis(const QByteArray& oldChecksum,
                               const QByteArray& newChecksum);
    void migrateReplayOverrides(const QByteArray& oldChecksum,
                                const QByteArray& newChecksum);
    void migrateReplayPlayers(const QByteArray& oldChecksum,
                              const QByteArray& newChecksum);
    void migrateReplayExternalPaths(const QByteArray& oldChecksum,
                                    const QByteArray& newChecksum);

    void markReplayForRechecksum(const QByteArray& checksum, bool rechecksum);

   private:
    void bootstrapMutationTable(const QList<QString>& values);

    void prepare(const QString& sql);

    void exec();

    void execBatch();

    void throwLast() const;

    void throwLastIfFailed() const;

    void nextOrThrow();

    [[nodiscard]] Replay readReplay() const;

    QSqlQuery m_query;
};

}  // namespace KWLegionCore
