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

// How long to wait for a Deferred::pathChanged signal before giving up, once
// a path is expected to actually fire.
constexpr int SIGNAL_TIMEOUT_MS = 1000;

// Short enough to keep tests fast, long enough to be well clear of scheduling
// jitter on a loaded CI box.
constexpr int FAST_RECHECK_MS = 20;

// A window to wait-and-confirm nothing arrived, for the unchanged case.
constexpr int CONFIRM_NO_SIGNAL_MS = 150;

// Long enough that the long stop can't fire during a test that isn't
// deliberately exercising it.
constexpr qint64 LONG_STOP_NEVER_MS = 5 * 60 * 1000;

QString writeFile(const QDir& root, const QString& relativePath,
                  const QByteArray& contents) {
    const QString absolutePath = root.filePath(relativePath);
    QFile file(absolutePath);
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write(contents);
    file.close();
    return absolutePath;
}

// Appends to a file, which both grows it and touches its modification time -
// i.e. what the game does to a replay while a match is in progress.
void append(const QString& path, const QByteArray& contents) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::Append));
    file.write(contents);
    file.close();
}

// Moves a file's modification time without changing its size, standing in for
// the writer closing its handle and flushing the timestamp.
void touch(const QString& path) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadWrite));
    const QDateTime newTime = QDateTime::currentDateTimeUtc().addSecs(-60);
    REQUIRE(file.setFileTime(newTime, QFileDevice::FileModificationTime));
    file.close();
}

// Deferred is a QObject, so it can't be handed back from a factory - configure
// one in place instead.
void configure(Deferred& deferred) {
    deferred.setRecheckIntervalMs(FAST_RECHECK_MS);
    deferred.setLongStopMs(LONG_STOP_NEVER_MS);
}

}  // namespace

TEST_CASE("Deferred::sample reports zero size for a nonexistent path") {
    const Watermark sample =
        Deferred::sample("Z:/definitely/not/a/real/path.KWReplay");
    CHECK(sample.size == 0);
    CHECK_FALSE(sample.modifiedAt.isValid());
}

TEST_CASE("Deferred::sample reports the size of an existing file") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    const QString path = writeFile(root, "replay.KWReplay", "replay");

    const Watermark sample = Deferred::sample(path);
    CHECK(sample.size == 6);
    CHECK(sample.modifiedAt.isValid());
}

TEST_CASE("Watermark::differsFrom is false for an unchanged file") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    const QString path = writeFile(root, "replay.KWReplay", "replay");

    const Watermark first = Deferred::sample(path);
    const Watermark second = Deferred::sample(path);

    CHECK_FALSE(second.differsFrom(first));
}

TEST_CASE("Watermark::differsFrom is true once a file grows") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    const QString path = writeFile(root, "replay.KWReplay", "replay");
    const Watermark before = Deferred::sample(path);

    append(path, "more");

    CHECK(Deferred::sample(path).differsFrom(before));
}

TEST_CASE(
    "Watermark::differsFrom is true when only the modification time moves") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    const QString path = writeFile(root, "replay.KWReplay", "replay");
    const Watermark before = Deferred::sample(path);

    // The size deliberately stays put - this is the "writer finally closed
    // the handle" case that a size-only comparison would miss.
    touch(path);

    const Watermark after = Deferred::sample(path);
    CHECK(after.size == before.size);
    CHECK(after.differsFrom(before));
}

TEST_CASE("Deferred does not emit pathChanged while a file is unchanged") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    const QString path = writeFile(root, "replay.KWReplay", "replay");

    Deferred deferred;
    configure(deferred);
    QSignalSpy spy(&deferred, &Deferred::pathChanged);
    deferred.waitForChange(path, Deferred::sample(path));

    CHECK_FALSE(spy.wait(CONFIRM_NO_SIGNAL_MS));
    CHECK(spy.isEmpty());
}

