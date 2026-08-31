// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include <kwlegion_core/transaction.h>

namespace KWLegionCore {
SqlTransactionGuard::SqlTransactionGuard(QSqlDatabase db)
    : m_db(db), m_inTx(db.transaction()) {}

SqlTransactionGuard::~SqlTransactionGuard() {
    if (m_inTx) {
        m_db.rollback();
    }
}

bool SqlTransactionGuard::rollback() {
    if (!m_inTx) {
        return false;
    }
    m_inTx = !m_db.rollback();
    return !m_inTx;
}

// TODO: For ergonomics should this throw when commit fails?
bool SqlTransactionGuard::commit() {
    if (!m_inTx) {
        return false;
    }
    // When commit is succesful we aren't in a transaction anymore
    m_inTx = !m_db.commit();
    return !m_inTx;
}
}  // namespace KWLegionCore