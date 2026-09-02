/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <kwlegion_core/replay.h>
#include <legionparser/replay.h>

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QSqlQuery>
#include <QString>
#include <array>
#include <optional>

namespace KWLegionCore {

// Helper utility class for dispatching queries. Kept alongside MIGRATIONS so
// the DDL and the statements that reference it stay adjacent.
class Queries final {
   public:
    explicit Queries(QSqlQuery&& query) : m_query(std::move(query)) {}

    ~Queries() = default;

    Queries(const Queries&) = delete;
    Queries(Queries&&) = delete;

    Queries& operator=(const Queries&) = delete;
    Queries& operator=(Queries&&) = delete;

    // Returns false on failure. Caller should inspect the db for whatever the
    // error was
    bool migrate();

    bool isReplayKnown(const QByteArray& checksum);

    void insertReplay(const LegionParser::ReplayMetadata& metadata);

    void updateOverrideTitle(const QByteArray& checksum,
                             const QString& overrideTitle);

    void insertReplayPlayers(const QByteArray& checksum,
                             const QList<LegionParser::Player>& players);

    // The checksum currently registered for path, if the path is tracked at
    // all.
    std::optional<QByteArray> checksumForExternalPath(const QString& path);

    // Insert an external filename, returns true when the file is new or was
    // reassigned from a different checksum (e.g. the game's rolling
    // "Last Replay.KWReplay" being overwritten with a new match) - a path
    // can only ever belong to one checksum, so this atomically reassigns it
    // rather than erroring or leaving the old registration in place.
    bool insertExternalFilename(const QByteArray& checksum,
                                const QString& path);

    // The content at path is not longer a valid replay so remove it
    // Returns the hash of the replay that was dropped (if any)
    std::optional<QByteArray> removeExternalFilename(const QString& path);

    // Forget replays that don't exist in current paths
    // These are replays that did exist in external paths but we should
    // dump
    void forgetMissingReplays(const QList<QString>& currentPaths);

    QList<Replay> selectReplays();

    std::optional<Replay> selectReplay(const QByteArray& checksum);

    QList<Player> selectReplayPlayers(const QByteArray& checksum);

    QList<QString> selectExternalPaths(const QByteArray& checksum);

    void bootstrapMutationTable(const QList<QString>& values);

   private:
    void prepare(const QString& sql);

    // Create a temporary table of text to work around max bound parameters
    // This is used in several query helpers so standardize it here
    void bootstrapTemporaryTable(const QList<QString>& values);

    void exec();

    void execBatch();

    void throwLast() const;

    void throwLastIfFailed() const;

    void nextOrThrow();

    [[nodiscard]] Replay readReplay() const;

    QSqlQuery m_query;
};

}  // namespace KWLegionCore
