---
type: Added
title: The synth's muffle, the lag graph, and Quake II's field of view and 2D settings
---

The synth gains a muffle (`Synth::muffle`: a high shelf over the whole mix) and the HUD kit a lag
graph (`draw_lag_graph`) beside the net graph. Shooter's Edge draws the field of view a Quake II
server sends (a mod's zoom, a server's fixed field of view, a recorded zoom), with `cl_adjustfov`
and `uf`; adds `scr_scale` (and picks the 2D scale by the window as Quake II does), `scr_draw2d`,
`scr_alpha`, the lag graph and its lost-connection picture (`scr_lag_*`), `cl_kickangles`, and
`s_underwater` with `s_underwater_gain_hf`, which muffle the world under water (on in Quake II,
off in Quake).
