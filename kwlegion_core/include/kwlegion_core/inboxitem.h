/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <QtQml/qqmlregistration.h>

#include <QDateTime>
#include <QString>

// InboxType/InboxItem live in the public API rather than kwlegion_core's
// private src/ despite otherwise being good candidates (see transaction.h).
// They're expected to be emitted from ReplayStore (which
// runs on a background thread) to something like IngestionModel, and any
// type used as a QObject signal argument on a publicly-declared class needs
// to be complete wherever that class's header is compiled - moc generates
// metatype-lookup code for every signal argument regardless of whether a
// given connection ends up queued. Q_DECLARE_METATYPE in particular
// static_asserts completeness at the exact point it's written, which rules
// out a forward-declare-and-define-elsewhere split. We independently hit
// and confirmed the same wall trying to privatize Replay. A type-erased
// signal boundary (QVariantList instead of QList<InboxItem> in the signal
// signature, packing/unpacking via QVariant in the .cpp files) might avoid
// this if keeping these private becomes worth the indirection later, but it
// wasn't attempted here.
namespace KWLegionCore::InboxTypeQml {
Q_NAMESPACE
QML_NAMED_ELEMENT(InboxType)

enum class InboxType : std::uint8_t { PENDING, TORN, CORRUPT };

Q_ENUM_NS(InboxType)
}  // namespace KWLegionCore::InboxTypeQml

namespace KWLegionCore {
// InboxTypeQml::InboxType is the canonical definition - it has to live in a
// dedicated namespace so Q_NAMESPACE doesn't collide with the rest of
// KWLegionCore. This alias just makes it ergonomic to write InboxType
// elsewhere in kwlegion_core's private code.
using InboxType = InboxTypeQml::InboxType;

// Pending items that need to be parsed. ReplayStore and IngestionModel
// communicate in this type
struct InboxItem {
    // The primary id of inbox items
    QString path;
    InboxType type;
    QDateTime observedAt;
};
}  // namespace KWLegionCore