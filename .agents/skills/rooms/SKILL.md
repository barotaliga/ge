---
name: rooms
description: "Use for plugins/rooms in gameengine2: the engine's own level format. Rooms, portals, solids and materials (a texture reference and a relief), the .gelv bytes and .gelv.txt text form, growing rooms from their seeds (reliefs, caves by surface nets), and the rooms lab."
---

# The rooms plugin

`plugins/rooms/` (`ge_rooms`, namespace `ge::rooms`) is the engine's own level format: a level is
a few plain shapes and every room carries the seed its surfaces grow from. It depends on the
engine's core only. The design is [`docs/levels.md`](../../../docs/levels.md); each file's header
states its part once.

| File | What |
| --- | --- |
| `level.hpp/.cpp` | The model (units, rooms, materials, portals, solids), `validate`, `link` |
| `codec.hpp/.cpp` | The `.gelv` bytes: `encode`, `decode` with its refusals |
| `text.hpp/.cpp` | The `.gelv.txt` text form: `parse`, `print` |
| `relief.hpp/.cpp` | The reliefs a surface grows: heights and grid sizes |
| `noise3.hpp` | Seeded 3D value noise and fbm on an integer hash |
| `build.hpp/.cpp`, `build_parts.hpp`, `cave.cpp` | Growing a room: walls, caps, frames, solids; caves |
| `levels/halls.gelv.txt` | The sample level: every relief, two caves |
| `labs/rooms_lab.cpp` | The `rooms` lab |

## Writing a level

Write the text form (`text.hpp` has the syntax) and check it with the lab:
`make lab LAB=rooms ARGS=--level=<file>`; a parse error names the line and what to write instead.
Corners run counter-clockwise seen from above (+x right, +z up the page). Two rooms that share a
doorway share a wall line, facing back; the portal is written once, in either room. An opening
into a cave needs `trim=`.

## Changing what grows

1. Run the lab first: `make lab LAB=rooms ARGS=--tag=before` (release build: `PRESET=release`).
2. Change the builder or a relief; a new relief is a row in `Relief` (`level.hpp`), its name and
   height and grid in `relief.cpp`, a row in `docs/levels.md`, and a material in the sample level.
3. `make lab LAB=rooms ARGS=--compare=before`; look at `reliefs.png`, `cutaway.png`, `inside.png`.
4. Rewrite the golden: `GE_GOLDEN_WRITE=1 build/<preset>/tests/rooms_build_test`, then
   `make labs-digests`, and say in the PR why the bits moved.

## Rules

- **Grown, never stored.** Nothing a seed grows goes in the file; a format change bumps
  `kFormatVersion` and keeps the reader of the old one.
- **Pure and exact.** No transcendental, no `Rng`, no state between rooms: rooms build side by side
  on the pool and give the same bits (`tests/rooms_build_test.cpp`).
- **Faces into the room**, every triangle, with its vertex normals agreeing (a prism's always; a
  crag cave's slivers at most a few in a hundred).
- **Budgets**: a relief grows at most `kMaxReliefCells` cells a surface, never finer than
  `kMinReliefCell`; finer detail is the texture's.
