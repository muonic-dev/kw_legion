#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QMutex>
#include <QQmlApplicationEngine>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>
#include <iostream>

#include "prospector.h"

namespace {

void fileMessageHandler(QtMsgType type, const QMessageLogContext& context,
                        const QString& msg) {
    static QMutex mutex;
    static QFile file;

    const QMutexLocker locker(&mutex);

    if (!file.isOpen()) {
        const QString logPath =
            QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
            "/kw_legion.log";
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
            level = "Debug";
            break;
        case QtInfoMsg:
            level = "Info";
            break;
        case QtWarningMsg:
            level = "Warning";
            break;
        case QtCriticalMsg:
            level = "Critical";
            break;
        case QtFatalMsg:
            level = "Fatal";
            break;
    }

    QTextStream stream(&file);
    stream << '[' << level << "] " << "<" << context.category << "> " << msg
           << "\n";
    // Only flush for the important stuff
    if (QtMsgType::QtWarningMsg <= type && type <= QtMsgType::QtCriticalMsg) {
        stream.flush();
        file.flush();
    }
}

}  // namespace

using namespace KWLegion;

// NOLINTNEXTLINE(modernize-avoid-c-arrays, cppcoreguidelines-avoid-c-arrays)
int main(int argc, char* argv[]) {
    QCoreApplication::setApplicationName("kw_legion");

    qInstallMessageHandler(fileMessageHandler);

    QGuiApplication app(argc, argv);
    QGuiApplication::setWindowIcon(
        QIcon(":/qt/qml/kw_legion/qml/CNCKW_Marked_of_Kane_logo.png"));

    // Background thread to run i/o jobs on
    // Currently, the only requirement is to move the I/O processing
    // off the GUI thread. We aren't trying to farm out to parse all the
    // replays discovered as fast as possible so just one thread here
    QThread ioThread;

    ReplayProspector replayProspector;
    replayProspector.moveToThread(&ioThread);

    QObject::connect(&ioThread, &QThread::started, &replayProspector,
                     &ReplayProspector::initialSweep);

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
    engine.loadFromModule("kw_legion", "Main");

    ioThread.start();

    const auto result = QGuiApplication::exec();

    ioThread.quit();
    ioThread.wait();

    return result;
}
