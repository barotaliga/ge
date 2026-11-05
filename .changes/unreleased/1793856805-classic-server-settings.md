---
type: Added
title: Classic server settings on the dedicated server
---

Every game's server now drops a client it has not heard from for `sv_timeout` seconds (65) and holds chat to a flood rule, `sv_floodprot "N S T"` with `sv_floodprotmsg`. Any shooter offers the move's and the loop's settings by name with their ranges (`sv_mintic`, `sv_maxtic`, `sv_maxvelocity`, `sv_speedcheck`, the `sv_` move numbers and the `pm_` switches, told to clients when changed). Shooter's Edge's dedicated server adds `pausable`, `sv_maxrate`, `sv_antilag`, `sv_voip`, `maxfps`, `allow_download*` and `sv_downloadchunksperframe` as server settings.
