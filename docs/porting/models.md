# Models: figures, skins, poses, items, the weapons players carry

Everything in a level that is not the map: players and monsters, their skins and colours, their
frames and poses, the items and weapons lying about, missiles, gibs, flames, and the weapon each
player carries in the third person. The held weapon in the first person is its own page
([`held-weapons.md`](held-weapons.md)).

## Shared

| Layer | File | What it gives |
| --- | --- | --- |
| Engine | `engine/render/morph.hpp` | THE MORPH PASS: vertex-animated meshes, two frames blended on the GPU, tints, holes (`kMarkHole`), warps (`MorphWarp`) |
| Engine | `engine/render/sprites.hpp`, `sprite_shapes.hpp` | Sprites and their facing |
| Engine | `engine/assets/` (`gltf.hpp`, `iqm.hpp`, `pcx.hpp`, `tga.hpp`, `image.hpp`) | Authored models and skins; the picture formats a skin comes in |
| Engine | `engine/overview/mesh_picture.hpp` | A mesh drawn small on the CPU, as a figure or an icon (the live map) |
| Morph plugin | `plugins/morph/read.hpp`, `model.hpp` | EVERY classic model family (the first's, the second's, the third's) and the sprite families into ONE form: skins, the seam split, every frame a morph target; damage refused |
| Morph plugin | `plugins/morph/skeletal.hpp` | A skeletal model posed through its clips into morph frames, so the morph pass draws it unchanged |
| Morph plugin | `plugins/morph/smooth.hpp` | Per-frame smooth normals and the optional point-normal rounding (`ce_smooth`, `ce_rounded`) |
| Morph plugin | `plugins/morph/carry.hpp` | A figure's pieces cut out by shared edges, a piece's rigid fit across frames, a held model placed per frame (`carried`, `carried_by`), a mesh's geodesic ends (`surface_distance`, `surface_ends`: hands, feet, head) |
| Morph plugin | `plugins/morph/glb.hpp`, `overview_mesh.hpp` | A model as glTF (classic enhance's models stage); one frame for the overview |
| Classic plugin | `plugins/classic/vweps.hpp` | Visible weapons: the list line, model paths, the fixed list, finding gun, axe and hands, stand-ins built in memory from the player's pack |
| Classic enhance | `plugins/classicenhance/models.hpp`, `cached.hpp` | The pack's models as glTF with enhanced skins, sprites as PNGs; read back by name |
| Shell | `games/shooters_edge/app/classic_models.hpp`, `figure_frames.hpp` | Models, sprites and brush items as morph meshes; one figure's frames mapped to another's by their names' kinds |
| Shell | `games/shooters_edge/client/world_models.hpp`, `model_lerp.hpp`, `model_filters.hpp` | Loaded, placed and drawn each frame (`filtered_models` swaps carried weapons); drawn between frames; gibs, bodies and flames hidden by choice |
| Shell | `games/shooters_edge/net/classic_looks.hpp` | The skins and figures a server offers (`listskins`) |

## Each game's

| | The first game | The sequel | Next: the third | Next: the sector family |
| --- | --- | --- | --- | --- |
| Model family | its own, one skin a model, shirt and pants colours | its own, `.pcx` skins in a folder a player model | its own (read today): a body of head, upper and lower parts joined at tags, `.skin` files | sprites in eight rotations; 3D models remixed in are baked to sprites, never the reverse |
| Poses | the frames' names (`app/figure_frames.hpp`) | `server/sequel_poses.hpp`, stepped at 10 Hz and eased | its animation config: legs and torso apart | sprite frames by state |
| Carried weapon | none in its model: built by `carry.hpp` from the player's pack | a weapon model a figure, the swap only | tagged weapon models: the swap only | a sprite of its own |
| Gibs | its rules' gib models | `server/sequel_gibs.hpp` | its own | its own |

## Adding a game

1. The model family's reader into `plugins/morph`'s form, fixture writer and fuzz case. A family
   whose parts join at tags (the third's) is joined into one form at load, not drawn in parts.
2. `open_model` in the game's adapter; skins through the shared calls, so stand-in PNGs and the
   enhanced cache work at once.
3. Its poses by the frames' NAMES (`figure_frames.hpp`), so remix plays another game's figure.
4. Its server's model list and weapon index for carried weapons; the swap is shared.
5. Every classic-enhance stage on its models (`models.hpp`), counted against the packs.
6. Its gibs, filters and statics; the live map's icon for each of its items (`livemap/icons.hpp`).

## Brought and left behind

| Benefit, now every game's | From | Layer |
| --- | --- | --- |
| Weapons seen in other players' hands | the sequel and the first game's community | `carry.hpp` (shared), `vweps.hpp`; the first game's figure gets one cut and fitted from its own pack |
| Animation blended between frames | the later clients | `model_lerp.hpp`, the morph pass |
| Gib, body and flame filters | the first game's community | `model_filters.hpp`, both games |
| Upscaled skins, smooth normals | ours | `ce_models`, `ce_smooth` |

| Drawback left behind | How |
| --- | --- |
| Figures stepping at the server's 10 Hz | eased in the window, blended between frames |
| A missing model file drawn as nothing or an error | stand-ins built in memory from the player's pack, never written to disk |
| Facetted low-poly figures | optional smooth normals, creases kept |

## Gaps

- The third family read but not drawn in the window; its tag joins and its animation config.
- The sector family's sprites in the morph pass, and the bake of 3D models to sprites.

## Labs and tests

`morph`, `gpu-morph`, `se-models`, `se-sequel-models`, `se-skins`, `se-model-lerp`,
`se-deaths`, `se-vweps`, `se-vweps-fit`; `tests/morph_*_test.cpp`.

## Lessons (from the carried weapons)

1. Hands are the surface ends nearest the gun, measured ALONG the surface from the body's middle;
   straight-line distance misses the far hand.
2. The second hand is more than two hand-reaches from the first along the surface.
3. A grip is a piece hanging under the weapon, taller than long; the rear grip goes in the rear hand.
4. Measure the underside along triangle edges.
5. Aim the weapon's long axis at the front hand.
6. Fall back to the gun's motion in deaths.
7. Draw the figure alone with its hands coloured before tuning anything.
- Another game's figure plays this game's poses by its frames' names, and wears its own palette.
