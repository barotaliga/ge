# Goals and roadmap

This is the long-term spec every task in this repository serves. Read it before you
start a task. When the task is done, update the milestone's **Status** column in the
same PR, then name the next open row as the next step.

## The goal

gameengine2 is a C++ game engine that is fast, deterministic to the bit, and built for
agents. Its first game, **Fall Line** (a downhill skiing game), lives in the tree as the
engine's demo game. The engine is not a ski
engine: what is generic is in `engine/`, what a genre or a world shares (racing, a generated ski
mountain, a skier on snow) is in plugins any game of that kind links, and only what makes Fall
Line Fall Line is in `games/fall_line/` (G8).

### High-level goals

1. **G1: Deterministic on its own terms.** The simulation produces the same bits for the
   same seed and inputs on every compiler and OS: the same `level_digest`, the same `sim`
   digests, the same ghost replays, held by the golden tables in `tests/golden/`. Being
   close across platforms is not good enough, because the pinned maps, the ghosts and the
   record book all depend on exact bits. Matching any other implementation's bits is not a
   goal: the goldens are the engine's own, and a deliberate change rewrites them.
2. **G2: Fast.** Every component does one job and does it quickly. A level generates
   in well under a second. A simulation step costs a fraction of a 120 Hz budget. A
   frame hits the display's refresh rate on the reference machines with the picture
   at its top. Every speed-up claim comes with a before-and-after measurement
   (`perf` skill).
3. **G3: Built for agents.** Only agents write code here. The code and the docs are laid
   out so an agent finds the one place an answer lives and can change it safely: one
   subject per file, flat data, explicit names, every rule stated once, and a skill for
   every subsystem (`AGENTS.md`).
4. **G4: Only what the game needs.** Each component traces back to something Fall Line
   does. Nothing is built for a hypothetical game. Once Fall Line is on the engine, the second game,
   **Shooter's Edge** (`docs/shooters-edge.md`, M18), justifies a component the same way. A general engine feature earns
   its place when the game needs it. G8 decides WHERE a component lives, never WHETHER it is
   built.
5. **G5: Native on desktop and mobile.** The engine runs as a native app on macOS, Linux
   and Windows, and in the stores on iOS, iPadOS, the Mac App Store and Android, from
   one codebase. There is no web build.
6. **G6: Consumed by agents.** Downstream game repos that use the engine are also
   built by agents, so the engine is a product for agents too. Every error, assert
   and log line says what went wrong, where, and the next action to take, so it reads
   as a prompt rather than a code. Every task a consumer has (start a game, add a
   system, add a draw pass, add a sound, pin a seed, cut a release) has a skill.
   A `ge` CLI scaffolds a consumer repo: the layout, AGENTS.md, and the engine's
   consumer skills copied into `.agents/skills/`.
7. **G7: Always green.** CI builds and tests every push on every compiler and OS row.
   PRs merge when they are green. A golden failure is fixed, never waived.
8. **G8: Genre-agnostic engine, reusable plugins.** Code is placed in one of three homes:
   `engine/` (any game of any genre would use it), `plugins/<name>/` (any game of one genre
   or world would use it unchanged) or `games/<game>/` (only this game). Dependencies run
   game → plugin → engine, plugins depend only on plugins they declare, and nothing is
   registered at run time. The contract is `docs/architecture.md` § Plugins, the procedure
   the `plugins` skill, and `make agents-check` holds it.

9. **G9: Measured and looked at.** Every subsystem, plugin and game has labs: sim and
   scenario runs, captioned screenshots, bot and agent drives, cost benches, all on one
   engine-level contract (`docs/labs.md`) that runs headless on a GPU-less Linux machine. An
   agent tunes by running a lab before and after, not by guessing, and every lab doubles as a
   determinism test through its digest.

### Non-goals

- A general-purpose editor, a scripting language, or plugins loaded at run time (shared
  libraries, scripts). Plugins are compile-time modules linked by the game (G8). The one exception is
  Shooter's Edge's QC VM (`plugins/progs`, M18c), which executes existing mods' bytecode for
  compatibility, and the game's own QC (its baseline deathmatch mod and its test mod, compiled
  by the tree's QC compiler, M18d); the engine and its games stay C++.
- Any physics, renderer or audio middleware the game does not need. The skier's physics
  is the game's own.
- Music. Fall Line has none by design: its sound is its effects.
- Changes to the reference sources used while porting. They are read-only from here.
- A web build. The engine is native only.

## Milestones

Each row is one or more PRs, and each row is ONE FILE, `docs/goals/<row>.md`, so pull requests
that add or update different rows never touch the same file. The file opens with a header of
`Key: value` lines up to its first blank line, then what is built and measured so far:

```
Status: in progress
Milestone: **The title**: what the row builds, and where (`engine/<module>`)
Exit: what must hold for the row to be done
Labs: the labs it lands, in `make labs-smoke`

Notes: what is built and measured so far.
```

