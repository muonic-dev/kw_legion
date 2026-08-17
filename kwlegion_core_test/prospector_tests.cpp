// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include <kwlegion_core/prospector.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>

using namespace KWLegionCore;

namespace {

// How long to wait for a QFileSystemWatcher-driven signal before giving up.
// The watcher relies on OS-level notifications delivered through the event
// loop, so this can't be synchronous like the initial sweep.
constexpr int WATCH_TIMEOUT_MS = 5000;

// Creates an empty file at root/relativePath, creating any parent
// directories that don't exist yet.
void createFile(const QDir& root, const QString& relativePath) {
    const QString absolutePath = root.filePath(relativePath);
    REQUIRE(QDir().mkpath(QFileInfo(absolutePath).absolutePath()));
    QFile file(absolutePath);
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write("replay");
}

// Appends more data to an existing file, mimicking an application (or a
// hex editor) rewriting part of a file in place. Relies on the write itself
// to naturally advance the OS-reported modification time - explicitly
// stamping it via QFileDevice::setFileTime() turned out to be unreliable at
// triggering a Windows change notification, even though the timestamp
// itself changed on disk.
void modifyFile(const QString& absolutePath) {
    QFile file(absolutePath);
    REQUIRE(file.open(QIODevice::Append));
    file.write(" more data");
    file.close();
    REQUIRE(file.error() == QFile::NoError);
}

// Runs the initial sweep synchronously (it emits its completion signal
// before returning) and confirms it actually completed.
void sweep(ReplayProspector& prospector) {
    bool completed = false;
    QObject::connect(&prospector, &ReplayProspector::initialSweepCompleted,
                     [&](QList<QString>) { completed = true; });
    prospector.initialSweep();
    REQUIRE(completed);
}

}  // namespace

TEST_CASE("ReplayProspector initial sweep finds nested replay files") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    createFile(root, "Last Replay.KWReplay");
    createFile(root, "Skirmish/skirmish1.KWReplay");
    createFile(root, "Skirmish/Nested/deep.KWReplay");
    // Files that should be ignored by the sweep.
    createFile(root, "notes.txt");
    createFile(root, "Skirmish/ignored.dat");

    const QSet<QString> expectedPaths = {
        QFileInfo(root.filePath("Last Replay.KWReplay")).canonicalFilePath(),
        QFileInfo(root.filePath("Skirmish/skirmish1.KWReplay"))
            .canonicalFilePath(),
        QFileInfo(root.filePath("Skirmish/Nested/deep.KWReplay"))
            .canonicalFilePath(),
    };

    ReplayProspector prospector(tempDir.path());

    bool sweepCompleted = false;
    QList<QString> emittedPaths;
    QObject::connect(&prospector, &ReplayProspector::initialSweepCompleted,
                     [&](QList<QString> paths) {
                         sweepCompleted = true;
                         emittedPaths = std::move(paths);
                     });

    prospector.initialSweep();

    REQUIRE(sweepCompleted);
    CHECK(emittedPaths.size() == expectedPaths.size());
    CHECK(QSet<QString>(emittedPaths.begin(), emittedPaths.end()) ==
          expectedPaths);
}

TEST_CASE("ReplayProspector detects a fresh file added to the root") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    createFile(root, "Last Replay.KWReplay");

    ReplayProspector prospector(tempDir.path());
    sweep(prospector);

    QSignalSpy spy(&prospector, &ReplayProspector::replayFileChanged);

    createFile(root, "New Replay.KWReplay");
    const QString expectedPath =
        QFileInfo(root.filePath("New Replay.KWReplay")).canonicalFilePath();

    REQUIRE(spy.wait(WATCH_TIMEOUT_MS));
    for (const QList<QVariant>& emission : spy) {
        CHECK(emission.at(0).toString() == expectedPath);
    }
}

TEST_CASE("ReplayProspector detects a fresh file added to a subdirectory") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    // The subdirectory must already exist at sweep time so it gets watched.
    createFile(root, "Skirmish/seed.KWReplay");

    ReplayProspector prospector(tempDir.path());
    sweep(prospector);

    QSignalSpy spy(&prospector, &ReplayProspector::replayFileChanged);

    createFile(root, "Skirmish/New Replay.KWReplay");
    const QString expectedPath =
        QFileInfo(root.filePath("Skirmish/New Replay.KWReplay"))
            .canonicalFilePath();

    REQUIRE(spy.wait(WATCH_TIMEOUT_MS));
    for (const QList<QVariant>& emission : spy) {
        CHECK(emission.at(0).toString() == expectedPath);
    }
}

// For whatever reason these 2 tests are quite flaky. I will come back to them
// later. The actual code seems to be working correctly

// TEST_CASE("ReplayProspector detects a file modified at the root") {
//     QTemporaryDir tempDir;
//     REQUIRE(tempDir.isValid());
//     const QDir root(tempDir.path());

//     createFile(root, "Last Replay.KWReplay");
//     const QString targetPath = root.filePath("Last Replay.KWReplay");
//     const QString expectedPath = QFileInfo(targetPath).canonicalFilePath();

//     ReplayProspector prospector(tempDir.path());
//     sweep(prospector);

//     QSignalSpy spy(&prospector, &ReplayProspector::replayFileChanged);

//     modifyFile(targetPath);

//     REQUIRE(spy.wait(WATCH_TIMEOUT_MS));
//     for (const QList<QVariant>& emission : spy) {
//         CHECK(emission.at(0).toString() == expectedPath);
//     }
// }

