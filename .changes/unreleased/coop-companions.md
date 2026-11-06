---
type: Added
title: Co-op in the campaign, with companion bots
---

Shooter's Edge's campaign plays in coop: `coopgame [bots] [novice|average|expert] [map]` starts a coop game (teamplay on, so players never harm each other) with up to three companion bots, and `addbot` and `removebot` change the party during it. A companion plays beside you rather than for you: it keeps at your shoulder, steps out of your way, kills only what comes for you or itself, holds its fire while you are in the line, presses the button you stop by, catches up at a hop when left behind, never fires a blast weapon at something within reach of a player, never takes the exit (it stands beside it and jumps to show you the way out), and while you wait it spins, looks at items, glances at you, hops round you, jumps and sways. The shooter plugin's senses take a game's marked non-player foes (monsters) as contacts, and `plugins/shooter/idle.hpp` is the idle repertoire any waiting bot can use. Lab `se-coop` charts it.

A leader bot plays a campaign map to its exit the way a first-time player does: the exit when a way leads there, else the button, key or trigger that opens a shut door onto places not yet seen, else exploring. Lab `se-coop-episode` plays episode 1 on your own pack with a companion beside him and draws every map. The campaign program now jumps, presses a button with health when it is shot, and opens doors that start open where the map puts them.
