// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include <kwlegion_core/persistence.h>

#include <QSqlError>
#include <utility>

#include "exception.h"
#include "queries.h"
#include "transaction.h"

namespace KWLegionCore {

Q_LOGGING_CATEGORY(logPersistence, "kwlegion.persistence");

Persistence::Persistence(QString dbPath, QString connectionName,
                         QObject* parent)
    : QObject(parent),
      m_dbPath(std::move(dbPath)),
      m_connectionName(std::move(connectionName)) {}

Persistence::~Persistence() = default;

void Persistence::init() {
    if (m_db.isOpen()) {
        return;
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(m_dbPath);
    m_db.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000"));
    if (!m_db.open()) {
        qCCritical(logPersistence)
            << "Failed to open database: " << m_db.lastError().text();
        return;
    }
    m_queries = std::make_unique<Queries>(m_db);

    try {
        // Outside a transaction so that we do as much as we can - if we ever
        // ship a broken migration this means there is less to do.
        Queries::get(*this).migrate();
    } catch (const StorageException& ex) {
        qCritical(logPersistence)
            << "Failed to migrate database: " << ex.what();
    }
}

SqlTransactionGuard Persistence::beginTransact() {
    return SqlTransactionGuard(m_db);
}

void Persistence::close() {
    m_queries.reset();
    m_db.close();
    m_db = QSqlDatabase();
}

}  // namespace KWLegionCore
