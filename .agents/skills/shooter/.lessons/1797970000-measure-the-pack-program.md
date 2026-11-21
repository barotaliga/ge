---
title: The pack's own program on our host is the record; measure it before porting a rule
date: 2026-11-21
scope: games/shooters_edge/rules
concepts: [black-box, pack-program, campaign, liquids, sounds, lab, clean-room]
---

To learn what Quake's game code does in a corner of play (the liquids, a pain sound, a clock),
run the pack's own progs.dat on our campaign host in a lab and record it: put the player where
the case happens each frame (origin, velocity zeroed, `link_edict`), set the server's own
`Edict::health` (the VM's copy is overwritten from it each frame), and read `Server::sounds` after
each frame for the program's sounds (`engine` false) with their channels. This is a black-box
measurement any implementer may take, no reader needed, and the same lab then holds our C++ rules
to it (`se-dive`). Feed a model the level the program SAW (the edict's before the frame), not the
one after: the program reads the last physics frame's.
