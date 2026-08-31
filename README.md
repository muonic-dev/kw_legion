
# LEGION Replay Manager

Organize your replays because Kane's Wrath built-in replay doesn't support filtering.

LEGION will gather every replay in your replay folder (including the rolling Last Replay)
and track it in a local database. You can then filter by player, map, faction, time and more
and toggle which replays are placed in the replay folder.

![Replay screen](docs/Replay%20Screen.png)

## ☢️ _*UNDER DEVELOPMENT/UNSTABLE*_ ☢️

This software is under active developmet and may be unstable. You should definitely back up any replays that you consider important. If you do encounter problems feel free to open an issue.

## License

This project is licensed under the GNU General Public License v3.0 or later - see the [LICENSE](LICENSE) file for details.

## Development

⚠️ This project is open source because I believe in it. However, as I have limited time if you drive by PR it is very unlikely that I will review and/or merge.

- [Architecture](docs/ARCHITECTURE.md) - how the components (parser, core, UI) fit together and interact.
- [test-harness/trickle.py](test-harness/trickle.py) - A python script to slowly copy a replay from one location to another. 
It writes 4k blocks and flushes them then sleeps for 4s to simulate the way KW seems to take its sweet time writing kbs of data.

## AI Disclosure

This program has been developed with the assistance of AI. The primary contribution has been implementing/debugging the build system, the UI,
and generating tests. I believe I have a firm understanding of all the code currently in program.
