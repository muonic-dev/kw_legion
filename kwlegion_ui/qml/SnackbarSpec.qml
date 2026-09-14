// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

import QtQml

// Description for a snackbar to be shown by a snackbar item
QtObject {
    property string title: ""
    property string description: ""
    property bool active: true

    signal confirmed
    signal dismissed
}
