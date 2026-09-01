// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include <kwlegion_core/replaystore.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>

using namespace KWLegionCore;

namespace {

QString fixtureReplayPath() {
    return QStringLiteral(REPLAY_TEST_DATA_DIR "/test_gdis.KWReplay");
}

// Copies the fixture replay into root/relativePath, as if it had just
// appeared on disk there, and returns its canonical path.
QString copyFixtureReplay(const QDir& root, const QString& relativePath) {
    const QString destination = root.filePath(relativePath);
    REQUIRE(QDir().mkpath(QFileInfo(destination).absolutePath()));
    REQUIRE(QFile::copy(fixtureReplayPath(), destination));
    return QFileInfo(destination).canonicalFilePath();
}

}  // namespace

TEST_CASE(
    "ReplayStore creates its database and replay directory under the given "
    "state path") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    ReplayStore store(tempDir.path(), tempDir.path());
    QSignalSpy loadedSpy(&store, &ReplayStore::replaysLoaded);

    // Nothing is on disk yet
    store.receiveInitialReplayPaths({});

    REQUIRE(loadedSpy.count() == 1);
    CHECK(loadedSpy.at(0).at(0).value<QList<Replay>>().isEmpty());

    CHECK(QFile::exists(tempDir.filePath("replays.db")));
    CHECK(QDir(tempDir.filePath("replays")).exists());
}

TEST_CASE(
    "ReplayStore ingests a replay reported at startup and stores a "
    "canonical copy under the state path") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    const QString replayPath =
        copyFixtureReplay(root, "Source/replay.KWReplay");

    ReplayStore store(tempDir.path(), tempDir.path());
    QSignalSpy loadedSpy(&store, &ReplayStore::replaysLoaded);

    // replayPath truthfully exists on disk at this point, so it belongs in
    // the startup listing.
    store.receiveInitialReplayPaths({replayPath});

    REQUIRE(loadedSpy.count() == 1);
    const QList<Replay> replays = loadedSpy.at(0).at(0).value<QList<Replay>>();
    REQUIRE(replays.size() == 1);
    CHECK_FALSE(replays.at(0).checksum.isEmpty());
    CHECK(replays.at(0).hasExternalPath);

    const QString canonicalCopy = root.filePath(
        "replays/" + replays.at(0).checksum.toHex() + ".KWReplay");
    CHECK(QFile::exists(canonicalCopy));
    // The source file is copied into the store, not moved out of place.
    CHECK(QFile::exists(replayPath));
}

TEST_CASE(
    "ReplayStore reopened at the same state path sees the previously "
    "ingested replay") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    const QString replayPath =
        copyFixtureReplay(root, "Source/replay.KWReplay");

    QByteArray checksum;
    {
        ReplayStore store(tempDir.path(), tempDir.path());
        QSignalSpy loadedSpy(&store, &ReplayStore::replaysLoaded);
        store.receiveInitialReplayPaths({replayPath});

        REQUIRE(loadedSpy.count() == 1);
        const QList<Replay> replays =
            loadedSpy.at(0).at(0).value<QList<Replay>>();
        REQUIRE(replays.size() == 1);
        checksum = replays.at(0).checksum;
    }

    // A second store opened at the same state path, as if the app had
    // restarted with the replay file still present on disk - the same
    // "currently on disk" listing must be reported, since that's the source
    // of truth receiveInitialReplayPaths uses to prune anything gone.
    ReplayStore store(tempDir.path(), tempDir.path());
    QSignalSpy loadedSpy(&store, &ReplayStore::replaysLoaded);
    store.receiveInitialReplayPaths({replayPath});

    REQUIRE(loadedSpy.count() == 1);
    const QList<Replay> replays = loadedSpy.at(0).at(0).value<QList<Replay>>();
    REQUIRE(replays.size() == 1);
    CHECK(replays.at(0).checksum == checksum);
    CHECK(replays.at(0).hasExternalPath);
}

TEST_CASE("ReplayStore ingests a replay reported live via analyzeReplayFile") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    ReplayStore store(tempDir.path(), tempDir.path());
    // Nothing is on disk yet at startup - the replay "arrives" afterward.
    store.receiveInitialReplayPaths({});

    const QString replayPath =
        copyFixtureReplay(root, "Source/replay.KWReplay");

    QSignalSpy changedSpy(&store, &ReplayStore::replaysChanged);
    store.analyzeReplayFile(replayPath);

    REQUIRE(changedSpy.count() == 1);
    const QList<Replay> replays = changedSpy.at(0).at(0).value<QList<Replay>>();
    REQUIRE(replays.size() == 1);
    CHECK(replays.at(0).hasExternalPath);

    const QString canonicalCopy = root.filePath(
        "replays/" + replays.at(0).checksum.toHex() + ".KWReplay");
    CHECK(QFile::exists(canonicalCopy));
}

TEST_CASE("ReplayStore ignores a corrupt file that was never tracked") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    ReplayStore store(tempDir.path(), tempDir.path());
    store.receiveInitialReplayPaths({});

    const QString corruptPath = root.filePath("garbage.KWReplay");
    QFile corrupt(corruptPath);
    REQUIRE(corrupt.open(QIODevice::WriteOnly));
    corrupt.write("not a real replay file");
    corrupt.close();

    QSignalSpy changedSpy(&store, &ReplayStore::replaysChanged);
    store.analyzeReplayFile(corruptPath);

    CHECK(changedSpy.count() == 0);
}

TEST_CASE(
    "ReplayStore removeReplayFileLink clears the external path but keeps the "
    "replay known") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    ReplayStore store(tempDir.path(), tempDir.path());
    store.receiveInitialReplayPaths({});

    const QString replayPath =
        copyFixtureReplay(root, "Source/replay.KWReplay");
    store.analyzeReplayFile(replayPath);

    QSignalSpy changedSpy(&store, &ReplayStore::replaysChanged);
    store.removeReplayFileLink(replayPath);

    REQUIRE(changedSpy.count() == 1);
    const QList<Replay> replays = changedSpy.at(0).at(0).value<QList<Replay>>();
    REQUIRE(replays.size() == 1);
    CHECK_FALSE(replays.at(0).hasExternalPath);
}

TEST_CASE(
    "ReplayStore removeReplayFileLink on an untracked path is a harmless no-op") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir root(tempDir.path());

    ReplayStore store(tempDir.path(), tempDir.path());
    store.receiveInitialReplayPaths({});

    QSignalSpy changedSpy(&store, &ReplayStore::replaysChanged);
    store.removeReplayFileLink(root.filePath("never-existed.KWReplay"));

    CHECK(changedSpy.count() == 0);
}
