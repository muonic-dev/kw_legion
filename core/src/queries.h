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

    void insertExternalFilename(const QByteArray& checksum,
                                const QString& path);

    // Attempt to perform migrations
    // Fails and logs when issues occur
    static void migrate(const QSqlDatabase& db);

   private:
    void prepare(const QString& sql);
    void exec();
    void execBatch();
    void throwLast() const;

    QSqlQuery m_query;
};

}  // namespace KWLegionCore
