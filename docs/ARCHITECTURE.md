# Architecture

This document describes how LEGION Replay Manager's components fit together
system-wide: ownership, threading, and the flow of a replay from disk to UI.
Each component's own header comment remains the source of truth for that
component's individual contract; this doc is for the interactions between
them.

## Component map

| Component | Location | Responsibility |
|---|---|---|
| `LegionParser::Parser` | `legionparser/` | Parses the `.KWReplay` binary format from a `QIODevice` into `ReplayMetadata` (players, factions, map, timestamp, checksum). This is only a format parser. There is no knowledge of persistence of the filesystem beyond whatever QIODevice is handed in. |
| `KWLegionCore::ReplayProspector` | `core/` | Watches the configured replay directory (`QFileSystemWatcher`), performs an initial sweep, and emits paths as replays are discovered or removed. |
| `KWLegionCore::ReplayStore` | `core/` | Owns the SQLite-backed catalog. Parses paths it's handed via `LegionParser::Parser`, ingests new/changed replays, and emits domain events. |
| `KWLegionCore::StoreModel` | `core/` | `QAbstractListModel` adapter exposing the store's replay list to QML. |
| App shell | `src/` | Wires the components together, owns the background I/O thread, registers the QML singleton, boots the engine. |
| UI | `src/qml/` | `Main.qml` / `NavRail.qml` shell plus pages (`ReplaysPage`, `StatisticsPage`, `SettingsPage`, `AboutPage`) that bind to the `StoreModel` singleton. |

## Threading

Two threads are in play:

- **GUI thread** — `QGuiApplication`, the QML engine, and the models like `StoreModel`
- **`ioThread`** — owns `ReplayProspector` and `ReplayStore` so directory scanning, file parsing, and
  SQLite access never block the UI.

## Data flow

### Startup

The `ioThread` starts and `QThread::started` triggers the `ReplayProspector` initialSweep.
The `ReplayProspector` scans the entire directory and builds its own model of the filesystem and arms
a `QFileSystemWatcher` for paths that are found. It tracks all seen files.
`QThread::started` also triggers the `ReplayStore` to do any outstanding migrations/maintenance work.

Once the initial sweep is complete `ReplayProspector` emits `initialSweepCompleted(paths)` with all found 
replay paths. This is the signal for `ReplayStore` to determine what changed while the application wasn't running. This means pruning its list of 'external' files for those that are missing. Ingesting any replays
that have appeared, etc. Once this is done, `ReplayStore` will emit `replaysLoaded(replays)` with the initial
data model to populate the view. 

### Runtime

As the `ReplayProspector` receives directory changed events it determines which files were added or removed
based on its own cached filesystem state. It emits the events `replayFileChanged` and `replayFileRemoved` for this update. The `ReplayStore` listens for these with `analyzeReplayFile` and `removeReplayFile` to synchronize
the database state. It subsequently emits events to keep the `StoreModel` in sync. There is now a 
`replayDiscovered`, `replayChanged`, and `replayRemoved`. These are keyed on a `Replay` type that bears a checksum. Functionally, the `ReplayStore` sits betwen the `Prospector` and the `Model` and converts between paths and checksums. Logically, a replay is not a file so the changed event can bear information like the replay is now present in the Documents folder or it was removed.
