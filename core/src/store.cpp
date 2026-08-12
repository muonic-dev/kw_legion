// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include <kwlegion_core/store.h>
#include <kwlegion_core/transaction.h>
#include <legionparser/parser.h>

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <array>
#include <exception>
#include <stdexcept>
#include <string>

Q_LOGGING_CATEGORY(logStore, "kwlegion.store");

namespace KWLegionCore {

constexpr std::array MIGRATIONS{
    // Store the replay here
    // Note: we use the checksum to compute a stored local state path
    R"(CREATE TABLE replays (
        checksum BLOB PRIMARY KEY,
        match_title TEXT NOT NULL,
        match_description TEXT NOT NULL,
        map_name TEXT NOT NULL,
        map_id TEXT NOT NULL,
        game_type INT NOT NULL,
        timestamp INT NOT NULL,
        has_commentary INT NOT NULL,
        filename TEXT NOT NULL,
        map_reference TEXT NOT NULL,

        version_major INT,
        version_minor INT,
        build_major INT,
        build_minor INT

    ) STRICT, WITHOUT ROWID;
    )",

    // Replays might occur more than once in the replay folder for whatever
    // reason so a single storage is not good enough
    // external_path is a full canonical path
    R"(CREATE TABLE replay_external_paths(
        replay_checksum BLOB NOT NULL,
        external_path TEXT NOT NULL,

        PRIMARY KEY(replay_checksum, external_path)
    )",

    R"(CREATE TABLE replay_players(
        replay_checksum INT NOT NULL,
        player_id INT NOT NULL,
        player_name TEXT NOT NULL,
        team_number INT,
        faction INT NOT NULL,
        is_computer INT NOT NULL,
        is_replay_saver INT NOT NULL,

        PRIMARY KEY(replay_checksum, player_id)
    ) STRICT, WITHOUT ROWID;
    )",
};

namespace {

// Local class that we can throw when somethign fails to unwind
// This should never propogate outside this TU. The SqlTransactionGuard should
// correctly roll back.
class IngestionException : public std::runtime_error {
   public:
    IngestionException(const QString& what)
        : std::runtime_error(what.toStdString()) {}
};

// Helper utility class for dispatching queries
// Keep this near the migrations so we have close reference
class Queries final {
   public:
    explicit Queries(QSqlQuery&& query) : m_query(std::move(query)) {}

    ~Queries() = default;

    Queries(const Queries&) = delete;
    Queries(Queries&&) = delete;

    Queries& operator=(const Queries&) = delete;
    Queries& operator=(Queries&&) = delete;

    bool isReplayKnown(const QByteArray& checksum) {
        prepare("SELECT count(*) FROM replays WHERE checksum = :checksum");
        m_query.bindValue(":checksum", checksum);
        exec();

        return m_query.value(0).toInt() != 0;
    }

    void insertReplay(const LegionParser::ReplayMetadata& metadata) {
        prepare(R"(
            INSERT INTO replays(
            checksum,
            match_title,
            match_description,
            map_name,
            map_id,
            game_type,
            timestamp,
            has_commentary,
            filename,
            map_reference,
            version_major,
            version_minor,
            build_major,
            build_minor
            ) VALUES (
             :checksum, 
             :match_title, 
             :match_description, 
             :map_name, 
             :map_id,
             :game_type,
             :timestamp,
             :has_commentary,
             :filename,
             :map_reference,
             :version_major,
             :version_minor,
             :build_major,
             :build_minor
             );)");
        m_query.bindValue(":checksum", metadata.checksum);
        m_query.bindValue(":match_title", metadata.matchTitle);
        m_query.bindValue(":match_description", metadata.matchDescription);
        m_query.bindValue(":map_name", metadata.mapName);
        m_query.bindValue(":map_id", metadata.mapId);
        m_query.bindValue(":game_type",
                          LegionParser::toUint8(metadata.gameType));
        m_query.bindValue(":timestamp", metadata.timestamp.toSecsSinceEpoch());
        m_query.bindValue(":has_commentary", metadata.hasCommentary);
        m_query.bindValue(":filename", metadata.filename);
        m_query.bindValue(":map_reference", metadata.mapReference);
        m_query.bindValue(":version_major", metadata.versionMajor);
        m_query.bindValue(":version_minor", metadata.versionMinor);
        m_query.bindValue(":build_major", metadata.buildMajor);
        m_query.bindValue(":build_minor", metadata.buildMinor);
        exec();
    }

    void insertReplayPlayer(const QByteArray& checksum,
                            const QList<LegionParser::Player> players) {
        throw IngestionException("not implemented");
    }

   private:
    void prepare(const QString& sql) {
        if (!m_query.prepare(sql)) {
            throwLast();
        }
    }

    void exec() {
        if (!m_query.exec()) {
            throwLast();
        }
    }

    void throwLast() const {
        throw IngestionException(m_query.lastError().text());
    }

    QSqlQuery m_query;
};
}  // namespace

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
        qCritical(logStore) << "Unable to ingest the replay " << ex.what();
    }
}

void ReplayStore::ingestReplay(QFile& file,
                               const LegionParser::ReplayMetadata& metadata) {
    SqlTransactionGuard tx(m_db);
    Queries queries{QSqlQuery(m_db)};

    if (queries.isReplayKnown(metadata.checksum)) {
        // A replay with this checksum has been seen previously
        // insert a new 'on disk' record for this replay iff. the replay is
        // underneath the replay folder
    } else {
        // This is the first time the replay has been seen so we need to perform
        // to insert everything
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

    executeDdl();
}

void ReplayStore::executeDdl() {
    QSqlQuery query(m_db);
    query.exec("PRAGMA user_version");
    query.next();
    const size_t currentVersion = query.value(0).toULongLong();

    qDebug(logStore) << "Current schema version is: " << currentVersion;

    // The behavior of PRAGMA user_version starts at 0 so this is always the
    // next thing to execute
    for (size_t nextExec = currentVersion; nextExec < MIGRATIONS.size();
         nextExec++) {
        SqlTransactionGuard tx(m_db);
        if (!query.exec(MIGRATIONS.at(nextExec))) {
            qCritical(logStore) << "Failed to execute migration: " << nextExec
                                << ": " << query.lastError().text();
            return;
        }
        if (!query.exec(QStringLiteral("PRAGMA user_version = %1;")
                            .arg(nextExec + 1))) {
            qCritical(logStore) << "Failed to execute migration: " << nextExec
                                << ": " << query.lastError().text();
            return;
        }
        if (!tx.commit()) {
            qCritical(logStore) << "Failed to execute migration: " << nextExec
                                << ": " << query.lastError().text();
            return;
        }
    }

    qDebug(logStore) << "Migrations successful";
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