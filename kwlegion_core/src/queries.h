/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <kwlegion_core/replay.h>
#include <legionparser/replay.h>

#include <QByteArray>
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

    void insertReplayPlayers(const QByteArray& checksum,
                             const QList<LegionParser::Player>& players);

    // Insert an external filename, returns true when the file is new
    bool insertExternalFilename(const QByteArray& checksum,
                                const QString& path);

    // If a replay's content changes at the same external path (e.g. the
    // game's rolling "Last Replay.KWReplay" gets overwritten with a new
    // match), the path may still be registered against the checksum of what
    // used to live there. Removes all registrations of path under any
    // checksum other than the given (current) one. Returns true when a
    // stale registration was actually removed.
    bool removeStaleExternalFilename(const QByteArray& checksum,
                                     const QString& path);

    // Forget replays that don't exist in current paths
    // These are replays that did exist in external paths but we should
    // dump
    void forgetMissingReplays(const QList<QString>& currentPaths);

    QList<Replay> selectReplays();

    std::optional<Replay> selectReplay(const QByteArray& checksum);

   private:
    void prepare(const QString& sql);

    void exec();

    void execBatch();

    void throwLast() const;

    void throwLastIfFailed() const;

    [[nodiscard]] Replay readReplay() const;

    QSqlQuery m_query;
};

}  // namespace KWLegionCore
