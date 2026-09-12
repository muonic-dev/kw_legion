# CHANGELOG

## 0.3.1

### Changed

- If replays need to be re-analyzed avoid scrolling to a random place in the view

### Fixed

- A crash that can happen due to invalid data from pre-0.2.0

## 0.3.0

### Added

- Replay time is now visible and can be filtered on using `longer:` and `shorter:` with the `m:ss` format.
- The installer will try and close the application, if this is not possible a dialog box will pop up.
- The installer will offer to launch the application now.
- The autostart function (which is important for proper functioning) will be offered to be setup until the user makes a decision
- Initial replay analysis view with an APM graph average of 5 second windows.

### Changed
- Linking a replay into the Replays folder now uses the matchtitle and part of the hash instead of the full hash for legibility


## 0.2.0

### Added

- Options to start automatically on login and start with the main window minimized
- Setting to control minimize to tray
- It is now possible to edit the match title and these edits will be used to choose file names when exporting
- An inbox to provide indication on the current status of replays that are waiting or have failed to parse
- Improved search - use magic prefixes like map: title:, player: or patch: to search
    - on:/before:/after: will search via date comparison (using whatever your system configured locale accepts)
    - known quirk of after: the search includes the day input
    - quote multiword filters like map:"Atacama Road" 

### Fixed

- Crash when parsing 8 player replays
- Some cases where newly written Last Replay.KWReplay woulnd't be ingested correctly
- Poor icon visibility in dark mode

### Changed

- The bulk operations have moved to a persistent toolbar. There is now a selection menu for all/none/invert.
- The layout of the replay display has been improved
- The patch version now displays R at its header and will not return things like R24BETA for things like WEC map WIPs

