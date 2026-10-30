---
name: brush
description: "Use for plugins/brush in gameengine2: maps in the classic BSP families (version 29, BSP2, 38, 46) read into one view (surfaces, lightmaps, the tree, clusters, visibility, entities, patches), refusing damage; the fixture writers that make test maps of each family; the content scan and asset map (`ge content scan`); the bsp and content-scan labs; the brush fuzz and read tests."
---

# The brush plugin

`plugins/brush/` (`ge_brush`, namespace `ge::brush`) reads maps of the classic BSP families into
one view. It depends on the engine's core only. The design is
[`docs/levels.md`](../../../docs/levels.md) § The classic BSP families; each file's header states
its part once. The byte layouts, with where each fact came from, are in the porting area's
format notes (the clean-room rules there say what may be written where).

| File | What |
| --- | --- |
| `map.hpp/.cpp` | The one view, `leaf_at`, `cluster_sees`, `to_engine`, `map_digest` |
| `read.hpp/.cpp`, `read_parts.hpp` | The front door (family by the first bytes), lumps, refusals, rle rows, polygons, extents, tree checks, entities, `.lit` |
| `read_v29.cpp`, `read_v38.cpp`, `read_v46.cpp` | One reader a family (29 also reads BSP2) |
| `bytes.hpp` | The little-endian cursor and writer |
| `write.hpp`, `write_parts.hpp`, `fixture.cpp` | Fixtures: the sample, the weather's (a hall, a yard, a store), the compile every writer shares |
| `write_v29.cpp`, `write_v38.cpp`, `write_v46.cpp` | One writer a family |
| `content.hpp/.cpp` | The content scan (what a set ships, what its maps' entities name, the numbered sets they pick), the asset map's parse and the match, the fixture set |
| `labs/content_scan_lab.cpp` | The `content-scan` lab behind `ge content scan` (skill: free-assets) |
| `labs/bsp_lab.cpp` | The `bsp` lab (`--map`, `--lit`, `--pak`, `--textures-out`: a map's textures as PNGs for the matcher) |
| `labs/textured_view.hpp/.cpp` | The lab's textured software view of a 29 or BSP2 map: its own pictures through a palette, its lightmaps, sky layers |
| `app/bsp_weather.hpp/.cpp`, `labs/gpu_bsp_weather_lab.cpp` | The weather on a map by cvars (`sv_weather` …), its shelter along the fall in `bsp_draw` and `bsp.wgsl`'s `weathered`; the `gpu-bsp-weather` lab |

## Reading a new field or lump

1. Run the lab first: `make lab LAB=bsp ARGS=--tag=before`.
2. Read it in its family's reader through a `detail::Cursor` over the lump, after `count_of` has
   checked the lump's size; check every index it keeps against what it points into and refuse
   with `detail::refuse` (what, where, `next:`). Nothing in the view may point out of range.
3. If the view grows, add the field to `map.hpp` (and `map_digest`), and teach the fixture writers
   to write it so a test can read it back.
4. `make lab LAB=bsp ARGS=--compare=before`; rewrite the golden
   (`GE_GOLDEN_WRITE=1 build/<preset>/tests/brush_read_test`) and the smoke digest
   (`build/<preset>/labs bsp --smoke --write-digests=tests/labs_digests.txt`), and say in the PR
   why the bits moved.

## Rules

- **Refused, never overrun.** A file is anyone's: every lump bounds-checked before it is read,
  every index checked before it is kept, every size bounded before it allocates. The fuzz test
  (`tests/brush_fuzz_test.cpp`) runs under the sanitizers on CI; a new field joins its `sound()`.
- **No one else's maps in the tree.** Tests and labs use the fixture writers; a real map is looked
  at with `--map=<file>` from outside the repository and never committed.
- **One view.** A family's difference is resolved in its reader (winding, luxel format, cluster
  numbering), never left for a caller to branch on.
- **Clean room.** Code, comments and docs say "version 29", "BSP2", "version 38", "version 46" and
  describe the format in our own words; no game's name and no other implementation's code.

## The weather on a map

- **The open sky decides.** Nothing about where it rains is authored: the renderer's third ortho
  map looks along the fall (sky faces never block it) and everything wet, pooled, snowed on or
  splashed reads it. A probe in `gpu-bsp-weather` that changes verdict means the fixture or the
  fall's way moved; read the probes table before the pictures.
- **Geometry is honest, not tidy.** At the steepest slant (75 degrees) a fall slants ten metres
  into a hall through a window and on through a doorway in line with it; the weather fixture
  keeps its store's door off that line rather than clamping the physics.
