/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */
#pragma once

#include <QString>
#include <stdexcept>

namespace KWLegionCore {

class StorageException : public std::runtime_error {
   public:
    StorageException(const QString& what)
        : std::runtime_error(what.toStdString()) {}
};
}  // namespace KWLegionCore