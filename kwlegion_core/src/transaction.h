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

    // Only commit is no-discard to force checks on whether the commit actually
    // succeeded
    [[nodiscard]] bool commit();
    bool rollback();

   private:
    QSqlDatabase m_db;
    bool m_inTx;
};

}  // namespace KWLegionCore