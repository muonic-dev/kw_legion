// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include <kwlegion_core/transaction.h>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <catch2/catch_test_macros.hpp>

using namespace KWLegionCore;

namespace {

int countRows(QSqlQuery& query) {
    REQUIRE(query.exec("SELECT COUNT(*) FROM t"));
    REQUIRE(query.next());
    return query.value(0).toInt();
}

}  // namespace

TEST_CASE("SqlTransactionGuard commits on success") {
    QSqlDatabase db =
        QSqlDatabase::addDatabase("QSQLITE", "transaction_commit");
    db.setDatabaseName(":memory:");
    REQUIRE(db.open());

    QSqlQuery query(db);
    REQUIRE(query.exec("CREATE TABLE t (id INTEGER)"));

    {
        SqlTransactionGuard tx(db);
        REQUIRE(query.exec("INSERT INTO t (id) VALUES (1)"));
        REQUIRE(tx.commit());
    }

    CHECK(countRows(query) == 1);

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("transaction_commit");
}

TEST_CASE("SqlTransactionGuard rolls back if never committed") {
    QSqlDatabase db =
        QSqlDatabase::addDatabase("QSQLITE", "transaction_rollback");
    db.setDatabaseName(":memory:");
    REQUIRE(db.open());

    QSqlQuery query(db);
    REQUIRE(query.exec("CREATE TABLE t (id INTEGER)"));

    {
        SqlTransactionGuard tx(db);
        REQUIRE(query.exec("INSERT INTO t (id) VALUES (1)"));
        // tx goes out of scope without commit() - should roll back.
    }

    CHECK(countRows(query) == 0);

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("transaction_rollback");
}

TEST_CASE("SqlTransactionGuard commit fails leaves the guard rollback-ready") {
    QSqlDatabase db =
        QSqlDatabase::addDatabase("QSQLITE", "transaction_double_commit");
    db.setDatabaseName(":memory:");
    REQUIRE(db.open());

    QSqlQuery query(db);
    REQUIRE(query.exec("CREATE TABLE t (id INTEGER)"));

    SqlTransactionGuard tx(db);
    REQUIRE(query.exec("INSERT INTO t (id) VALUES (1)"));
    REQUIRE(tx.commit());
    // A second commit() with no open transaction should just report failure,
    // not crash or attempt to commit again.
    CHECK_FALSE(tx.commit());

    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("transaction_double_commit");
}
