---
type: Added
title: Classic remix lets the players move as the other game's do (cr_physics)
---

With classic remix on, `cr_physics` chooses whose movement the players run, jump and strafe with from the next map: 1 Quake's, 2 Quake II's, 0 each game's own (the default). Quake moving as Quake II runs at 300, stops sooner, steers in the air Quake II's way and pauses a moment after a hard landing, so hops no longer chain; Quake II moving as Quake runs at 320, slides further and chains strafe-jumps on every landing.

New server settings carry it on the classic rules: `pm_airaccel`, `pm_airwish` and `pm_landpause`. Quake II's server takes `sv_physics q1`.
