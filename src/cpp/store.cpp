#include "store.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <array>
#include <string>

#include "transaction.h"

Q_LOGGING_CATEGORY(logStore, "kwlegion.store");

namespace KWLegion {

constexpr std::array MIGRATIONS{
    R"(CREATE TABLE replays (
        id INTEGER PRIMARY KEY,
        match_title TEXT NOT NULL,
        match_description TEXT NOT NULL,
        map_name TEXT NOT NULL,
        map_id TEXT NOT NULL,
        game_type INT NOT NULL,
        timestamp INT NOT NULL,
        has_commentary INT NOT NULL,
        filename TEXT NOT NULL,
        map_reference TEXT NOT NULL,
        checksum BLOB NOT NULL UNIQUE,

        version_major INT,
        version_minor INT,
        build_major INT,
        build_minor INT
    ) STRICT;
    )",

    R"(CREATE INDEX replays_id ON replays(id);)",

    R"(CREATE TABLE replay_players(
        id INT NOT NULL,
        player_id INT NOT NULL,
        player_name TEXT NOT NULL,
        team_number INT,
        faction INT NOT NULL,
        is_computer INT NOT NULL,
        is_replay_saver INT NOT NULL,

        PRIMARY KEY(id, player_id)
    ) STRICT;
    )",
    R"(CREATE INDEX replay_players_id ON replay_players(id);)",
};

ReplayStore::ReplayStore(QObject* parent)
    : QObject(parent),
      m_dbPath(QStandardPaths::writableLocation(QStandardPaths::StateLocation) +
               "/replays.db"),
      m_replayDir(
          QStandardPaths::writableLocation(QStandardPaths::StateLocation) +
          "/replays"),
      m_scratchDir(
          QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
          "/replays") {}

void ReplayStore::startup() {
    ensureDirectories();
    ensureDb();
}

void ReplayStore::analyzeReplay(const QString& path) {
    qDebug(logStore) << "Analyzing replay: " << path;
    // First thing we do is copy into the staging directory before we run it
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

    if (!QDir().mkpath(m_scratchDir)) {
        if (QDir().exists(m_scratchDir)) {
            qDebug(logStore)
                << "Scratch directory already exists: " << m_scratchDir;
        } else {
            qCritical(logStore)
                << "Unable to create scratch directory: " << m_scratchDir;
        }
    }
}
}  // namespace KWLegion