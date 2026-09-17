/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <QLoggingCategory>
#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <memory>

namespace KWLegionCore {

Q_DECLARE_LOGGING_CATEGORY(logPersistence);

class SqlTransactionGuard;
class Queries;

// Owns the SQLite connection and the associated Queries instance.
class Persistence final : public QObject {
    Q_OBJECT

   public:
    explicit Persistence(QString dbPath, QString connectionName,
                         QObject* parent = nullptr);

    ~Persistence() override;

    Persistence(const Persistence&) = delete;
    Persistence(Persistence&&) = delete;

    Persistence& operator=(const Persistence&) = delete;
    Persistence& operator=(Persistence&&) = delete;

    // Opens the database connection and runs any pending migrations. Wire
    // this to the owning thread's QThread::started (connected ahead of
    // anything that depends on the schema existing, since started's direct
    // connections run synchronously in connection order).
    void init();

    // Releases the query and closes the underlying connection. Callers that
    // need to remove the connection (e.g. QSqlDatabase::removeDatabase() in
    // tests) must call this first - otherwise this object's own handle keeps
    // it open and removeDatabase() just warns instead of doing anything.
    void close();

    // Begin a transaction against this connection.
    [[nodiscard]] SqlTransactionGuard beginTransact();

   private:
    friend class Queries;

    QString m_dbPath;
    QString m_connectionName;
    QSqlDatabase m_db;
    std::unique_ptr<Queries> m_queries;
};

}  // namespace KWLegionCore
