---
title: Count a stage's files against the pack's, and match names in any letter case
date: 2026-11-14
scope: plugins/classicenhance/profile.cpp, plugins/classicenhance/cached.cpp
concepts: [sounds-stage, asset-name, letter-case, q2, cached-sound, ce-sounds, any-case]
---

The sounds stage said "500 sounds" on Quake II's pack, which holds 546: the 46 it skipped end in
`.WAV`, and `asset_name` matched the kind's folder and extension exactly. Count a stage's output
against the pack's own files of that kind (a few lines of pak directory reading) before calling it
whole; `asset_name` now ignores letter case and keeps the pack's spelling in the name.

The cache keeps that spelling too (`sounds/weapons/Rocklr1b.wav`), while the game asks by the name
the map or the protocol uses, in any case: a reader of the cache matches each part of the name
ignoring case (`cached_sound`), as the game's own search path does, or it misses on Linux and
works on a Mac.
