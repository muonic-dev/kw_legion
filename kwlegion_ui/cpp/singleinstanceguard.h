// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#pragma once

#include <QLocalServer>
#include <QObject>
#include <QString>

// Ensures only one instance of the application is running at a time.
//
// On construction, tries to connect to a QLocalServer already listening
// under `key`. If that succeeds, another instance owns it and this one is
// not primary. Otherwise this instance claims the name itself, becoming the
// primary (and the one future launches will detect).
class SingleInstanceGuard : public QObject {
    Q_OBJECT

   public:
    explicit SingleInstanceGuard(const QString& key, QObject* parent = nullptr);

    [[nodiscard]] bool isPrimaryInstance() const { return m_isPrimary; }

   signals:
    // Emitted (primary instance only) whenever another launch of the app
    // connects to check for a running instance.
    void activationRequested();

   private:
    QLocalServer m_server;
    bool m_isPrimary = false;
};
