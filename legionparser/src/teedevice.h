/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <QIODevice>
#include <functional>
#include <optional>

namespace LegionParser {

/**
 * A device that can be switched into tee mode and drive a callback with all
 * read data.
 *
 * The TeeDevice is always in unbuffered mode. Control actual buffering through
 * the wrapped device
 */
class TeeDevice : public QIODevice {
   public:
    template <std::invocable<QByteArrayView> Fn>
    TeeDevice(QIODevice& wrapped, Fn&& sink)
        : m_sink{std::forward<Fn>(sink)}, m_wrapped{wrapped} {
        setOpenMode(QIODevice::ReadOnly | QIODevice::Unbuffered);
    }

    TeeDevice(QIODevice& wrapped) : m_sink{std::nullopt}, m_wrapped{wrapped} {
        setOpenMode(QIODevice::ReadOnly | QIODevice::Unbuffered);
    }

    TeeDevice(const TeeDevice& device) = delete;
    TeeDevice(TeeDevice&& device) = delete;
    TeeDevice& operator=(const TeeDevice& device) = delete;
    TeeDevice& operator=(TeeDevice&& device) = delete;

    ~TeeDevice() override;

    [[nodiscard]] bool isSequential() const override;

    template <std::invocable<QByteArrayView> Fn>
    void setSink(Fn&& sink) {
        m_sink = std::forward<Fn>(sink);
    }

    void clearSink() { m_sink = std::nullopt; }

   protected:
    qint64 writeData(const char* data, qint64 maxSize) override;
    qint64 readData(char* data, qint64 maxSize) override;

   private:
    std::optional<std::function<void(QByteArrayView)>> m_sink;
    QIODevice& m_wrapped;
};
}  // namespace LegionParser
