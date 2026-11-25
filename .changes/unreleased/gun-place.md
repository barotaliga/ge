---
type: Added
title: Each game's own settings for the held weapon's place, size and lens
---

The first classic game's `r_viewmodelsize` and `r_viewmodeloffset`, and Quake II's `cl_gun_x`, `cl_gun_y`, `cl_gun_z` and `cl_gunfov`, move and size the held weapon in each game's own units; an offset adds to `cl_gunx`, `cl_guny` and `cl_gunz`, and `r_viewmodel_fov` wins over `cl_gunfov` when set.
