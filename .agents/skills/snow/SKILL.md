---
name: snow
description: "Use for engine/snow in gameengine2: a layer of snow over any ground that remembers what crossed it. The column model (depth and mass over a ground), the cover, the sparse fixed-point tiles, sample, press and the imprint record, the reference contacts, the snapshot and `ge snow`; snow over time (weather schedules, settling, filling, drift, melt and crust, grooming); the level of detail (focus actors, the coarse layer, folding and refining, the tile budget); the snow-press, snow-field, snow-contact, snow-weather, snow-lod and snow-cost labs; and the M29 plan in docs/snow.md (contact laws, time, tiers, drawing, sound, Fall Line's move onto it)."
---

# Snow

`engine/snow/` (`ge_snow`, namespace `ge::snow`) is snow as an engine feature: any game lays a
layer over any ground and presses whatever crosses it into the snow. `docs/snow.md` is the design
and the plan (rows M29a–M29l); this skill is how to work on it.

| File | What |
| --- | --- |
| `snow/kinds.hpp` | The named snows, their loose, packed and cap densities, the packed share |
| `snow/layer.hpp/.cpp` | The layer: region, ground, cover, tiles of fixed-point cells, `sample`, `cell_for`, `mass_units` |
| `snow/contact.hpp/.cpp` | The contact laws: a `ContactRow` a kind, Bekker's sink and its cap (`column_sink`), compaction and bulldozing, Mohr–Coulomb and Janosi traction, glide and tyre friction, planing |
| `snow/press.hpp/.cpp` | `press`: targets from a sink or a pressure, mass kept, the berm, the imprint record; with a weather, cells brought to the minute first and a crust broken or borne |
| `snow/time.hpp/.cpp` | Snow over time: `Schedule`, `Weather`, `spell`, `weathered`, `filled`, `Moment`, `bring_to`, `crust_load`, `sample(layer, x, z, t)`, `groom` |
| `snow/lod.hpp/.cpp` | The level of detail: `set_focus`, `far_from_focus`, `fold_stale`, `fold_tile`, `refine_tile`, `stalest_tile`, `press_coarse`, `bytes_of`, `live_tiles` |
| `snow/reference.hpp` | The reference contacts (a carving and a skidding ski, a boot, a tyre, a tread, a body), `place` |
| `snow/snapshot.hpp/.cpp` | `save`, `load`, `read_info`, `load_bare` (the format in the header) |
| `engine/render/wgsl/snow_light.wgsl` | The snow light model for any material: `snow_relief`, `snow_light` (`SnowPoint`, `SnowGains`), and the `snow_` noise, relief and glint blocks |
| `engine/render/snow_mirror.hpp/.cpp` | The layer mirrored on the GPU: `create_snow_mirror`, `update_snow_mirror` (the fine window's atlas, the coarse view, only stamped tiles and blocks re-sent), `snow_mirror_expect`, `read_snow_raw`, `read_snow_columns` |
| `engine/render/wgsl/snow_mirror.wgsl` | The mirror's bindings (group 3, 0 to 4), `snow_raw` (a sample to the bit) and `snow_column` (`snow::sample` on the GPU) |
| `engine/cli/snow.hpp/.cpp` | `ge snow show` and `ge snow column` over a saved layer |

## Rules

- **Mass to the unit.** A cell keeps its mass in units of 0.05 kg/m2 and every press moves whole
  units: what the tiles hold plus `spilled` is always `mass_in`. `snow-press` and
  `tests/snow_press_test.cpp` hold it; a change that breaks it is a bug, never a tolerance.
- **An untouched layer is its cover, bit for bit.** `sample` reads the cover itself where no
  sample round the point was pressed. That is what lets a game (Fall Line, M29j) move its
  physics onto a layer without moving a digest. Never return a quantized cell for an untouched
  sample.
- **Fixed point in the tiles, doubles between.** Cells are integers (the same bits everywhere,
  uploaded to the GPU as they are); the arithmetic is doubles with `core/libm` only
  (`make libm-check` scans `engine/snow`).
- **The game's order is the snow's order.** Presses change state; a game presses in its step
  order. Nothing in `engine/snow` reads a clock, a camera or `Math`-style randomness.
- **The deeper of two passes.** Targets are measured from the untouched cover, so pressing the
  same imprint twice changes nothing; keep it so (the stationary contact must not dig forever).
- **A narrow outline reaches a sample.** Outlines are read at least three quarters of a sample
  wide and berms reach a sample and a half out; the true width is the picture's (M29e's marks).
- **Golden**: `tests/golden/snow_press.txt` (`GE_GOLDEN_WRITE=1 build/<preset>/tests/snow_press_test`
  rewrites it). A move is a behaviour change: say why in the PR with the `snow-press` table
  before and after.
- **Labs**: `snow-press` (every contact on three snows: cross-sections, plans, the table),
  `snow-contact` (the laws charted for every kind: sink, traction, glide, drag) and
  `snow-field` (a patch crossed by everything: relief, density, tiles, the press's cost). Run
  them with `--tag=before` and `--compare=before` around any change here.
- **A row is a fit, not a fact.** Settled snow's contact row is the published set; the rest are
  fitted to one stated sink each (its comment). Moving a row moves `tests/golden/snow_contact.txt`
  and the press golden: say what it was fitted to and put the `snow-contact` table before and
  after in the PR.
- **Time is read, not stepped.** A `Weather` is schedules; what it does between two moments is
  worked out from their integrals (`spell`). Never add a per-step update to the layer: a cell
  carries the minute it last changed and is weathered from there on read. Every write to a cell
  of a weathered layer goes through `bring_to` first, which counts the weather's mass into
  `mass_in`; a write that skips it breaks the ledger (`snow-weather` and
  `tests/snow_time_test.cpp` catch it). A layer with no weather must stay the static layer bit
  for bit.
- **Labs for time**: `snow-weather` (storms filling a groove, settling, drift, crust, a groomer).
  A default in `Weather` moved is a behaviour change: put its table before and after in the PR.
- **Coarse cells hold sums.** A coarse cell keeps the SUM of its 16×16 samples' mass units and
  depths, never a mean, so folding and refining are integer arithmetic and the ledger holds to
  the unit. Read a column through `stored_at` (fine cell, else touched coarse cell, else the
  cover); a reader that goes to `cell_at` alone misses the coarse layer.
- **The focus is the game's actors, never the camera.** Tiles near a focus are stamped at
  `set_focus`; the fold order (stalest first, ties by slot) is the same on every machine. A tile
  the operation in progress wrote is never folded by it (`near_t < layer.clock`).
- **Labs for the level of detail**: `snow-lod` (a long run under a small budget: the strip, the
  memory, close-ups) and `snow-cost` (a call's cost, measured; quote the release build).
- **The light model is shared.** `render/wgsl/snow_light.wgsl` is what snow does to the light;
  a material (the mountain's, later a sheet's or a roof's) keeps only its own surface and
  additions. A change there moves every snow picture: run `world --tag=before` and
  `--compare=before` around it, and put the diff table in the PR.
- **Every write stamps what it changed.** A write to a fine cell goes through `cell_for`, which
  stamps the tile; a write to a coarse block through `block_for`, which stamps the block. The GPU
  mirror re-sends only what a stamp moved, so a new write path that skips them leaves the picture
  stale. `tests/render_snow_mirror_test.cpp` holds the uploads; `gpu-snow` holds the GPU to the
  layer to the bit (run it before and after any change to the mirror or the cell layout).
- **What is next** is `docs/snow.md`'s rows: the rest of M29e (the fine marks from the imprint
  records, the mountain's material reading the mirror).
