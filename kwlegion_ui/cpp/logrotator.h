/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <QDebug>
#include <QFile>
#include <QMessageLogContext>
#include <QMutex>
#include <QObject>
#include <QThread>
#include <QWaitCondition>
#include <array>
#include <concepts>
#include <cstddef>
#include <optional>

/*
 * Implementation of a rotating qt log handler
 * We want to handle fast log messages while also rotating the log files and
 * encoding a cap on the number of log files that are retained to prevent
 * unbounded disk usage.
 *
 * This implements a circular buffer for log messages. Messages are pushed into
 * the buffer and drained on another thread so that in typical use disc i/o
 * (including rotating the files themselves) does not block
 */
namespace KWLegionUI {

// The default buffer size is quite large to accommodate bursts without stalls.
// This may need to be tuned if it appears there is large constant memory usage
template <typename T, std::size_t BufferSize = 10000>
    requires std::movable<T> && std::default_initializable<T>
class RingBuffer {
   public:
    RingBuffer() = default;

    void enqueue(T&& item) {
        const QMutexLocker locker(&m_condGuard);
        while ((m_head + 1) % BufferSize == m_tail) {
            m_bufferNotFull.wait(&m_condGuard);
        }
        (*m_buffer)[m_head] = std::move(item);
        m_head = (m_head + 1) % BufferSize;
        m_bufferNotEmpty.wakeOne();
    }

    T dequeue() {
        const QMutexLocker locker(&m_condGuard);
        while (m_head == m_tail) {
            m_bufferNotEmpty.wait(&m_condGuard);
        }
        T item = std::move((*m_buffer)[m_tail]);
        m_tail = (m_tail + 1) % BufferSize;
        m_bufferNotFull.wakeOne();
        return item;
    }

   private:
    QMutex m_condGuard;
    QWaitCondition m_bufferNotFull;
    QWaitCondition m_bufferNotEmpty;
    std::unique_ptr<std::array<T, BufferSize>> m_buffer =
        std::make_unique<std::array<T, BufferSize>>();
    std::size_t m_head = 0;
    std::size_t m_tail = 0;
};

struct LogMessage {
    QtMsgType type = QtDebugMsg;
    QString category;
    QString file;
    int line = 0;
    QString function;
    QString msg;
};

class LogRotator : public QThread {
   public:
    LogRotator(const QString& logPath, int maxRotations = 3);

    void logMessage(QtMsgType type, const QMessageLogContext& context,
                    const QString& msg);

    void run() override;

    void stop();

   private:
    void rotateIfNecessary();

    void rotate();

    void startup();

    void fileLog(const LogMessage& message);
    // This exists as a fallback to get diagnostics if nothing else works
    void consoleLog(const LogMessage& message);

    // Ring buffer for storing the log messages
    // std::optional is default construtible and if we ever get an actual
    // nullopt that signifies shutdown requested
    RingBuffer<std::optional<LogMessage>> m_buffer;

    QFile m_logFile;
    int m_maxRotations = 3;
    int m_rotatePollCheck = 0;
};
}  // namespace KWLegionUI