# Maps: the families, collision, movers, entities, place names

A classic game's levels: the map file read into one view, its collision for the move and the
shots, its movers (doors, plats, trains), its entity text, its place names, and the tools that
build, check and survey maps. The player meets it as every level they walk; remix meets it as
"one game's maps played by another's rules and move".

## Shared

| Layer | File | What it gives |
| --- | --- | --- |
| Reader | `plugins/brush/read.hpp`, `map.hpp`, `read_parts.hpp` | Every BSP family (29, BSP2, 38, 46) into ONE map view: surfaces, lightmaps, tree, clusters, PVS and PHS, entities, 46's patches tessellated; damage refused with what and where |
| Reader | `plugins/brush/pictures.hpp`, `wal.hpp`, `anim.hpp` | The map's own pictures, the 38 family's wall texture files, texture sequences |
| Reader | `plugins/brush/lights.hpp`, `light_probe.hpp`, `rebake.hpp` | Light entities, the baked light under a point, the lightmaps rebaked finer with bounce |
| Reader | `plugins/brush/survey.hpp`, `tool.hpp`, `content.hpp` | A map surveyed (leaf cells, liquid bodies, slices, entities near a point), `ge map` / `secli map`, the content scan |
| Reader | `plugins/brush/compile/*.hpp` | The map compiler: a plan, an SVG floor plan or `.map` text to a version 29 map with hulls and light |
| Reader | `plugins/brush/write.hpp` | Fixture maps of every family for tests |
| Shooter kit | `plugins/shooter/hull.hpp`, `brush_hull.hpp`, `bsp_world.hpp`, `cells.hpp` (brush) | Clip hulls (from the tree, or built from convex brushes), the version 29 cells as brushes |
| Shooter kit | `plugins/shooter/world.hpp`, `trace.hpp`, `hull_world.hpp`, `plugins/brush/trace.hpp` | ONE TRACE for both families: `MoveWorld`'s solids are hulls or `brushes`, so `player_trace`, `box_free`, contents and `point_trace` answer on any map |
| Shooter kit | `plugins/shooter/push.hpp`, `mover_motion.hpp` | Movers: doors, plats and trains on their own clock, the plain, accelerated and turning motion |
| Shooter kit | `plugins/shooter/map_spec.hpp` | A map as the rules start it: brush models with hulls, entity text |
| Progs plugin | `plugins/progs/entity_text.hpp` | The entity text: blocks of keys, a block's keys into fields |
| Classic plugin | `plugins/classic/locs.hpp` | Place names: the `.loc` format, macros, the nearest place |
| Classic plugin | `plugins/classic/content/map_versions.hpp` | A stock map in several versions (the first release's, a later rerelease's, any other a server plays): another version fetched from a server that refused the player's copy, kept hidden as `maps/versions/<name>.<checksum>.bsp` beside it, read in its place while that server is joined, the version each server took remembered |
| Engine | `engine/overview/plate.hpp` with `plugins/brush/app/overview_floors.hpp` | The map from above for the live map (`features.md`) |
| Engine | `engine/texgen/texture_map.hpp` | A map's painted textures replaced by procedural recipes (`.texmap`) |
| Tools | `tools/mapsdk` | Maps written as Python scripts by agents, saved as `.map` text (`docs/levels.md` § The map SDK) |
| Shell | `games/shooters_edge/server/bsp_map.hpp`, `client/map_load.hpp`, `client/map_line.hpp` | The map as the mod server plays it; loaded behind the window on a thread; a line's stop on either family |

## Each game's

| | Quake | Quake II | Next: the third | Next: the sector family |
| --- | --- | --- | --- | --- |
| Family | 29 and BSP2 | 38 | 46 (read today, patches too) | sectors and linedefs, converted to 3D once (`docs/porting-sprite-games.md`) |
| Collision | clip hulls from the tree | brushes traced (`brushes` solids) | brushes; patches as brushes (open) | the converted solids, with a thin fit per move |
| Pictures | in the map | wall files beside it (`wal.hpp`) | shaders over picture files (a new reader) | the archive's flats and wall patches |
| Movers | in its rules (`rules/`, the progs) | `server/q2_map.hpp` on the kit | its own table on the kit | sector movers (floors, ceilings, doors) on the kit |
| Place names | `locs/<map>.loc` | none: named off the map's items | none: named off the items | none: named off the items |

## Adding a game

1. The family's reader into `plugins/brush`'s one view (a new family is a new reader beside the
   others, its refusals named). Fixture writer in `write.hpp` and a fuzz case.
2. Its collision behind `MoveWorld` (a new solid kind only if neither hulls nor brushes fit). Every
   `sv_playermove` value and every rules program must then run on the map.
3. Its movers as rows of `mover_motion.hpp`; the state machine stays shared.
4. Its pictures through the adapter's `map_textures`.
5. Its overview floors beside `overview_floors.hpp`; its liquids, fog and sky through the brush
   plugin's `app/` (`picture.md`).
6. Its maps in `ge map` / `secli map` (survey, check) for free once the reader lands.

## Brought and left behind

| Benefit, now every game's | From | Layer |
| --- | --- | --- |
| Place names and their macros in team chat | Quake's community | `locs.hpp`; a game without loc files names places off its items |
| Brush collision exact to the authored solid | Quake II | the kit's trace, any rules on any family |
| Lightmaps rebaked finer with colour bounce | ours, on every family | `rebake.hpp`, `ce_rebake` |
| A map's fog, sky box, liquids as real bodies | the later clients of both games, ours | `plugins/brush/app/` |

| Drawback left behind | How |
| --- | --- |
| Collision only on three fixed hull sizes | hulls built from brushes for any box (`brush_hull.hpp`) |
| Each game's rules only on its own family | one trace: Quake's rules run on Quake II's maps by tracing its models as brushes |
| A turned brush model traced in one frame and drawn in another, or not drawn turned | one frame for both (`plugins/shooter/turned.hpp`): the server's trace, the client's prediction and the picture (`brush/app/bsp_draw.hpp`'s `Mover::rotation`), held to the same frame by `tests/shooter_turned_test.cpp` |
| Map loading stalls the window | read and decoded on a thread, compiled while drawing |
| A server whose map is another version of the player's refuses him (a later release rebuilt the stock maps) | the client keeps every version it meets by checksum and joins each server with the version it plays (`map_versions.hpp`); a map rebuilt only in its visibility (a water-vis build) has the same checksum and needs no version |

## Gaps

- The nav and the bots' world built from a 38 map: Quake's move run on one (M18v2c).
- The 46 family's patches in collision, its shaders.
- A map's versions named by release (original, rerelease) where a list shows both: no list shows
  them yet, and the rerelease's checksums are not measured.
- Quake II's protocol carries a map checksum too; its client does not keep versions yet.

## Labs and tests

`bsp`, `gpu-bsp`, `map-build`, `content-scan`, `se-q2`, `se-nav`, `map-intent`, `map-moves`;
`tests/brush_*_test.cpp`, `tests/shooter_world_test.cpp`.

## Lessons

- A file is read by its kind, never by the game that holds it.
- The classic rules run on Quake II's maps by tracing its models as brush solids: lift collision
  to one trace before writing any rules that use it.
- Labs use real maps from the player's packs, never test maps.
