// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#pragma once

#include <QSqlDatabase>

namespace KWLegionCore {
class SqlTransactionGuard {
   public:
    explicit SqlTransactionGuard(QSqlDatabase);

    virtual ~SqlTransactionGuard();

    SqlTransactionGuard(const SqlTransactionGuard&) = delete;
    SqlTransactionGuard(SqlTransactionGuard&&) = delete;

    SqlTransactionGuard& operator=(const SqlTransactionGuard&) = delete;
    SqlTransactionGuard& operator=(SqlTransactionGuard&&) = delete;

    // Throws StorageException if there is no active transaction to commit,
    // or if the underlying commit fails.
    void commit();
    bool rollback();

   private:
    QSqlDatabase m_db;
    bool m_inTx;
};

}  // namespace KWLegionCore