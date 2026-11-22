---
type: Added
title: QuakeWorld's map check, god and give by player id, the dedicated server's terminal, the look keys and the turtle and net icons
---

A QuakeWorld server now turns away a player whose copy of the map differs from its own (`sv_mapcheck`, on by default), telling them which map to get. On a server started with cheats, the operator's `god <id>` and `give <id> <item> <count>` reach that player, and players there get `god`, `give`, `noclip` and `fly` with the classic words. A dedicated server takes console lines typed on its own terminal, and `fraglogfile` writes every frag to a frag_<n>.log file that the `log` query serves. CUSTOMIZE CONTROLS gains look up, look down, center view, mouse look and keyboard look, and the classic menu scrolls a page too long for the screen. `showturtle 1` shows the turtle after a run of slow frames, and the net icon shows while a joined server goes quiet.
