# Picture: the two looks, the world's draw, effects

How a classic game is drawn: the CLASSIC look (the game as it was made to look) and the MODERN look
(the same faces under the engine's lights, shadows, sky, water and bloom), the world pass, the
effects of shots, blasts and arrivals, and the switches that turn each enhancement on or off. The
player meets it in every frame, in the VIDEO and CLASSIC ENHANCE menus, and in `r_look`.

## Shared

| Layer | File | What it gives |
| --- | --- | --- |
| Engine | `engine/render/` (`frame.hpp`, `lights.hpp`, `shadows.hpp`, `sky.hpp`, `water.hpp`, `liquid.hpp`, `fog.hpp`, `bloom.hpp`, `post.hpp`, `grade.hpp`, `taa.hpp`, `particles.hpp`, `explosion.hpp`, `beam.hpp`, `muzzle.hpp`, `flame.hpp`, `body_burn.hpp`, `mirror.hpp`, `underwater.hpp`, `precip.hpp`) | Every mechanism: the pass table, real-time lights and shadows, the sky and clouds, water and liquids, fog, bloom and the palette grade, particles, blasts, beams, flames (`docs/rendering.md`) |
| Engine | `engine/detail/decals.hpp`, `hits.hpp`, `surfaces.hpp` | Decals that age (scorch, bullet holes, blood), the marks a fight leaves, a surface's stuff from its texture's name |
| Brush plugin | `plugins/brush/app/bsp_draw.hpp` and its `Options` | THE WORLD RENDERER for every map family: classic or modern, our sky, our liquids, real-time lights; one `Options` field per enhancement |
| Brush plugin | `plugins/brush/app/bsp_sky.hpp`, `sky_box.hpp`, `bsp_fog.hpp`, `bsp_liquids.hpp`, `bsp_flames.hpp`, `bsp_weather.hpp`, `bsp_views.hpp`, `light_grid.hpp`, `casters.hpp`, `bsp_cull.hpp` | A map's sky picked by its texture, its own sky box, its fog areas, its liquids as bodies, its torches as flames, weather, mirrors, the light grid, moving casters, the cull |
| Classic plugin | `plugins/classic/app/look.hpp` | THE TWO LOOKS: `r_look`, the picture's `r_` cvars with a game's defaults, the stops (off, low, medium, high) of shadows, water and skies; `world_options` maps the switches to the renderer's `Options` |
| Classic enhance | `plugins/classicenhance/switches.hpp` | One `ce_` switch per effect, the catalog `kSwitches` (textures, relief, glow, rebake, shadows, decals, water, lava, slime, teleports, skies, flames, muzzles, explosions, trails, hits, blood, beams, warps, burning, fog, clouds, weather, reflections, bloom, models, smooth, weapons, sounds, acoustics, ...) |
| Classic enhance | `plugins/classicenhance/texture.hpp`, `upscaler.hpp`, `lightmaps.hpp`, `replace.hpp` | Textures upscaled with height, normal, roughness and glow maps; lightmaps rebaked; the pack's effects the engine replaces |
| Shell | `games/shooters_edge/app/look.hpp`, `effects.hpp`, `muzzle.hpp`, `weapon_fx.hpp`, `monster_fx.hpp`, `ladder.hpp` | The game's effects joined to the plugin's look: trails, bursts, dynamic lights, muzzle fire, the weapons' and monsters' modern effects, the quality ladder (`r_quality`) |
| Shell | `games/shooters_edge/client/world_draw.hpp`, `world_extras.hpp`, `world_fx.hpp`, `warps.hpp`, `burning.hpp` | The window's frame: the map, effects, sky, bloom, post; water, lava, flames and weather by switch; arrivals; bodies burning |

## Each game's

| | The first game | Quake II | Next: the third | Next: the sector family |
| --- | --- | --- | --- | --- |
| Light | grey lightmaps with styles | coloured lightmaps (texel times luxel over 128), lamps and sky faces emit | lightmaps and a light grid; shaders | sector light levels, no lightmaps: the converted map baked or lit live |
| The look's input | `client/world_draw.cpp` as both | Quake II's effect and render bits (`client/q2_look.hpp`) | its shaders read into `Options` | the palette and colour map (`engine/vfs/palette.hpp`) |
| Effects | `app/effects.hpp` (its fog at a warp, its trails) | its own: `q2_teleport_burst` and kin, rail, bolts, lasers | its own, by name | its own sprites' effects |
| Status | done | done, every switch checked (M18v6e) | reader only | research (`docs/porting-sprite-games.md`) |

## Adding a game

1. Draw the map in the CLASSIC look first, matching the original's brightness rule (settled by a
   clean-room reader, then measured on the game's own maps).
2. Read the game's render bits (its entities' effects, colours, shells) into what the picture
   draws, as `q2_look.hpp` does; never branch the renderer on the game.
3. Turn every `ce_` switch on the game's maps and compare: each must change the picture or be
   named as a gap. A difference in the map's data (Quake II's lightmaps) is a field of
   `Options` under the SAME switch (Quake II's lightmapped maps get a lightmap overlay path under `ce_shadows`),
   never a second switch.
4. The game's effects as its own functions in `app/effects.hpp` and kin, by the game's names. An
   effect the engine draws in a pack's place goes in `replace.hpp`.
5. A strength beside a switch is an `r_` cvar beside the `ce_` bool (the reflections' strength
   beside `ce_reflections`), with a menu slider.

## Brought and left behind

| Benefit, now every game's | From | Layer |
| --- | --- | --- |
| Coloured light carried through the modern look | Quake II | `bsp_draw.hpp`; any family with RGB lightmaps |
| A power's coloured shell, an item's pulse (`r_powershells`, `r_itempulse`) | Quake II | `client/q2_look.hpp`'s `power_look`; on in Quake II, off in the first game |
| A map's own fog, sky box and its turn | the later clients of both | `bsp_fog.hpp`, `sky_box.hpp`, `map_fog.hpp` |
| Real-time lights and shadows, our sky, water, lava, slime, bloom, upscaled textures | ours | `ce_` switches, every family |

| Drawback left behind | How |
| --- | --- |
| A fixed 8-bit picture | the modern look; the classic look keeps it by `palette_grade`, chosen, not forced |
| Liquids as flat warped faces | bodies of our water and liquids (`bsp_liquids.hpp`) |
| A teleport drawn with another game's fog | each game's own arrival effect; `ce_warps` adds the dust |

## Gaps

- Quake II's modern look keeps its lightmaps, so its shadows ride them (a lightmap overlay under `ce_shadows`);
  which other switches draw in which game is the census of `docs/goals/M18v.md` § M18v6e, kept
  current there.
- An item's return as an entity event in Quake II; the third game's shaders.

## Labs and tests

`gpu-bsp`, `se-world`, `se-q2`, `se-weapon-fx`, `se-q2-weapon-fx`,
`se-muzzle`, `se-burn`, `se-wade`, `enhance`; `tests/shooters_edge_effects_test.cpp`.

## Lessons

- A switch can reach `Options` and still draw nothing because the uniform lane it rides was never
  written. Check the lane before taking shots.
- Measure on real maps before keeping an effect: two switches changed under 1 % of the picture on
  classic maps and were judged for removal. Put lab views near lamps, items and figures; a start
  view often shows no change.
- Screen-space reflections use the face's own normal, not the relief's bumped one; gate them by
  baked roughness, keep them to floors, keep the default subtle.
- An effect that changes over time needs a chart lab, not a still (the flicker of lights, the reverb's RT60 at every spawn).
