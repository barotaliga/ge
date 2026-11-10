---
type: Added
title: A player's CLI for every game, starting with shooters_edge_cli
---

Every game on the engine can ship `<game>_cli` (`ge::gamecli`, `ge_add_game_cli`): a command line a player's agent drives to do everything but the playing. It lists, reads and changes the player's settings (checked against the catalog the window now writes beside `config.cfg`), reads and writes a dedicated server's `server.cfg` with where each value came from, lists the servers on the LAN, at the masters and those named, says who is on one, runs a server's remote console, hosts a game (a dedicated server kept running, or a listen server in the window), lists and stops what it hosted, joins a friend's game and reads the logs of what it started. Every answer is text or one JSON object (`--json`), and `skill` prints a SKILL.md an agent installs to drive the game. A plugin adds its own group (classic enhance's `content`), a game its own rows (Shooter's Edge's `demos`). The dedicated server's output is now line-buffered, so its log reads as it runs.
