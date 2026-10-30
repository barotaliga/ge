---
title: Tell a demo's format by its first bytes, and settle a padded struct by walking records
date: 2026-10-30
scope: games/shooters_edge/client/demo.cpp, games/shooters_edge/client/timedemo.cpp
concepts: [demo, recording, format-detection, usercmd-size, clean-slate, timedemo]
---

The multiplayer client's recording has an extension whose letters the clean-slate check refuses
anywhere in the tree, so nothing may branch on it. The readers tell the formats apart by their
bytes instead (the single-player demo opens with the music track as digits and a newline; a
client recording with a finite time and a record type 0..2), and `find_demo` finds a name with
no extension by its stem in the packs. The command record's struct is 24 bytes or 28 from a
client that pads it: checking only the record right after the first command misjudged a 28-byte
file whose angles were zero (a zero float reads as a valid time and type). Walking 16 records
under the 24-byte guess and checking each time, type and packet length settles it.
