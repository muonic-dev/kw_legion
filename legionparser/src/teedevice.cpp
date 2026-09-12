/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include "teedevice.h"

namespace LegionParser {
TeeDevice::~TeeDevice() = default;

// Tee device is always sequential
bool TeeDevice::isSequential() const { return true; }

qint64 TeeDevice::writeData(const char* /* data */, qint64 /* maxSize */) {
    setErrorString(QStringLiteral("write unsupported"));
    return -1;
}

qint64 TeeDevice::readData(char* data, qint64 maxSize) {
    // Use the read into char* so we don't have to do multiple copies
    const qint64 read = m_wrapped.read(data, maxSize);
    if (read < 0) {
        setErrorString(m_wrapped.errorString());
        return read;
    }
    // There's no reason to write to the sink if we are already at EOF
    if (m_sink.has_value() && read > 0) {
        (*m_sink)(QByteArrayView(data, read));
    }
    return read;
}
}  // namespace LegionParser