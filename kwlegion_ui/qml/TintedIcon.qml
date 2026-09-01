// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

import QtQuick.Controls.impl
import KWLegionUI

// A monochrome icon recolored to match the current theme - the source SVGs
// are fixed black, which reads as low-contrast against a dark background.
// Reuses IconImage, the same renderer Qt Quick Controls uses internally for
// Button/MenuItem's icon.color - MultiEffect's colorization was tried first
// but silently produced no visible tint here.
IconImage {
    color: Theme.lightMode ? Theme.dark : Theme.light
}
