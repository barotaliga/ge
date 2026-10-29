---
type: Added
title: Doors, plats, buttons, trains and triggers in a room level
---
`rooms::build_movers` reads a room level's `func_door`, `func_plat`, `func_button` and `func_train` groups and its `trigger_multiple` and `trigger_once` volumes, and `rooms::step_movers` steps them as a pure function. Doors open on a touch or when their name is fired, touching doors move as one, and a closing door that meets a body goes back open. Plats rise under a rider and carry it. Buttons fire their target, and trains run their path corners. `rooms::movers_world` is what a body meets with every part where it has moved to, and `rooms::clip_world` can now leave the groups out (`clip_group` builds one alone). The cloner writes a mover's `speed`, `lip` and `height` in metres. The vault sample gains a trigger that opens its door, and the `rooms` lab pictures it.
