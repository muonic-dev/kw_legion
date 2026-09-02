// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include "transaction.h"

#include <QSqlError>

#include "exception.h"

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

void SqlTransactionGuard::commit() {
    if (!m_inTx) {
        throw StorageException("commit failed: no active transaction");
    }
    // When commit is succesful we aren't in a transaction anymore
    m_inTx = !m_db.commit();
    if (m_inTx) {
        throw StorageException("commit failed: " + m_db.lastError().text());
    }
}
}  // namespace KWLegionCore