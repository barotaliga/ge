---
name: plugins
description: "Use before writing or moving code in gameengine2 to decide where it lives (engine/, plugins/<name>/ or games/<game>/), and to add a plugin, move code between the three, or change a plugin's dependencies. The classification test, the plugin layout, ge_add_plugin, the checks, and what moving code owes."
---

# Engine, plugin or game

The engine is not a ski engine. Generic code lives in `engine/`, reusable code of a genre or a
world (racing, a generated ski mountain, a skier on snow) lives in a **plugin** that any game of
that kind can link, and only what makes Fall Line Fall Line lives in `games/fall_line/`. The
contract is stated once in [`docs/architecture.md`](../../../docs/architecture.md) § Plugins;
this skill is the procedure.

## Classify before the first edit

Ask, in order, of the file (or the part of a file) you are about to write:

1. **Would a game of a different genre use it unchanged?** (a seeded stream, a heightfield, the
   run clock, an SDL window, a draw pass, the synth, the 2D layer) → `engine/<module>/`.
2. **Would another game of the same genre or world use it unchanged?** (the control tape and
   the standings for any racer; the mountain generator for any game on a generated ski
   mountain; the skier's physics for any skiing game) → `plugins/<name>/`.
3. **Otherwise** (the modes and their rules, the order of systems in the step, the field and the
   bot that plays this game, pinned maps, every word the player reads, the HUD, the cards, the
   game's own look) → `games/<game>/`.

When one file would mix two homes, split it at the seam and say so in both files' headers.
When in doubt between plugin and game, choose the game: moving code down into a plugin later is
a small PR, and G4 (only what the demo game needs) still holds inside a plugin. No option, table or hook
is added "so another game could use it" until one does.

Words: a plugin may use its world's words (a ski plugin says skier, piste, gate). It never uses
one game's: no game's name, mode, string or pinned seed. An engine module uses neither, only the
mechanism's (vehicle, checkpoint, track).

## Add a plugin

1. Pick the name: one lower-case word (`racing`, `mountain`, `skier`). It is the directory, the
   namespace `ge::<name>`, the target `ge_<name>`, the skill and the test prefix at once. It must
   not equal an `engine/` or `games/` directory.
2. `plugins/<name>/CMakeLists.txt` is one call naming every dependency:
   `ge_add_plugin(<name> ENGINE core PLUGINS racing)`. `cmake/Plugins.cmake` defines it; the root
   build finds the directory by itself.
3. Sources go in `plugins/<name>/`, or in `plugins/<name>/<module>/` when the plugin has more
   than one module (`mountain/mapgen/`, `mountain/analysis/`), and include each other as
   `"<name>/<file>.hpp"` or `"<name>/<module>/<file>.hpp"`.
   The simulation half is held by `make libm-check` like the engine's core.
4. The skill: `.agents/skills/<name>/SKILL.md` and `.lessons/.gitkeep`, routed from AGENTS.md's
   Skills table and listed in `.agents/skills/README.md` under Plugins. Every file in the plugin
   names `<name>` first in its `ge:skill` line.
5. Tests: `tests/<name>_<topic>_test.cpp`, goldens `tests/golden/<name>_<topic>.txt`. Every test
   links every plugin; nothing to wire.
6. Labs: `plugins/<name>/labs/` (not compiled into `ge_<name>`), registered through the lab
   contract (`docs/architecture.md` § Plugins).
7. A presentation half (draw passes, sound beds) goes in `plugins/<name>/app/`. The build refuses
   sources there until the first one splits it into its own `ge_<name>_app` target, so the
   simulation target never links a renderer.
8. Add the plugin's row to AGENTS.md § Where things live, run `make index` and `make agents-check`.

## Move code between homes

1. `git mv` the files (so history follows), then rewrite includes and namespaces with one `sed`
   over the moved files and their tests.
2. Retag: `ge:skill` names the new owner; test and golden files take the new prefix
   (`tests/golden/<name>_*.txt`, and the `porting/parity/` script that writes them).
3. `make test`, `make agents-check`, `make libm-check`. A move changes no bits: every golden
   passes unchanged.
4. Update AGENTS.md's table, `porting/README.md`'s layout map when a whole directory moved,
   and any skill or lesson `scope:` that names the old path.
5. Tell every open thread that edits the moved files where they went (the old path, the new
   path, the namespace sed), because their branch will not rebase cleanly by itself.

## Change a plugin's dependencies

Add the engine target or plugin to its `ge_add_plugin` call. `tools/check-plugins.mjs` fails an
`#include` the call does not declare, a cycle between plugins, and any engine file that includes
a plugin. A cycle means the shared part is its own plugin, or belongs in the engine.

## Registration is the build, not the run

There is no run-time plugin registry and no static-initializer self-registration: linkers drop
unreferenced objects from static libraries, the order across translation units is unspecified,
and both break determinism. A plugin exposes plain functions and data; the game links its target
and calls them from its own `create_game` and `step`, in one order, stated once there.
