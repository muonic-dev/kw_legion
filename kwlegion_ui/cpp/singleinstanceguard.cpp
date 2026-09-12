// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include "singleinstanceguard.h"

#include <QLocalSocket>
#include <QObject>
#include <QTimer>

namespace {
// No response is expected on the connection - its existence alone is the
// signal that another instance is already listening. A generous timeout
// avoids a false "primary" verdict on a loaded machine still starting up
// the first instance.
constexpr int PROBE_TIMEOUT_MS = 500;

enum class Request : std::uint8_t {
    // For backwards compatibility we treat unrecognized or missing
    // messages on the local socket as an activation request
    Activate = 0,
    Quit = 1,
};

constexpr const char* ACTIVATE_MESSAGE = "\0";

}  // namespace

SingleInstanceGuard::SingleInstanceGuard(const QString& key, QObject* parent)
    : QObject(parent), m_server(this) {
    QLocalSocket probe;
    probe.connectToServer(key);
    if (probe.waitForConnected(PROBE_TIMEOUT_MS)) {
        // Another instance is listening so request that it activate
        probe.write(ACTIVATE_MESSAGE, 1);
        // Guarantee that we wrote successfully
        probe.waitForBytesWritten(PROBE_TIMEOUT_MS);
        probe.close();
        return;
    }

    // Nobody answered. That may genuinely mean we're first, or it may mean a
    // previous instance crashed and left a stale socket file behind
    QLocalServer::removeServer(key);
    m_isPrimary = m_server.listen(key);

    // This branch means we won a race with anything else currently starting up
    if (m_isPrimary) {
        // No payload, a connection at all means someone is asking for
        // activation
        connect(&m_server, &QLocalServer::newConnection, this, [this] {
            QLocalSocket* socket = m_server.nextPendingConnection();
            auto* timer = new QTimer(socket);  // delete on teardown
            timer->setSingleShot(PROBE_TIMEOUT_MS);

            // Safe for concurrent modification; single thread event loop
            auto settled = std::make_shared<bool>(false);
            auto finished = [this, socket, timer, settled](bool haveData) {
                if (*settled) {
                    return;
                }
                *settled = true;
                timer->stop();

                if (haveData &&
                    socket->read(1) ==
                        QByteArray(1, static_cast<char>(Request::Quit))) {
                    emit quitRequested();
                } else {
                    emit activationRequested();
                }
                socket->deleteLater();
            };

            QObject::connect(socket, &QLocalSocket::readyRead, socket,
                             [finished] { finished(true); });
            QObject::connect(timer, &QTimer::timeout, socket,
                             [finished] { finished(false); });
            timer->start(PROBE_TIMEOUT_MS);
        });
    }
}
