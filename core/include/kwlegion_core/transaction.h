#pragma once

#include <kwlegion_core/kwlegion_core_export.h>

#include <QSqlDatabase>

namespace KWLegionCore {
class KWLEGION_CORE_EXPORT SqlTransactionGuard {
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