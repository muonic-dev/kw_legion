// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>
#include <catch2/catch_test_macros.hpp>

#include "deferred.h"

using namespace KWLegionCore;

namespace {

// How long to wait for a Deferred::pathReady signal before giving up, once a
// path is expected to actually fire.
constexpr int SIGNAL_TIMEOUT_MS = 1000;

// Short enough to keep tests fast, long enough to be well clear of scheduling
// jitter on a loaded CI box.
constexpr int FAST_RECHECK_MS = 20;

// A window to wait-and-confirm nothing arrived, for the not-ready case. Must
// stay well under WAIT_FOR_SETTLED_MS (15s in deferred.cpp) so it can't
// accidentally pass because the file genuinely aged past the real threshold.
constexpr int CONFIRM_NO_SIGNAL_MS = 150;

// Comfortably past deferred.cpp's internal WAIT_FOR_SETTLED_MS threshold, so
// tests don't need to track that constant directly.
constexpr qint64 WELL_SETTLED_MS = 5 * 60 * 1000;

QString createFile(const QDir& root, const QString& relativePath) {
    const QString absolutePath = root.filePath(relativePath);
    QFile file(absolutePath);
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write("replay");
    file.close();
    return absolutePath;
}

// Backdates a file's modification time by msAgo, so readiness tests don't
// need to actually wait in real time for a file to "age".
void backdate(const QString& path, qint64 msAgo) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadWrite));
    const QDateTime oldTime = QDateTime::currentDateTimeUtc().addMSecs(-msAgo);
    REQUIRE(file.setFileTime(oldTime, QFileDevice::FileModificationTime));
    file.close();
}

}  // namespace

TEST_CASE("Deferred::readyForParsing is false for a nonexistent path") {
    const QString missingPath = "Z:/definitely/not/a/real/path.KWReplay";
    CHECK_FALSE(Deferred::readyForParsing(missingPath));
}

TEST_CASE("Deferred::readyForParsing is false for an empty file") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    const QString path = root.filePath("empty.KWReplay");
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.close();
    backdate(path, WELL_SETTLED_MS);

    CHECK_FALSE(Deferred::readyForParsing(path));
}

TEST_CASE(
    "Deferred::readyForParsing is false for a non-empty, recently modified "
    "file") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    const QString path = createFile(root, "fresh.KWReplay");

    CHECK_FALSE(Deferred::readyForParsing(path));
}

TEST_CASE(
    "Deferred::readyForParsing is true for a non-empty, well-settled file") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    const QString path = createFile(root, "settled.KWReplay");
    backdate(path, WELL_SETTLED_MS);

    CHECK(Deferred::readyForParsing(path));
}

TEST_CASE("Deferred emits pathReady for a path that is already settled") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    const QString path = createFile(root, "settled.KWReplay");
    backdate(path, WELL_SETTLED_MS);

    Deferred deferred;
    deferred.setRecheckIntervalMs(FAST_RECHECK_MS);

    QSignalSpy spy(&deferred, &Deferred::pathReady);
    deferred.waitForReady(path);

    REQUIRE(spy.wait(SIGNAL_TIMEOUT_MS));
    REQUIRE(spy.size() == 1);
    CHECK(spy.at(0).at(0).toString() == path);
}

TEST_CASE(
    "Deferred does not emit pathReady while a path is still too fresh") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    const QString path = createFile(root, "fresh.KWReplay");

    Deferred deferred;
    deferred.setRecheckIntervalMs(FAST_RECHECK_MS);

    QSignalSpy spy(&deferred, &Deferred::pathReady);
    deferred.waitForReady(path);

    CHECK_FALSE(spy.wait(CONFIRM_NO_SIGNAL_MS));
    CHECK(spy.isEmpty());
}

TEST_CASE(
    "Deferred emits pathReady once a previously-fresh path settles on a "
    "later recheck") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    const QString path = createFile(root, "settling.KWReplay");

    Deferred deferred;
    deferred.setRecheckIntervalMs(FAST_RECHECK_MS);

    QSignalSpy spy(&deferred, &Deferred::pathReady);
    deferred.waitForReady(path);

    CHECK_FALSE(spy.wait(CONFIRM_NO_SIGNAL_MS));

    // Simulate time passing without an actual wait - back-date the file out
    // from under the still-pending Deferred instance.
    backdate(path, WELL_SETTLED_MS);

    REQUIRE(spy.wait(SIGNAL_TIMEOUT_MS));
    REQUIRE(spy.size() == 1);
    CHECK(spy.at(0).at(0).toString() == path);
}

TEST_CASE("Deferred tracks multiple paths and only emits the ready one") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    const QString readyPath = createFile(root, "settled.KWReplay");
    backdate(readyPath, WELL_SETTLED_MS);
    const QString freshPath = createFile(root, "fresh.KWReplay");

    Deferred deferred;
    deferred.setRecheckIntervalMs(FAST_RECHECK_MS);

    QSignalSpy spy(&deferred, &Deferred::pathReady);
    deferred.waitForReady(readyPath);
    deferred.waitForReady(freshPath);

    REQUIRE(spy.wait(SIGNAL_TIMEOUT_MS));
    // Give any (incorrect) second emission a chance to arrive before
    // asserting there's exactly one.
    QTest::qWait(CONFIRM_NO_SIGNAL_MS);

    REQUIRE(spy.size() == 1);
    CHECK(spy.at(0).at(0).toString() == readyPath);
}

TEST_CASE("Deferred::stop halts pending rechecks") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    const QString path = createFile(root, "fresh.KWReplay");

    Deferred deferred;
    deferred.setRecheckIntervalMs(FAST_RECHECK_MS);

    QSignalSpy spy(&deferred, &Deferred::pathReady);
    deferred.waitForReady(path);
    deferred.stop();

    // Settle the file after stopping - if the timer were still running this
    // would otherwise fire on the next recheck.
    backdate(path, WELL_SETTLED_MS);

    CHECK_FALSE(spy.wait(CONFIRM_NO_SIGNAL_MS));
    CHECK(spy.isEmpty());
}
