/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <QString>
#include <stdexcept>


namespace KWLegionCore {

class IngestionException : public std::runtime_error {
   public:
    IngestionException(const QString& what)
        : std::runtime_error(what.toStdString()) {}
};
}  // namespace KWLegionCore