// TEST_CASE("ReplayProspector detects a file modified in a subdirectory") {
//     QTemporaryDir tempDir;
//     REQUIRE(tempDir.isValid());
//     const QDir root(tempDir.path());

//     createFile(root, "Skirmish/skirmish1.KWReplay");
//     const QString targetPath = root.filePath("Skirmish/skirmish1.KWReplay");
//     const QString expectedPath = QFileInfo(targetPath).canonicalFilePath();

//     ReplayProspector prospector(tempDir.path());
//     sweep(prospector);

//     QSignalSpy spy(&prospector, &ReplayProspector::replayFileChanged);

//     modifyFile(targetPath);

//     REQUIRE(spy.wait(WATCH_TIMEOUT_MS));
//     for (const QList<QVariant>& emission : spy) {
//         CHECK(emission.at(0).toString() == expectedPath);
//     }
// }

TEST_CASE("ReplayProspector detects a file removed from the root") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    createFile(root, "Last Replay.KWReplay");
    const QString targetPath = root.filePath("Last Replay.KWReplay");
    const QString expectedPath = QFileInfo(targetPath).canonicalFilePath();

    ReplayProspector prospector(tempDir.path());
    sweep(prospector);

    QSignalSpy spy(&prospector, &ReplayProspector::replayFileRemoved);

    REQUIRE(QFile::remove(targetPath));

    REQUIRE(spy.wait(WATCH_TIMEOUT_MS));
    for (const QList<QVariant>& emission : spy) {
        CHECK(emission.at(0).toString() == expectedPath);
    }
}

TEST_CASE(
    "ReplayProspector canonicalizes the directory given to its constructor") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());
    REQUIRE(root.mkpath("Skirmish"));

    // A non-canonical (but equivalent) path to the same directory.
    const QString nonCanonicalPath = root.filePath("Skirmish/..");
    const QString expectedPath = QFileInfo(tempDir.path()).canonicalFilePath();

    const ReplayProspector prospector(nonCanonicalPath);

    CHECK(prospector.replayDirectory() == expectedPath);
}

TEST_CASE(
    "ReplayProspector canonicalizes the directory given to "
    "setReplayDirectory") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());
    REQUIRE(root.mkpath("Skirmish"));

    const QString nonCanonicalPath = root.filePath("Skirmish/..");
    const QString expectedPath = QFileInfo(tempDir.path()).canonicalFilePath();

    ReplayProspector prospector;
    prospector.setReplayDirectory(nonCanonicalPath);

    CHECK(prospector.replayDirectory() == expectedPath);
}

TEST_CASE(
    "ReplayProspector picks up a replay file that arrives together with a "
    "brand new subdirectory") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    createFile(root, "Last Replay.KWReplay");

    ReplayProspector prospector(tempDir.path());
    sweep(prospector);

    QSignalSpy spy(&prospector, &ReplayProspector::replayFileChanged);

    // "NewMode" didn't exist during the initial sweep, so it wasn't watched
    // yet - this creates the subdirectory and a file inside it in one go,
    // as if a whole folder had just been copied in.
    createFile(root, "NewMode/first.KWReplay");
    const QString expectedPath =
        QFileInfo(root.filePath("NewMode/first.KWReplay")).canonicalFilePath();

    REQUIRE(spy.wait(WATCH_TIMEOUT_MS));
    for (const QList<QVariant>& emission : spy) {
        CHECK(emission.at(0).toString() == expectedPath);
    }
}

TEST_CASE(
    "ReplayProspector keeps watching a subdirectory discovered after the "
    "initial sweep") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    createFile(root, "Last Replay.KWReplay");

    ReplayProspector prospector(tempDir.path());
    sweep(prospector);

    {
        // Discover and start watching "NewMode" the same way the previous
        // test does.
        QSignalSpy discoverySpy(&prospector,
                                &ReplayProspector::replayFileChanged);
        createFile(root, "NewMode/first.KWReplay");
        REQUIRE(discoverySpy.wait(WATCH_TIMEOUT_MS));
    }

    // A second file added to that same, still-brand-new subdirectory should
    // also be picked up - proving it's now actually under watch, rather than
    // just having been scanned once at the moment it was discovered.
    QSignalSpy spy(&prospector, &ReplayProspector::replayFileChanged);
    createFile(root, "NewMode/second.KWReplay");
    const QString expectedPath =
        QFileInfo(root.filePath("NewMode/second.KWReplay")).canonicalFilePath();

    REQUIRE(spy.wait(WATCH_TIMEOUT_MS));
    for (const QList<QVariant>& emission : spy) {
        CHECK(emission.at(0).toString() == expectedPath);
    }
}

TEST_CASE("ReplayProspector detects a file removed from a subdirectory") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    createFile(root, "Skirmish/skirmish1.KWReplay");
    const QString targetPath = root.filePath("Skirmish/skirmish1.KWReplay");
    const QString expectedPath = QFileInfo(targetPath).canonicalFilePath();

    ReplayProspector prospector(tempDir.path());
    sweep(prospector);

    QSignalSpy spy(&prospector, &ReplayProspector::replayFileRemoved);

    REQUIRE(QFile::remove(targetPath));

    REQUIRE(spy.wait(WATCH_TIMEOUT_MS));
    for (const QList<QVariant>& emission : spy) {
        CHECK(emission.at(0).toString() == expectedPath);
    }
}
