/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <legionparser/replay.h>

#include <QByteArray>
#include <QList>
#include <QSqlQuery>
#include <QString>
#include <array>

namespace KWLegionCore {

inline constexpr std::array MIGRATIONS{
    // Store the replay here
    // Note: we use the checksum to compute a stored local state path
    R"(CREATE TABLE replays
        ( checksum BLOB PRIMARY KEY
        , match_title TEXT NOT NULL
        , match_description TEXT NOT NULL
        , map_name TEXT NOT NULL
        , map_id TEXT NOT NULL
        , game_type INT NOT NULL
        , timestamp INT NOT NULL
        , has_commentary INT NOT NULL
        , filename TEXT NOT NULL
        , map_reference TEXT NOT NULL
        , version_major INT
        , version_minor INT
        , build_major INT
        , build_minor INT
        ) STRICT, WITHOUT ROWID;
    )",

    // Replays might occur more than once in the replay folder for whatever
    // reason so a single storage is not good enough
    // external_path is a full canonical path
    R"(CREATE TABLE replay_external_paths
        ( replay_checksum BLOB NOT NULL
        , external_path TEXT NOT NULL
        , PRIMARY KEY(replay_checksum, external_path)
        ) STRICT, WITHOUT ROWID;
    )",

    R"(CREATE TABLE replay_players
        ( replay_checksum INT NOT NULL
        , player_id INT NOT NULL
        , player_name TEXT NOT NULL
        , team_number INT
        , faction INT NOT NULL
        , is_computer INT NOT NULL
        , is_replay_saver INT NOT NULL
        , PRIMARY KEY(replay_checksum, player_id)
        ) STRICT, WITHOUT ROWID;
    )",
};

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

    bool isReplayKnown(const QByteArray& checksum);

    void insertReplay(const LegionParser::ReplayMetadata& metadata);

    void insertReplayPlayers(const QByteArray& checksum,
                             const QList<LegionParser::Player>& players);

   private:
    void prepare(const QString& sql);
    void exec();
    void execBatch();
    void throwLast() const;

    QSqlQuery m_query;
};

}  // namespace KWLegionCore
