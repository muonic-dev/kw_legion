/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include "logrotator.h"

#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QtLogging>
#include <iostream>
#include <optional>
#include <utility>

namespace KWLegionUI {
LogRotator::LogRotator(const QString& logPath, int maxRotations)
    : m_logFile(logPath), m_maxRotations(maxRotations) {}

void LogRotator::logMessage(QtMsgType type, const QMessageLogContext& context,
                            const QString& msg) {
    LogMessage message;
    message.type = type;
    message.category = context.category;
    message.file = context.file;
    message.line = context.line;
    message.function = context.function;
    message.msg = msg;
    m_buffer.enqueue(std::make_optional(std::move(message)));
}

void LogRotator::run() {
    startup();
    // Startup opens the message

    std::optional<LogMessage> messageOpt = m_buffer.dequeue();
    while (messageOpt.has_value()) {
        const LogMessage message = std::move(messageOpt.value());

        if (m_logFile.isOpen()) {
            fileLog(message);
        } else {
            consoleLog(message);
        }

        rotateIfNecessary();

        messageOpt = m_buffer.dequeue();
    }
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
}

void LogRotator::stop() { m_buffer.enqueue(std::nullopt); }

void LogRotator::startup() {
    if (!QDir().mkpath(QFileInfo(m_logFile.fileName()).dir().path())) {
        std::cerr << "Failed to create log directory: "
                  << QFileInfo(m_logFile.fileName()).dir().path().toStdString()
                  << '\n';
    }
    rotate();
}

void LogRotator::fileLog(const LogMessage& message) {
    QString level;
    switch (message.type) {
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

    QTextStream fileStream(&m_logFile);

    fileStream << '[' << level << "] " << "<" << message.category << "> "
               << message.msg << " <" << message.line << "> " << "\n";

    if (QtMsgType::QtWarningMsg <= message.type &&
        message.type <= QtMsgType::QtCriticalMsg) {
        fileStream.flush();
        m_logFile.flush();
    }
}

void LogRotator::consoleLog(const LogMessage& message) {
    QString level;
    switch (message.type) {
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

    QTextStream stderrStream(stderr);
    stderrStream << '[' << level << "] " << "<" << message.category << "> "
                 << message.msg << " <" << message.line << "> " << "\n";
    if (QtMsgType::QtWarningMsg <= message.type &&
        message.type <= QtMsgType::QtCriticalMsg) {
        stderrStream.flush();
        fflush(stderr);
    }
}

// Every 10_000 messages we will check to see if we need to rotate
constexpr qsizetype LOG_FILE_POLL_INTERVAL = 10000;

constexpr qsizetype MAX_LOG_FILE_SIZE =
    static_cast<qsizetype>(50 * 1024 * 1024);  // 50 MB

void LogRotator::rotateIfNecessary() {
    ++m_rotatePollCheck;
    if (m_rotatePollCheck >= LOG_FILE_POLL_INTERVAL) {
        m_rotatePollCheck = 0;
        if (m_logFile.size() > MAX_LOG_FILE_SIZE) {
            rotate();
        }
    }
}

void LogRotator::rotate() {
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }

    const QString baseLogPath = m_logFile.fileName();
    QFile maxLogFile(baseLogPath + QString(".%1").arg(m_maxRotations));

    if (!maxLogFile.remove()) {
        std::cerr << "Failed to remove max log file: "
                  << maxLogFile.fileName().toStdString() << '\n';
    }

    // Now, move everything up one
    for (int i = m_maxRotations; i > 0; --i) {
        const QString sourcePath =
            baseLogPath + (i - 1 > 0 ? QString(".%1").arg(i - 1) : QString(""));
        const QString destPath = baseLogPath + QString(".%1").arg(i);
        if (QFile::rename(sourcePath, destPath)) {
            std::cerr << "Failed to rename log file from "
                      << sourcePath.toStdString() << " to "
                      << destPath.toStdString() << '\n';
        }
    }

    // Note: If rotation ever fails this will degrade to truncating the file at
    // the limit

    // Now that we ahve rotated the log files re-open the log file
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        std::cerr << "Failed to open log file: "
                  << m_logFile.fileName().toStdString() << '\n';
    }
}
}  // namespace KWLegionUI