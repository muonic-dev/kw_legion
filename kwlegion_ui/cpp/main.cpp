// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include <kwlegion_core/appinfo.h>
#include <kwlegion_core/ingestionmodel.h>
#include <kwlegion_core/metatypes.h>
#include <kwlegion_core/prospector.h>
#include <kwlegion_core/queries.h>
#include <kwlegion_core/replaystore.h>
#include <kwlegion_core/replaystoremodel.h>
#include <kwlegion_core/settings.h>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QLatin1StringView>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>
#include <QUrl>
#include <QWidgetSet>
#include <QWindow>
#include <Qt>
#include <QtLogging>
#include <cstdio>

#include "kwlegion_core/autostart.h"
#include "logrotator.h"
#include "singleinstanceguard.h"

namespace {

// Adapt to the Qt log handler
std::optional<
    std::function<void(QtMsgType, const QMessageLogContext&, const QString&)>>
    messageTrampoline;

void logMessageHandler(QtMsgType type, const QMessageLogContext& context,
                       const QString& msg) {
    if (messageTrampoline.has_value()) {
        (*messageTrampoline)(type, context, msg);
    }
}

template <typename T>
T* requireSingleton(QQmlApplicationEngine& engine, const char* typeName) {
    auto* instance = engine.singletonInstance<T*>("KWLegionCore", typeName);
    if (instance == nullptr) {
        qFatal() << "No KWLegionCore." << typeName << " singleton";
    }
    return instance;
}

}  // namespace

using namespace KWLegionCore;
using namespace KWLegionUI;

// NOLINTNEXTLINE(modernize-avoid-c-arrays, cppcoreguidelines-avoid-c-arrays)
int main(int argc, char* argv[]) {
    QCoreApplication::setOrganizationName("Muonic-Dev");
    QCoreApplication::setOrganizationDomain("muonic-dev.github.io");

    QCoreApplication::setApplicationName(
        KWLegionCore::DEBUG_BUILD ? "kw_legion-debug" : "kw_legion");

    QGuiApplication app(argc, argv);

    LogRotator logRotator(KWLegionCore::AppInfo::defaultLogFilePath());
    logRotator.start();

    messageTrampoline = [&logRotator](QtMsgType type,
                                      const QMessageLogContext& context,
                                      const QString& msg) {
        logRotator.logMessage(type, context, msg);
    };
    qInstallMessageHandler(logMessageHandler);

    const SingleInstanceGuard singleInstanceGuard(
        QCoreApplication::applicationName());
    if (!singleInstanceGuard.isPrimaryInstance()) {
        qInfo() << "Another instance of kw_legion is already running - "
                   "exiting.";
        return 0;
    }

    if (QGuiApplication::arguments().contains(QStringLiteral("--minimized"))) {
        qInfo() << "Start minimized requested";
        AppInfo::setStartMinimized(true);
    }

    QGuiApplication::setWindowIcon(
        KWLegionCore::DEBUG_BUILD
            // Different icon so we can determine which app is which when both
            // are running BH looks like stop
            ? QIcon(":/qt/qml/KWLegionUI/ico/CNCKW_Black_Hand_Logo.png")
            : QIcon(":/qt/qml/KWLegionUI/ico/CNCKW_Marked_of_Kane_Logo.png"));

    registerMetaTypes();

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [](const QUrl& url) {
            qCritical() << "Failed to create QML object from" << url;
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    QObject::connect(&engine, &QQmlEngine::warnings, &app,
                     [](const QList<QQmlError>& errors) {
                         for (const auto& error : errors) {
                             qWarning() << error.toString();
                         }
                     });

    // Background thread to run i/o jobs on
    // Currently, the only requirement is to move the I/O processing
    // off the GUI thread. We aren't trying to farm out to parse all the
    // replays discovered as fast as possible so just one thread here
    QThread ioThread;

    const QString statePath =
        QStandardPaths::writableLocation(QStandardPaths::StateLocation);
    Queries queries(statePath + "/replays.db", "kwlegion_store");
    queries.moveToThread(&ioThread);

    ReplayProspector replayProspector;
    replayProspector.moveToThread(&ioThread);
    ReplayStore replayStore(queries, ReplayProspector::defaultReplayDirectory(),
                            statePath);
    replayStore.moveToThread(&ioThread);
    ReplayAnalyzer replayAnalyzer(replayStore);
    replayAnalyzer.moveToThread(&ioThread);

    QObject::connect(&ioThread, &QThread::finished, &replayStore,
                     &ReplayStore::stop);

    // Connected ahead of initialSweep: QThread::started's direct connections
    // run synchronously in connection order, so this guarantees the schema
    // exists before the prospector's results ever reach ReplayStore.
    QObject::connect(&ioThread, &QThread::started, &queries, &Queries::init);
    QObject::connect(&ioThread, &QThread::started, &replayProspector,
                     &ReplayProspector::initialSweep);
    QObject::connect(&replayProspector,
                     &ReplayProspector::initialSweepCompleted, &replayStore,
                     &ReplayStore::receiveInitialReplayPaths);
    QObject::connect(&replayProspector, &ReplayProspector::replayFileChanged,
                     &replayStore, &ReplayStore::synopsizeReplayFile);
    QObject::connect(&replayProspector, &ReplayProspector::replayFileRemoved,
                     &replayStore, &ReplayStore::removeReplayFile);

    engine.loadFromModule("KWLegionUI", "Main");

    QObject::connect(
        &singleInstanceGuard, &SingleInstanceGuard::activationRequested, &app,
        [&engine] {
            if (engine.rootObjects().isEmpty()) {
                return;
            }
            auto* rootWindow =
                qobject_cast<QWindow*>(engine.rootObjects().constFirst());
            if (rootWindow == nullptr) {
                return;
            }
            rootWindow->show();
            rootWindow->raise();
            rootWindow->requestActivate();
        });

    QObject::connect(&singleInstanceGuard, &SingleInstanceGuard::quitRequested,
                     &app, [&engine] {
                         QObject* root = engine.rootObjects().first();
                         if (root == nullptr) {
                             return;
                         }
                         // The property used to bypass the shutdown in Main.qml
                         root->setProperty("quitting", true);
                         QGuiApplication::quit();
                     });

    auto* settings = requireSingleton<Settings>(engine, "Settings");
    settings->setAutostartMechanism(
        KWLegionCore::createPlatformAutostartMechanism());

    auto* replayStoreModel =
        requireSingleton<ReplayStoreModel>(engine, "ReplayStoreModel");
    replayStoreModel->finishInit(&replayStore, &replayAnalyzer);

    auto* ingestionModel =
        requireSingleton<IngestionModel>(engine, "IngestionModel");
    ingestionModel->finishInit(&replayStore);

    ioThread.start();

    const auto result = QGuiApplication::exec();

    ioThread.quit();
    ioThread.wait();

    logRotator.stop();
    logRotator.wait();

    return result;
}
