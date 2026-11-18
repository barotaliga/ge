---
type: Added
title: Overtime, the idle kick, prewar, team sizes, the join lock and the home map on every lobby
---

The engine's server gains a match's rules every game's lobby can use: overtime on a tie at the time limit (`sv_overtime`: extra time, sudden death, a tie-break or a golden frag; `sv_overtime_minutes`), the idle kick of players not ready once half are (`sv_idlekick`), what the warmup lets a player do (`sv_prewar`), each team's size before a start (`sv_teamsize_min`), a match closed to newcomers (`sv_lockjoin`), the home map an empty server goes back to (`sv_homemap`) and a cap on a voted time limit (`sv_timelimit_max`). It also gains a rate rule every game's endpoint calls (`sv_minrate` beside `sv_maxrate`, a client out of range told and set back) and a chat route (`sv_spectalk`, and spectators reading the team reports of the team they follow, `sv_sayteam_to_spec`); `sv_spectalk 0` now mutes spectators only during a match. Shooter's Edge's classic server uses both.
