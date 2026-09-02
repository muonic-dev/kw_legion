// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include <kwlegion_core/appinfo.h>
#include <kwlegion_core/ingestionmodel.h>
#include <kwlegion_core/metatypes.h>
#include <kwlegion_core/prospector.h>
#include <kwlegion_core/replaystore.h>
#include <kwlegion_core/replaystoremodel.h>
#include <kwlegion_core/settings.h>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QMutex>
#include <QQmlApplicationEngine>
#include <QTextStream>
#include <QThread>
#include <QWindow>
#include <cstdio>

#include "singleinstanceguard.h"

namespace {

void logMessageHandler(QtMsgType type, const QMessageLogContext& context,
                       const QString& msg) {
    static QMutex mutex;
    static QFile file;

    const QMutexLocker locker(&mutex);

    if (!file.isOpen()) {
        const QString logPath = KWLegionCore::AppInfo::defaultLogFilePath();

        const QFileInfo logInfo(logPath);
        QDir().mkpath(logInfo.dir().path());

        file.setFileName(logPath);
        // Keep trying to open it if this fails
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                       QIODevice::Text)) {
            return;
        }
    }

    QString level;
    switch (type) {
        case QtDebugMsg:
            level = QLatin1String("Debug");
            break;
        case QtInfoMsg:
            level = QLatin1String("Info");
            break;
        case QtWarningMsg:
            level = QLatin1String("Warning");
            break;
        case QtCriticalMsg:
            level = QLatin1String("Critical");
            break;
        case QtFatalMsg:
            level = QLatin1String("Fatal");
            break;
    }

    QTextStream fileStream(&file);
    QTextStream stderrStream(stderr);

    fileStream << '[' << level << "] " << "<" << context.category << "> " << msg
               << " <" << context.line << "> " << "\n";
    stderrStream << '[' << level << "] " << "<" << context.category << "> "
                 << " <" << context.line << "> " << msg << "\n";

    // Only flush for the important stuff
    if (QtMsgType::QtWarningMsg <= type && type <= QtMsgType::QtCriticalMsg) {
        fileStream.flush();
        file.flush();

        stderrStream.flush();
        fflush(stderr);
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

// NOLINTNEXTLINE(modernize-avoid-c-arrays, cppcoreguidelines-avoid-c-arrays)
int main(int argc, char* argv[]) {
    QCoreApplication::setOrganizationName("Muonic-Dev");
    QCoreApplication::setOrganizationDomain("muonic-dev.github.io");

    QCoreApplication::setApplicationName(
        KWLegionCore::DEBUG_BUILD ? "kw_legion-debug" : "kw_legion");

    qInstallMessageHandler(logMessageHandler);

    QGuiApplication app(argc, argv);

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
            // are running
            ? QIcon(":/qt/qml/KWLegionUI/ico/CNCKW_Traveler-59_Logo.png")
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

    ReplayProspector replayProspector;
    replayProspector.moveToThread(&ioThread);
    ReplayStore replayStore(ReplayProspector::defaultReplayDirectory());
    replayStore.moveToThread(&ioThread);

    QObject::connect(&ioThread, &QThread::finished, &replayStore,
                     &ReplayStore::stop);

    QObject::connect(&ioThread, &QThread::started, &replayProspector,
                     &ReplayProspector::initialSweep);
    QObject::connect(&replayProspector,
                     &ReplayProspector::initialSweepCompleted, &replayStore,
                     &ReplayStore::receiveInitialReplayPaths);
    QObject::connect(&replayProspector, &ReplayProspector::replayFileChanged,
                     &replayStore, &ReplayStore::analyzeReplayFile);
    QObject::connect(&replayProspector, &ReplayProspector::replayFileRemoved,
                     &replayStore, &ReplayStore::removeReplayFile);

    engine.loadFromModule("KWLegionUI", "Main");

    QObject::connect(
        &singleInstanceGuard, &SingleInstanceGuard::activationRequested, &app,
        [&engine] {
            auto* rootWindow =
                qobject_cast<QWindow*>(engine.rootObjects().constFirst());
            if (rootWindow == nullptr) {
                return;
            }
            rootWindow->show();
            rootWindow->raise();
            rootWindow->requestActivate();
        });

    auto* settings = requireSingleton<Settings>(engine, "Settings");
    settings->setAutostartMechanism(
        KWLegionCore::createPlatformAutostartMechanism());

    auto* replayStoreModel =
        requireSingleton<ReplayStoreModel>(engine, "ReplayStoreModel");
    replayStoreModel->setStore(&replayStore);

    auto* ingestionModel =
        requireSingleton<IngestionModel>(engine, "IngestionModel");
    ingestionModel->setStore(&replayStore);

    ioThread.start();

    const auto result = QGuiApplication::exec();

    ioThread.quit();
    ioThread.wait();

    return result;
}
