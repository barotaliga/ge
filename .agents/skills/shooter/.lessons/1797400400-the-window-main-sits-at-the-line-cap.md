---
title: The window's main.cpp sits at the line cap, so new wiring goes in a file of its own
date: 2026-11-14
scope: games/shooters_edge/client/main.cpp
concepts: [line-cap, window, install-ask, game-number, agents-check, exe-only-file]
---

`games/shooters_edge/client/main.cpp` is at 2000 lines, the hard cap, and its `ge:` tags sit near
the line the index reads up to, so three more header lines made agents-check report both the cap
and a missing `ge:skill`. A feature the window needs (`-classic <id>` asking for an install) goes in its
own file built into the `se` executable alone (`client/install_ask.cpp`, listed in
`add_executable` and in the client library's EXCLUDE regex, since it calls the platform's folder
chooser), with main.cpp's own change kept to a call or two and the same number of lines.
