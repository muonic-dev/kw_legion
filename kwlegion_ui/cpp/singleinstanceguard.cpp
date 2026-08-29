// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include "singleinstanceguard.h"

#include <QLocalSocket>

namespace {
// No response is expected on the connection - its existence alone is the
// signal that another instance is already listening. A generous timeout
// avoids a false "primary" verdict on a loaded machine still starting up
// the first instance.
constexpr int PROBE_TIMEOUT_MS = 500;
}  // namespace

SingleInstanceGuard::SingleInstanceGuard(const QString& key, QObject* parent)
    : QObject(parent), m_server(this) {
    QLocalSocket probe;
    probe.connectToServer(key);
    if (probe.waitForConnected(PROBE_TIMEOUT_MS)) {
        // Another instance is listening - this one is not primary.
        return;
    }

    // Nobody answered. That may genuinely mean we're first, or it may mean a
    // previous instance crashed and left a stale socket file behind
    QLocalServer::removeServer(key);
    m_isPrimary = m_server.listen(key);

    if (m_isPrimary) {
        // No payload, a connection at all means someone is asking for
        // activation
        connect(&m_server, &QLocalServer::newConnection, this, [this] {
            delete m_server.nextPendingConnection();
            emit activationRequested();
        });
    }
}
