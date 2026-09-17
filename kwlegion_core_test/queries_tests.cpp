// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTimeZone>
#include <algorithm>
#include <catch2/catch_test_macros.hpp>

#include "queries.h"

using namespace KWLegionCore;

namespace {

void insertExternalReplayFile(QSqlDatabase& db, const QByteArray& checksum,
                              const QString& path, const QDateTime& mtime,
                              qsizetype size) {
    QSqlQuery query(db);
    REQUIRE(
        query.prepare("INSERT INTO replay_external_paths "
                      "(replay_checksum, external_path, mtime, size) "
                      "VALUES (:checksum, :path, :mtime, :size)"));
    query.bindValue(":checksum", checksum);
    query.bindValue(":path", path);
    query.bindValue(":mtime", mtime.toMSecsSinceEpoch());
    query.bindValue(":size", size);
    REQUIRE(query.exec());
}

}  // namespace

TEST_CASE(
    "Queries isReplayKnown matches the complete external file fingerprint",
    "[queries][sql][external-path]") {
    const QString connectionName = "queries_is_replay_known";
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(":memory:");
    REQUIRE(db.open());

    {
        Queries queries(db);
        queries.migrate();

        const QString path = "C:/replays/known.KWReplay";
        const QDateTime mtime = QDateTime::fromMSecsSinceEpoch(
            1700000000123, QTimeZone(QTimeZone::UTC));
        constexpr qsizetype size = 123456;
        insertExternalReplayFile(db, "checksum-known", path, mtime, size);

        CHECK(queries.isReplayKnown(path, mtime, size));
        CHECK_FALSE(
            queries.isReplayKnown("C:/replays/other.KWReplay", mtime, size));
        CHECK_FALSE(queries.isReplayKnown(path, mtime.addMSecs(1), size));
        CHECK_FALSE(queries.isReplayKnown(path, mtime, size + 1));

        // The database stores an epoch value, so the same instant expressed
        // in another time zone must still match.
        CHECK(queries.isReplayKnown(path, mtime.toLocalTime(), size));
    }

    db.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}

TEST_CASE("Queries selectKnownExternalReplayFiles returns stored metadata",
          "[queries][sql][external-path]") {
    const QString connectionName = "queries_select_known_external_files";
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(":memory:");
    REQUIRE(db.open());

    {
        Queries queries(db);
        queries.migrate();

        CHECK(queries.selectKnownExternalReplayFiles().isEmpty());

        const QDateTime firstMtime = QDateTime::fromMSecsSinceEpoch(
            1700000000123, QTimeZone(QTimeZone::UTC));
        const QDateTime secondMtime = QDateTime::fromMSecsSinceEpoch(
            1800000000456, QTimeZone(QTimeZone::UTC));
        insertExternalReplayFile(db, "checksum-a", "C:/replays/a.KWReplay",
                                 firstMtime, 1024);
        insertExternalReplayFile(db, "checksum-b", "C:/replays/b.KWReplay",
                                 secondMtime, 2048);

        const QList<ExternalReplayFile> files =
            queries.selectKnownExternalReplayFiles();
        REQUIRE(files.size() == 2);

        const auto first = std::find_if(
            files.cbegin(), files.cend(), [](const ExternalReplayFile& file) {
                return file.externalPath == "C:/replays/a.KWReplay";
            });
        REQUIRE(static_cast<bool>(first != files.cend()));
        CHECK(first->replayChecksum == "checksum-a");
        CHECK(first->mtime.toMSecsSinceEpoch() ==
              firstMtime.toMSecsSinceEpoch());
        CHECK(first->fileSize == 1024);

        const auto second = std::find_if(
            files.cbegin(), files.cend(), [](const ExternalReplayFile& file) {
                return file.externalPath == "C:/replays/b.KWReplay";
            });
        REQUIRE(static_cast<bool>(second != files.cend()));
        CHECK(second->replayChecksum == "checksum-b");
        CHECK(second->mtime.toMSecsSinceEpoch() ==
              secondMtime.toMSecsSinceEpoch());
        CHECK(second->fileSize == 2048);
    }

    db.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}

TEST_CASE("Queries refreshes metadata without changing the replay checksum",
          "[queries][sql][external-path]") {
    const QString connectionName = "queries_refresh_fingerprint";
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(":memory:");
    REQUIRE(db.open());
    {
        Queries queries(db);
        queries.migrate();
        const QString path = "C:/replays/known.KWReplay";
        const QByteArray checksum = "same-content";
        const auto mtime = QDateTime::fromMSecsSinceEpoch(1700000000123);
        REQUIRE(queries.insertExternalFilename(checksum, path, mtime, 123));
        CHECK_FALSE(queries.insertExternalFilename(checksum, path, mtime, 123));

        const auto touched = mtime.addMSecs(1);
        CHECK(queries.insertExternalFilename(checksum, path, touched, 123));
        CHECK_FALSE(queries.isReplayKnown(path, mtime, 123));
        CHECK(queries.isReplayKnown(path, touched, 123));
        CHECK(queries.insertExternalFilename(checksum, path, touched, 124));
        CHECK_FALSE(queries.isReplayKnown(path, touched, 123));
        CHECK(queries.isReplayKnown(path, touched, 124));
        CHECK_FALSE(
            queries.insertExternalFilename(checksum, path, touched, 124));

        const auto files = queries.selectKnownExternalReplayFiles();
        REQUIRE(files.size() == 1);
        CHECK(files.front().replayChecksum == checksum);
        CHECK(files.front().mtime == touched);
        CHECK(files.front().fileSize == 124);
    }
    db.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}
