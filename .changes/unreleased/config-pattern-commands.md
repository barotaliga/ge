---
type: Added
title: Config commands over many settings at once
---

The console gains the commands players' configs lean on: `cvarlist_re`, `cmdlist_re`, `cvar_reset_re`, `toggle_re`, `unset_re` and `unalias_re` act on every name a pattern matches; `unset <name>` removes a setting `set` made. Shooter's Edge adds `set_ex` and `set_ex2` (a value with the team codes filled in), `set_tp` (teamplay settings only) and `macrolist`, and reads the first game's `cvar_reset` and `viewalias` as `reset` and `alias`.