TEST_CASE("Deferred emits pathChanged once a file grows") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    const QString path = writeFile(root, "replay.KWReplay", "replay");

    Deferred deferred;
    configure(deferred);
    QSignalSpy spy(&deferred, &Deferred::pathChanged);
    deferred.waitForChange(path, Deferred::sample(path));

    CHECK_FALSE(spy.wait(CONFIRM_NO_SIGNAL_MS));

    append(path, "more data");

    REQUIRE(spy.wait(SIGNAL_TIMEOUT_MS));
    REQUIRE(spy.size() == 1);
    CHECK(spy.at(0).at(0).toString() == path);
}

TEST_CASE("Deferred emits pathChanged for a file that appears later") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    const QString path = root.filePath("later.KWReplay");

    Deferred deferred;
    configure(deferred);
    QSignalSpy spy(&deferred, &Deferred::pathChanged);
    // Deferred against a path that doesn't exist yet - the zero-size, empty
    // watermark still gives us something to compare against.
    deferred.waitForChange(path, Deferred::sample(path));

    CHECK_FALSE(spy.wait(CONFIRM_NO_SIGNAL_MS));

    writeFile(root, "later.KWReplay", "replay");

    REQUIRE(spy.wait(SIGNAL_TIMEOUT_MS));
    CHECK(spy.at(0).at(0).toString() == path);
}

TEST_CASE("Deferred emits pathChanged on the long stop with no file change") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    const QString path = writeFile(root, "stalled.KWReplay", "replay");

    Deferred deferred;
    deferred.setRecheckIntervalMs(FAST_RECHECK_MS);
    // Already elapsed by the time the first recheck runs, so a completely
    // static file still gets one more attempt.
    deferred.setLongStopMs(0);

    QSignalSpy spy(&deferred, &Deferred::pathChanged);
    deferred.waitForChange(path, Deferred::sample(path));

    REQUIRE(spy.wait(SIGNAL_TIMEOUT_MS));
    CHECK(spy.at(0).at(0).toString() == path);
}

TEST_CASE("Deferred tracks multiple paths and only emits the changed one") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    const QString changedPath = writeFile(root, "changed.KWReplay", "replay");
    const QString staticPath = writeFile(root, "static.KWReplay", "replay");

    Deferred deferred;
    configure(deferred);
    QSignalSpy spy(&deferred, &Deferred::pathChanged);
    deferred.waitForChange(changedPath, Deferred::sample(changedPath));
    deferred.waitForChange(staticPath, Deferred::sample(staticPath));

    append(changedPath, "more data");

    REQUIRE(spy.wait(SIGNAL_TIMEOUT_MS));
    // Give any (incorrect) second emission a chance to arrive before
    // asserting there's exactly one.
    QTest::qWait(CONFIRM_NO_SIGNAL_MS);

    REQUIRE(spy.size() == 1);
    CHECK(spy.at(0).at(0).toString() == changedPath);
}

TEST_CASE("Deferred stops watching a removed path") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    const QString path = writeFile(root, "replay.KWReplay", "replay");

    Deferred deferred;
    configure(deferred);
    QSignalSpy spy(&deferred, &Deferred::pathChanged);
    deferred.waitForChange(path, Deferred::sample(path));
    deferred.removeWaitForChange(path);

    append(path, "more data");

    CHECK_FALSE(spy.wait(CONFIRM_NO_SIGNAL_MS));
    CHECK(spy.isEmpty());
}

TEST_CASE("Deferred::stop halts pending rechecks") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    const QString path = writeFile(root, "replay.KWReplay", "replay");

    Deferred deferred;
    configure(deferred);
    QSignalSpy spy(&deferred, &Deferred::pathChanged);
    deferred.waitForChange(path, Deferred::sample(path));
    deferred.stop();

    // Change the file after stopping - if the timer were still running this
    // would otherwise fire on the next recheck.
    append(path, "more data");

    CHECK_FALSE(spy.wait(CONFIRM_NO_SIGNAL_MS));
    CHECK(spy.isEmpty());
}