**Status** is one of `done`, `in progress` or `open`. Other header keys are allowed and kept.
Nothing else lists the rows: `make index` (and `make agents-check`) write the whole table, in
roadmap order, to `docs/roadmap.md`, which is generated in every checkout and never committed,
as `docs/index.md` is. `tools/check-goals.mjs` holds every file's header. Rows sort by id,
numbers as numbers (`M8f9` before `M8f11`, `M13e` before `M14`), so a new row needs no edit
anywhere but its own file. The `ge` wishes are [`goals/ge-backlog.md`](goals/ge-backlog.md),
one a line. A row is `done` when its exit criterion holds AND the labs in its **Labs** line run
in `make labs-smoke` (`docs/labs.md`). A subsystem an agent cannot measure and look at is not
finished.

## Where each milestone's code goes (G8)

Classify each file with the `plugins` skill when its row starts; this is the expected split.

| Rows | engine/ | plugins/ | games/fall_line/ |
| --- | --- | --- | --- |
| M4b–M4d | | `mountain` (the ski area, the routes, the real faces) | which seeds and maps the game races (the pinned lists) |
| M5 | | `skier` (the skier on snow: the legs, the snow, the edge, the poles, flight, the crash and the body); `racing` (the field's standings) | `create_game`, the order of systems in `step`, the modes and their rules, the course as Fall Line runs it, the bot, the `sim` CLI |
| M6, M11 | `platform`, `gpu`, the mobile and store builds | | |
| M7–M8 | `render`: the pass table, GPU-driven instance families, the heightfield clipmap, shadows, sky and clouds, TAA-U, post, compute particles, skinning (`docs/rendering.md`) | `mountain/app` (the snow shader, the trail map and grooves, the forest's look), `skier/app` (the figure, the spray) | Fall Line's own look and tuning of those passes |
| M9 | the synth's DSP graph, racks, `play_def`, the meter; the audio device | `skier/app`: the wind and the skis on the snow, as beds | done (M9a and M9b) |
| M10 | the 2D layer, text, the dev UI; `hud`: the HUD kit (faces, anchors, readouts, bars, pips, banners, the feed, the layout check) | `racing` (the ghost and record stores) | the HUD (`games/fall_line/hud`: the snapshot, the news, the words, the view), the cards, settings, every word in `strings` |
| M12 | the `ge` CLI, its templates, consumer skills | each plugin's skill, copied when a consumer links it | the play contract's Fall Line half (M12b) |
| M15 | | | the wanted level and the army (`sim/wanted`, `sim/army`, then the army's machines), their labs |
| M16 | `ai`: the decision record, the humanness metrics and the hands, paths, thinking on a schedule, sight and memory, steering, choosing; `play`'s `ge why` | `skier`: the skier's pilot (M16c) | its bots' decisions, profiles and bands; the `why` and `humanness` labs' runs |
| M25 | `save`: the file, the `io` description, the run's save and load on `play`, the store and its slots, pictures, the content cache | each plugin's state types described (`skier`, `snowmobile`, `helicopter`, `paramotor`) | its chunks and versions, its character, when it autosaves, its save and load cards |
| M19 | `input`: devices, events, the state, shapes, actions and bindings, rebinding, the device database, touch, players, feedback, the event log, simulated devices; `platform`'s backend; `play/devices` | | Fall Line's actions, default bindings and what is felt |
| M29 | `snow` (the layer, the query, `press`, the contact laws, time, the tiers, the snapshot); `render`'s snow mirror, light model, marks, displaced patch and sheets; `audio`'s snow voices | `skier`, `snowmobile`: their contacts through the laws; `mountain`: a mountain's cover and its corduroy | which cover a map gets, the free ride's day, the dial |
| M17 | `net`: links, channels, nodes, the simulated network, captures, sessions, NAT, replication, lockstep, voice, accounts and the service | a genre's shared messages, when one is shared | what it replicates, its messages, its lobby's settings (`net/`) |
| M21 | `console`: cvars, the language, binds, cfg files, the drop-down console's model; `play`'s `ge cvar`, `ge cfg` | | its cvars (`cfg/`), its `default.cfg`, the console drawn in its app |
| M14 | the log channels (`core/output`), the series files, the outlier rule and the chart kit (`lab/`), `ge graph`, the pose track and its views | `skier`: the figure's joints for the pose track | its log channels, the `trace` lab, the `anim` lab's scenes |
| M48 | `viewmodel`: the held model, its place and its light, the CPU reference; `render`: the view-model pass (M48c) | `classicenhance`: the pack's weapons (M48b) | Shooter's Edge: its weapons' mounts, its row and cvars (M48d) |
| M22 | `ui`: paints, the draw list, images, fonts, the CPU raster, backdrops, menus (model and view); `render`: the 2D pass (M22b) | | its front door, options and pause pages, their words and sounds |
| M23 | `replay` (keyframes, the seek, the transport, the file, the director, spectating); `capture` (frames and sound, muxers, encoders, the stream); a Discord backend in `services` | | its clone, its moments and their weights, its replay lenses, where the transport is drawn |
| M32 | | `rooms` (the engine's own level format and what grows from it); `brush` (the classic BSP families, with M18b) | none: Fall Line is a mountain; Shooter's Edge's maps come in both |
| M28 | `move`: the knobs and presets, the step over a trace, the box world, the strafer, the scenarios | `shooter`: the classic row and the brush trace (M18e) | none yet: Fall Line's people on foot follow routes |
| M31 | `texgen`: the recipe, tileable noise, the layer kinds, the bake and its cache, the CPU preview, `ge texture` | none yet: a genre's recipes ship with its plugin when one is shared | its recipes beside its levels (M31c) |
| M18 | `vfs`, `console`; sampled voices in `audio`, captured mouse and text input in `platform`, the world, morph and sprite passes in `render`; the play contract's first-person growth | `brush` (the map format), `shooter` (first-person movement, hitscan, prediction), `progs` (the QC VM) | `games/shooters_edge/`: the server, the client, the protocols, demos, the HUD, its labs (`docs/shooters-edge.md`) |
| M20 | **Platform services** (`engine/services`, `docs/services.md`): what a store's client gives a game (who is playing, achievements, stats, leaderboards, cloud saves, rich presence, friends and invites, the overlay, lobbies and the store's network, the workshop, the controller layer, ownership and DLC, screenshots, the handheld) behind one interface, Steam first, and Discord's presence beside it, in the rows below; everything built and tested without any store's SDK | M20a–M20h done; Fall Line runs on Steam and outside any store from one build of its code | `services`, then each row's | [in progress](goals/M20.md) |
| M24 | `wind` (the field, the profile, shelter, local sources); `water` (bodies, the query, the spectrum and waves, floating, swimming, the optics, the flowing grid); `render`'s water passes and the view under water; `audio`'s water voices | `shooter`: the classic swim preset; `skier`: the clothed skier's preset; `mountain`: a map's water and its wind as a source of the engine's | which water a map gets, lake ice, falling in, skimming, the bot near water |
| M30 | `water`: the liquid types, harm, the glow, the named liquids; `render`'s liquid pass and looks | `shooter`: the classic liquids' ticks and swim (M30g) | none yet: no liquid but water on the mountain |
| M35 | `render`: the post pass's lens and film, the lens pass, the grade's knobs | | its defaults, its `r_` cvars, the ladder's stop |
| M26 | `camera`: the pose, the followers, the rigs, the modifiers, the transitions, the stack, its cvars | | its ladder and rows, its director, its cvars' defaults and world unit (Fall Line's in M26c, M26d, M26f; Shooter's Edge's in M26e) |
| M33 | `prof`: the timeline, the OS sampler, the load analysis; `core`'s thread names and the pool's listener; `lab`'s load charts; `play`'s load run and `ge load` | | its threads named and its frame loop marked; the `gpu-load` lab |
| M27 | `charset`: the fonts' atlases and their builder, the noise layers, the looks' CPU reference, text layout; `render`'s text pass (M27b) | | which fonts and looks its HUD, menus and console use |
| M42 | `detail`: solids and bevels, receivers and decals, props and their shapes; `render`'s decal pass and the props as instances (M42e) | `rooms`, `brush`: their brushes and levels handed to the detail passes (M42f); `classic-enhance`: the pak0 maps dressed | its rules (what a prop stands on, which events leave which marks: a shot, a wound, a blast) |
| M50 | `render`: the beam's channel, level, lights and burns, its pass | | the weapon that fires it, the preset it picks (Shooter's Edge, M50b) |
| M38 | `render`: the fog in areas (`fog.hpp`), the scene's reflection helper, the glow's lamps | | its mists dealt by the map's weather, its cvars (M38f) |
| M34 | `grass` (kinds, covers, the scatter, the sway); `render`'s grass passes | `rooms`, `brush`: a material's grass grown on its floors (M34d) | its summer meadows and their kinds (M34e) |
| M40 | `bake`: charts, the BVH, the bake, the directional shade | `brush`: a map's faces as a bake's scene, the cache's codec, the rebaked lighting in its renderer; `classic-enhance`: the pack's `lightmaps` stage | none: a game turns it on (`r_bsplight`) |

### Open for extension

Other games of the same shape (a rally game, a jet-ski racer, a snowmobile racer) may run on the
engine later. None is built for now (G4), but the boundaries above are drawn so they would fit:
the engine never learns a ski word, `racing` serves any racer, `mountain` never depends on the
rider (a snowmobile game generates the same mountain), the snow's surface model is expected to
be its own plugin under `skier` rather than a part of it, and a new world (roads, water) is a new
plugin beside `mountain`, which is why world generation is not in the engine.

## How a task uses this page

1. Find the row the task belongs to (`make index`, then `docs/roadmap.md`). If no row fits, the task is probably out of scope
   (G4), so ask before you build it. Decide each file's home (engine, plugin or game) with
   the `plugins` skill before the first edit.
2. Do the work under that row's skill (`AGENTS.md` routes it). Run the labs of the subject
   you change before and after (the `labs` skill), and land the row's labs with it.
3. In the same PR, update the row's file, `docs/goals/<row>.md`. If the work showed that a
   row needs splitting, split it: a new row is a new file (`ge goals claim`).
4. Finish by naming the next open row (or the next unfinished part of this row) as the
   next step.
