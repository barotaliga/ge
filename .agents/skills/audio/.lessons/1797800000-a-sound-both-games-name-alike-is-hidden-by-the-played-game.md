---
title: A sound both games name alike is hidden by the played game's copy; test through the hearing
date: 2026-11-26
scope: games/shooters_edge/client/demo_sound.cpp, games/shooters_edge/profiles/remix_sounds.hpp, engine/vfs/search.hpp, tests/shooters_edge_remix_heard_test.cpp
concepts: [remix, cr-sounds, heard-as, search-path, shared-names, find-below, footsteps, listen-hearing]
---

Classic remix mounts the other game's files under the played game's, so a sound both games ship
by one name (`player/gasp1.wav`, `items/protect.wav`, `misc/talk.wav`) always read the played
game's copy, whatever `cr_sounds` said: a rename table cannot fix a name that does not change.
Hearing "as the other game" reads that game's own copy below the winner (`vfs::find_below` over
the other game's first sources, `DemoSounds::hear_as`'s `theirs`). And a report that one sound
"is not swapped" may be true of only one way of hearing: the window's own game read the jump right
while a joined game and a demo heard nothing swapped at all, because only the listen's hearing was
told the game. Test a sound part through each hearing the window runs (`ListenHearing` and
`DemoHearing`), with a real server starting the sound, not by the table's lookup alone. A sound one
game makes on the server and the other never does (Quake II's footsteps) is the server's to make,
so `cr_sounds` reaching the server (a rules line, `g_footsteps 1`) is what adds it.
