---
type: Added
title: Classic remix lets the players move as the other game's do (cr_moves)
---

With classic remix on, `cr_moves` chooses whose movement the players run, jump and strafe with from the next map: 1 the classic game's, 2 the sequel's, 0 each game's own (the default). The classic game moving as the sequel runs at 300, stops sooner, steers in the air the sequel's way and pauses a moment after a hard landing, so hops no longer chain; the sequel moving as the classic game runs at 320, slides further and chains strafe-jumps on every landing.

New server settings carry it on the classic rules: `pm_airaccel`, `pm_airwish` and `pm_landpause`. The sequel's server takes `sv_moves classic`.
