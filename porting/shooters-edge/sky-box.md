# A map's own sky box (M18i12)

From a clean-room reader (the `clean-room` skill) over two community clients of the first game
and two of the sequel, 2026-11-12. Each fact is marked **read** (the source states it),
**derived** (worked out from what the source does) or **open**. Nothing was measured.

Coordinates here are the map's: x forward at yaw 0, y left, z up. The engine's axes are x, z, y
(`brush/map.hpp`'s `to_engine`).

## The first game

**The name** (read):

- The worldspawn key `sky`; a leading underscore is stripped from every key, so `_sky` works.
  One client also takes `skyname` (another family's key); both read, the later in the entity
  text wins.
- A console variable overrides the map (`r_skyname` in one client, `r_skybox` in the other);
  empty means the map's own. A `loadsky <name>` command sets it. Server-set and per-map group
  overrides exist in both clients.
- An empty name, or one holding a dot, loads no box.

**The files** for a name N, each of the six pictures found on its own (read):

- The suffixes in order: `rt`, `bk`, `lf`, `ft`, `up`, `dn`.
- The paths: `env/N<sfx>`, `gfx/env/N<sfx>`, `env/N_<sfx>`, `gfx/env/N_<sfx>`; for each,
  `.tga`, `.png`, `.jpg`, `.pcx` in that order; the first found wins.
- All six are needed: one missing refuses the whole box, a console line names the sky, and the
  classic sky is drawn.
- The box is square: its size is the smaller side of the `rt` picture, and every picture is
  rescaled to it.

**The orientation** (derived; every renderer path of both clients agrees but one, which looks
like a fault): looking along the axis to see the picture straight on, with the picture's top and
right edges toward:

| Suffix | Look | Top | Right |
| --- | --- | --- | --- |
| `rt` | +x | +z | -y |
| `bk` | +y | +z | +x |
| `lf` | -x | +z | +y |
| `ft` | -y | +z | -x |
| `up` | +z | -x | -y |
| `dn` | -z | +x | -y |

No picture is mirrored: in every row, right = look × top, the viewer's true right (derived).
`bk` is on the player's left at yaw 0 and `ft` on the right (derived).

**Where it is drawn** (read): through the map's sky faces only, looked up by the direction from
the eye, so it is at infinity (it turns with the view, never moves with the eye). No cloud layer
and no rotation over a loaded box. Fog, when on, tints it by a share (`skyfog`).

## The sequel

- The worldspawn key `sky` (default `unit1_`, the underscore part of the name), `skyrotate` in
  degrees a second about `skyaxis` (default 0 0 0), the box turned by the right-hand rule (read).
- Files: `env/N<sfx>` only, `.tga` (or `.pcx` on a paletted renderer; `.png` and `.jpg` in one
  client), no underscore forms (read).
- A missing picture is drawn as the "no texture" pattern; the rest still draw (read).
- The orientation table above holds unchanged (derived).
- The box is drawn after the world, centred on the eye at 2300 units, depth-tested, in the
  rectangles the sky faces mark (read).

## What the engine does

`plugins/brush/app/sky_box.hpp` (the orientation table in engine axes, the atlas),
`plugins/brush/app/bsp.wgsl`'s `sky_box_at` (the box on the sky faces, at infinity) and
`games/shooters_edge/client/files.hpp`'s `sky_box_files` (the paths and formats). The console override
is `r_skyname` (the map's own when empty). Not yet: `loadsky`, fog's tint, the sequel's rotation.
