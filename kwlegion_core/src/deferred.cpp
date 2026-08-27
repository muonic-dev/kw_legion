/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include "deferred.h"

namespace KWLegionCore {
Deferred::Deferred(QObject* parent)
    : QObject(parent), m_trigger(new QTimer(this)) {}

bool Deferred::readyForParsing(const QString& path) { return false; }

void Deferred::waitForReady(const QString& path) {}

void stop() {}

}  // namespace KWLegionCore