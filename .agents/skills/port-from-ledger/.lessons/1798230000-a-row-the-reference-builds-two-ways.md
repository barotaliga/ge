---
title: A row the reference builds two ways: ask which build, and decide the one we show
date: 2026-11-23
scope: porting/shooters-edge, games/shooters_edge/menu, games/shooters_edge/client
concepts: [ledger, quit-box, build-variant, default, gl-flashblend, decision, reader]
---

Quake's quit box shows a random joke on some builds and the program's credits on others, and its
GL client draws `gl_flashblend` glow discs by default where the software client lit the walls. A
row's one line ("a box with one of the joke lines") hid both. When a reader says the behaviour
depends on the build or the renderer, pick the one this engine shows, write the choice in
`docs/porting/<system>.md` § Brought and left behind in the same PR, and name it in the row's
note, so the next reader of the ledger does not reopen it. A box naming another program's authors
and version is never ours to show.
