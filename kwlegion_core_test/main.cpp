// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

// QSqlDatabase's plugin loader needs a QCoreApplication instance to resolve
// the SQLite driver's search path, so we can't use Catch2::Catch2WithMain
// here - it doesn't construct one.
#include <QCoreApplication>

#define CATCH_CONFIG_RUNNER
#include <catch2/catch_session.hpp>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    return Catch::Session().run(argc, argv);
}
