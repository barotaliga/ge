---
name: classic
description: "Use for plugins/classic in gameengine2: what only an imported classic game needs on top of the shooter kit (plugins/shooter), kept apart so a shooter of our own links the kit alone. The content layer (content/: an install's search path over base, bundle and mod folders, packs, zips and loose files; each game's load order and startup configs; the open calls for maps, wall textures, models by kind, sounds, configs, place names and pictures, taking the game's content row as an argument) and the classic protocol family (protocol/: the net channel with its sequences, reliable resend, qport, rate and choke; info strings; the wire's sizes, quantization, fields and CRC-16); place names (locs: the .loc format, macros, the nearest place); the item tables' shapes and item roles with remix's swaps; the prediction on the original move; the sequel's view (bob, kicks, falls) and its own move (packed integer state over brushes); the classic look (app/look.hpp: r_look, the r_ and ce_ cvars). Load it before moving a piece of a classic game out of plugins/shooter (docs/goals/M60.md § M60j's survey says which pieces go)."
---

# classic

`plugins/classic` (`ge::classic`) is the layer between the shooter kit and each classic game's
module: everything a classic game needs that a shooter of our own would not. Dependencies run
one way: `classic` uses `shooter`, `brush`, `morph`, `progs` and `classicenhance`; `shooter`
never includes `classic`. Shooter's Edge links both; a new pure game links only `shooter`.

## What lives here

- `content/` (`ge::classic::content`): the search path, the load orders, the open calls.
- `locs`, `item_tables`, `item_roles`, `predict_original`, `sequel_view`, `sequel_move`
  (`ge::classic`): the old
  games' formats, tables and views. A header names the kit types it reads by using-declarations
  (`using ge::shooter::Vec3f;`); a `.cpp` may use the kit's namespace.
- `app/look.hpp` (`ge::classic::look`, `ge_classic_app`): the picture every classic game shares.
- `protocol/` (`ge::classic::protocol`): the protocol family every classic game's own protocol
  shares. A game's own protocol (its version, ports, usercmd, move checksum) stays in its module
  (`games/shooters_edge/net/classic_wire.hpp`).

## Moving a piece here

1. Check the survey (`docs/goals/M60.md` § M60j): is the piece classic-only, or would our own shooter
   want it? A mechanism a new game might choose (the classic move in floats, the movers) stays
   in `plugins/shooter`; only each old game's numbers and formats come here.
2. `git mv` the files, rename the namespace (`ge::shooter::x` to `ge::classic::x`), the includes
   (`"classic/..."`), the tests (`tests/classic_<file>_test.cpp`) and their goldens.
3. Qualify any `ge::shooter` name the moved code used unqualified.
4. Goldens and lab digests unchanged prove the move (`make precheck`).
