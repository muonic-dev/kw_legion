/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include "replayfilterqueryparser.h"

#include "filterquery.h"
#include "replayfilterquery.h"

namespace KWLegionCore {
ReplayFilterQueryParser::ReplayFilterQueryParser(QObject* parent)
    : QObject(parent), m_current(new TautologyFilterQuery(this)) {}

QString ReplayFilterQueryParser::queryText() const { return m_text; }

void ReplayFilterQueryParser::setQueryText(const QString& value) {
    m_text = value;
    // Memory management by QObject semantics
    // NOLINTBEGIN(cppcoreguidelines-owning-memory)
    const FilterQuery* previous = m_current;
    if (value.isEmpty()) {
        m_current = new TautologyFilterQuery(this);
    } else {
        m_current = new AnyTextReplayFilterQuery(value, this);
    }
    emit queryTextChanged();
    emit queryChanged();
    delete previous;
    // NOLINTEND(cppcoreguidelines-owning-memory)
}

QObject* ReplayFilterQueryParser::query() const { return m_current; }

}  // namespace KWLegionCore