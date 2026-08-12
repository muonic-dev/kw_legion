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

    bool commit();

   private:
    QSqlDatabase m_db;
    bool m_inTx;
};

}  // namespace KWLegionCore