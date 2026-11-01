# Classic content enhanced locally

`plugins/classicenhance` (row M41) takes a player's own classic packs (the shareware pack, or
the full game they own) and enhances them on their machine into a local cache. The enhanced textures look sharper, and the
generated maps give a modern renderer normals, height, roughness and glow to light them with.

## The line that keeps it legal

The shareware pack may be used unchanged, as it comes in its release zip or as an installed game
leaves it; the full game is the player's own copy, for them alone. So the pipeline:

- starts from **any pack that reads**: the release zip (and the self-extracting LHA installer
  inside it), an installed folder, or the pack file itself. A pack whose SHA-256 is in
  `kKnownPacks` (`source.hpp`) is reported as that release; any other (the full game, another
  release) is taken as the player's own copy and reported **unverified** in `source.txt` and the
  tool's answer. A pack that does not read is refused, and the refusal prints the digest it saw;
- layers the **full game's later packs** (`pak1.pak`, `pak2.pak` ... beside `pak0.pak`, in any
  case, read until one is missing) over the first, a later copy of a path winning, as the game
  does. Every stage finds its assets by path and kind in the merged packs (`maps/*.bsp`,
  `sound/*.wav`, `progs/*.mdl` and `*.spr`), never by a list of one release's files, so the full
  game's every level, sound and model is enhanced. The cache folder is keyed by every pack's
  digest together;
- writes everything it reads or makes to the player's **local cache** and nowhere else, beside a
  `NOTICE.txt` that says not to share it;
- ships the **code** and never the output. Nothing read from the pack or made from it is
  committed, published, attached to a pull request or put on a page. Tests and the `enhance` lab
  run on fixtures painted in code.

Only the steps that are about the pack live here. The renderer's own effects (fog, reflections,
glow, eye adaptation, shadows) belong to the engine and apply to any level; the rebaked light is
the lightmaps work's (`plugins/brush`, M40) and this plugin only calls it.

## Running it

```sh
ge tool classicenhance -- verify <zip|folder|pack>   # which route, each pack's digest, known or not
ge tool classicenhance -- run <zip|folder|pack>      # every stage, skipping those up to date
ge tool classicenhance -- run <src> --only=textures --force --scale=4 --limit=8
ge tool classicenhance -- run <src> --upscaler=kernel     # no download: the kernel at any scale
ge tool classicenhance -- run <src> --device=cpu          # the trained upscaler on the CPU
ge tool classicenhance -- status <src>               # what the cache holds for this pack
```

A stage cut off between items (a cancel, a killed process, a failure) resumes on the next run
with the same options: each stage but the pack and the models keeps a journal,
`<stage>.partial` in the pack's cache folder (its key, then a line for each item whose files
are written), and the rerun skips every item the journal holds. A long CPU bake stopped at 186
of 217 pictures does the last 31. Changed options (a new key) or `--force` start the stage over;
the journal is removed once the stage is recorded.

`ge tool <plugin>` builds the plugin's `tool/` executable and relays its one JSON answer. The
cache is `GE_CLASSIC_CACHE` when set, else the platform's cache folder (`XDG_CACHE_HOME` or
`~/.cache`, `~/Library/Caches`, `%LOCALAPPDATA%`) under `ge/classicenhance/<digest>/`.

## The stages

| Stage | Writes | Rerun when |
| --- | --- | --- |
| `pack` | `pack/pak0.pak`, `NOTICE.txt`, `source.txt` | the source changes |
| `textures` | `textures/<name>/{albedo,height,normal,roughness,emissive}.png` | `kTextureStageVersion`, the options, the upscaler's weights or its device move |
| `lightmaps` | `lightmaps/<map>.gerb`: each map's light rebaked (`brush::rebake_map`: 0.25 m luxels, two bounces off the textures' mean colours, the light's direction, and the bounce alone); the shareware pack's 21 maps take about 4 minutes on four cores and 53 MB | `kLightmapStageVersion`, the options or the textures' stage move |
| `sounds` | `sounds/<path>.wav` (48 kHz, 16-bit, the loop start in a `cue ` chunk), `sounds/index.txt` | `kSoundStageVersion` or the options move |
| `models` | `models/<name>.glb` (every model: enhanced skins, smooth normals, a morph target a frame, its clips), `models/<name>/skin<k>.png` and `glow<k>.png` (each skin alone, for a game drawing the pack's own model), `sprites/<name>/<frame>.png`, `models/index.txt`, `replaced.txt` | `kModelStageVersion` or the options move |
| `pictures` | `pictures/<name>.png` (the wad's pictures and the 128x128 font sheet by lump name, `pictures/gfx/<name>.png` for the pack's own picture files), unscaled, in the palette's colours with the see-through index clear; the same upscaled under `pictures/enhanced/` (`enhance_picture`); `pictures/index.txt` | `kPictureStageVersion`, the scale, the sharpening, the grain or the upscaler moves |

Each stage records a key in `stages.txt`; a file is written whole (to `.part`, then renamed), so a
stopped run never leaves half a texture that looks finished.

## The menu

In the game the player does not run the tool: the classic-enhance menu (`menu.hpp`) does the same
from a menu over the engine's own (`engine/ui`). Its root page picks the pack among those found on
the machine, starts ENHANCE and opens the switches. ENHANCE starts an `EnhanceJob`
(`progress.hpp`): the pipeline on its own thread, reporting every stage's start, every item and
its end (`RunOptions::progress`), and stopping after the item it is on when cancelled
(`RunOptions::cancel`, the stages finished kept). A `ProgressBoard` keeps what the reports say
for the drawing thread: each stage's state (waiting, working, up to date, done, failed), its
count and the item it is on, the run's whole fraction (each stage alike, a skipped one whole), the
time so far and the time left at the running stage's own rate, and the last six textures, the
pack's picture beside its enhanced albedo, shrunk to 96 texels.

The enhancing page draws that board (`draw_progress`) in a panel beside the column (above it on a
phone held upright): the head (ENHANCING, READY or STOPPED and the percent), the whole bar, the
clock, a row a stage with its bar, then the textures and the result: READY with PLAY lit, or the
stage that failed and how to go on. The player can leave the page; the run goes on. The
`classic-menu` lab pictures every page at three screen sizes over a run on the lab's fixture pack.

## The switches

Every enhancement is a switch (`switches.hpp`), off meaning the classic original as the pack has
it: the textures (enhanced, the trained upscaler, the relief, the glow), the light (rebaked, live
lights, shadows), the world (round arches, bevels, rubble, clutter, decals), the liquids and the
sky (water, lava, slime, teleporters, skies), the effects (fire, bonfires, muzzle fire, explosions,
flares, bubbles, lightning beams), the air (fog, low clouds, wind, weather), the picture (reflections,
bloom), the models (enhanced skins, smooth shading, rounded models, lit weapons) and the sound
(enhanced sounds, room acoustics). Each is a cvar `ce_<id>` (`bind_switches`: boolean, saved with
the player's settings), so the console and a config set it as the menu does (`ce_decals 0`); the
menu's `switches` pages hold a toggle a switch, a page a group, with every switch of the group on
or off together and the defaults. Most switches change only what is drawn and played, read by
whatever draws or plays the thing; two change the cache (`apply_bakes`: the trained upscaler off
leaves the kernel, rounding on rounds the models) and want ENHANCE again. `ce_skies` is read by
the modern look (`brush/app/bsp_draw.hpp`'s `pack_sky`): on, the sky faces show our sky, its
clouds and its night; off, the pack's own two scrolling layers under the modern look's lights.
`ce_bonfires` burns the braziers (a map's large flames on the floor) as bonfires throwing sparks
into the wind and the lanterns' caged flames bigger (`brush/app/bsp_flames.hpp`, M41l); off,
they burn at the pack's flame sizes. The wall torches are torches either way.
`ce_relief` and `ce_parallax` light the enhanced textures with their maps under the modern look
(`brush/app/bsp_draw.hpp`'s `relief`, `specular` and `parallax`; the game reads the maps beside
each albedo with `cached.hpp`'s `cached_surface` into the scene's surface atlas). `ce_relief` puts
each texel's normal under every light (the torches, the lamps, the sun and the sky) and gives each
lamp and torch a highlight by the texel's roughness (GGX, widened for a near flame and dimmed at
glancing angles: the metal shines, the stone barely); off, flat
faces. `ce_parallax` marches the eye's ray into the height map (parallax occlusion, 3 cm deep from
the top to the deepest, within 10 m of the eye and faded out before it), so mortar and cracks sink
into the wall and shift as the player moves, and shades the crevices on the room's light; off, the
bumps lie flat. Neither changes the cache.
Under a lava's or a slime's level the modern look draws the liquid round the eye
(`render/liquid.hpp`, seen from below, with a least view of 3 m) and leaves out the screen's flat
tint for it.

`ce_rebake` reads the map's rebake from the cache (`cached.hpp`'s `cached_rebake`, for a map of the
same vertices only) and, under the modern look, adds the light bounced round the rooms from it
(`brush/app/bsp_draw.hpp`'s `rebake_bounce`) in place of the stand-in the look otherwise uses (a
share, `r_bounce`, of the pack's own lightmaps). The bake leaves out the sun and the open sky,
which the game draws live at the map's own hour. The classic walls are dark (their mean colour is
about 0.1 in linear light), so their true bounce is about a twentieth of the direct light: rooms
read a little darker and more contrasted than under the stand-in, with the walls' colour in their
shadows. Off, the stand-in.

## The texture enhancer

Each wall texture is indexed colour on a 256-entry palette. The enhancer works in linear light:

1. **Upscale** by kernel regression steered by the local structure tensor. Along an edge the
   kernel is long and across it short, so edges stay crisp without the stair steps of
   nearest-neighbour or the blur of Catmull-Rom.
2. **Back-project** once: the result shrunk back is compared with the source and the difference
   is spread back, so the albedo still averages to the original texels.
3. **Finish**: a light unsharp mask, a clamp to each 2 × 2 source neighbourhood's range (no halos),
   and fine grain scaled by local contrast, so flat areas do not look like plastic.
4. **Height** is band-passed luminance stretched between its 2nd and 98th percentiles; the
   **normal** is its Sobel gradient (+Y up); **roughness** is the material's base (stone, metal,
   wood, liquid) moved by height; **emissive** is the fullbright palette entries (224 and up).

Every step wraps at the edges, so the textures still tile. Sky textures get the albedo only.

## The trained upscaler

At 4× (the default) the albedo is a trained network's instead of the kernel's (row M41g): a
**dual aggregation transformer** trained to upscale material textures, keeping grain, cracks
and worn edges where the kernel can only keep the edges sharp. The height, normal, roughness and
glow maps are still made from that albedo as above; the glow stays the kernel's.

- **The weights** (`kUpscalerWeights` in `upscaler.hpp`: a CC0 release, 32.5 million parameters,
  140 MB in the safetensors layout) are fetched once by the tool into the cache's `weights/`
  folder with `curl` and held to their published SHA-256; `--weights=<file>` reads a copy from
  disk. They are never committed. `--upscaler=kernel`, or any other `--scale`, uses the kernel
  and fetches nothing.
- **The network** (`upscaler.hpp`): a 3 × 3 convolution to 180 channels, six residual groups of
  six blocks, then upsampling by two pixel shuffles. An even block attends within windows of
  8 × 32 tokens (half the channels) and 32 × 8 (the other half), with a learned bias by the
  tokens' offset, some blocks on windows rolled by half a window; an odd block attends across
  channels over the whole map. Each attention is gated by a depthwise convolution of its values
  and gates it back, and a gated feed-forward follows. The texture is wrapped round a margin of
  8 texels before it goes in, so the network sees what the wall shows across each edge, and the
  margin is cropped off after.
- **On the CPU** (`net.hpp`, `upscaler.cpp`): the layers in plain C++ in a fixed order, the rows
  split in bands on the worker pool. It gives the original's output within 1.2e-6 (an 8-bit step
  is 3.9e-3), and the tests hold a small seeded network of the same form to values the original
  gives for the same weights. A 64 × 64 texture takes about 3.5 s on four cores.
- **On the GPU** (`app/upscaler_gpu.hpp`, `app/upscaler.wgsl`): the same layers as compute
  shaders through Dawn: a tiled 64 × 64 matrix product (a 3 × 3 convolution is the same product
  over each token's neighbours), the window attention one query a thread with the keys streamed
  through workgroup memory and the softmax kept online, the channel attention as reductions.
  The weights are uploaded once; a texture's few hundred dispatches go in one submit. It matches
  the CPU within 1e-4 (the tests' network), a step of 1 in 8 bits at most on the trained one.
  The tool uses it when the build has a GPU and the adapter is not a software one
  (`--device=auto|gpu|cpu`); the `gpu-upscaler` lab times both.

The cache's `textures` key names the weights' digest and the device, so a new network or a move
between CPU and GPU reruns the stage.

## The held weapons

The pack's first-person weapons (`progs/v_*.mdl`) are drawn through the engine's view-model layer
(`engine/viewmodel`, `docs/rendering.md` § The view model); `plugins/classicenhance/weapons.hpp`
is the bridge, and like everything here it reads the player's own pack and writes nothing that
ships.

- **The form**: the classic model reader (`plugins/morph`) reads each file; `held_from_morph`
  turns every frame into the held frame in metres. The files author a held weapon at the eye
  (+x forward, +y left, +z up), so no grip mount is needed: the classic row
  (`classic_weapon_row`) holds it at the eye with no offset, and the bob, the lag on a turn and
  the kick are the engine's. The two frames differ in handedness, so the triangles are wound the
  other way round to keep their fronts outward. The frames play at ten a second.
- **The skin**: the palette indices through `gfx/palette.lmp`; the fullbright entries (224 and
  up) are the glow mask, so a sight or a lamp on the weapon lights itself. An enhanced skin from
  the cache can stand in for it unchanged.
- **The light**: the classic game lights a model by the floor under it. `probe_under` follows
  the line straight down from the player to the first lit surface (up to 2048 units) and reads
  its lightmap there, bilinear, with a luxel of 128 as 1 (the surface as painted): most of it
  ambient and the rest from above. The weapon is never darker than the classic 24 of 128
  (`kClassicFloor`). A shot's flash is a point light at the muzzle (`muzzle_flash_light`, 200
  units of reach); in the enhanced mode, whose torches burn as live lights, `flame_lights` lists
  the map's torches and flames as point lights (outside it their light is in the lightmaps
  already).
- **The fire**: in the enhanced mode a weapon's shot is the engine's muzzle fire
  (`engine/render/muzzle.hpp`), not the flash its file draws. `classic_fire_of` names each
  weapon's look by its file (the shotguns `muzzle.shotgun`, the grenade launcher
  `muzzle.grenade`, the rocket launcher `muzzle.rocket`, each turned down to how a real gun's
  flash looks: a few hundredths of a second of small, dim fire, the smoke what is left; the nailguns throw nails, not burning
  powder, so theirs is `muzzle.nailgun` turned down to a faint puff with no flame and no light;
  the axe and the lightning gun throw no flame) and its rate of fire. Each shot leaves from the
  barrel the file shows its flash on for that frame: where the flash stands on each firing frame
  is read as a barrel (`barrels`, `frame_barrel` on the held model), so the nailgun's two barrels
  fire in turn and the super nailgun's cluster one barrel at a time. A shot plays the firing frames at ten a
  second, blended (`classic_fire_frame`), and the flame is staged each frame off the muzzle carried
  into the world through the gun's lens (`viewmodel::muzzle_to_world`), so it rides the recoil.
  The file's flash triangles (the model's `flame` mask) are hidden with `DrawLit::hidden`.

The `classic-weapons` lab draws every weapon over the map in first person: the lineup at the
start, one weapon at the darkest and brightest places to stand, a shot in the dark and the
torches baked and live. The `classic-weapons-fire` lab (GPU) fires every weapon that throws a
flame in the explosion labs' yard: each shot through its life at dusk, the file's flash beside the
engine's fire, and each by day. With `--source` the pictures are of the player's pack and stay
local.

## The sound enhancer

The pack's sounds are 8-bit or 16-bit mono PCM at 11 025 Hz: band-limited near 5 kHz, with
quantisation hiss under the quiet ones. `enhance_sound` (`plugins/classicenhance/sound.hpp`) brings
each to 48 kHz in four steps, all deterministic:

1. **Noise reduction** at the source rate: a short-time spectrum (256 samples, hop 64), the noise
   measured in the sound's own quiet gaps, and a decision-directed Wiener gain with a floor
   (`floor_db`), which lowers the hiss without the warble of plain spectral subtraction. A sound
   with no quiet gap is held to a quarter of the quantisation floor, so a held tone is never taken
   for noise.
2. **Resampling** to 48 kHz through `audio::resample` (a Kaiser-windowed sinc; images under −85 dB).
3. **Bandwidth extension**: the empty band above the source's edge, up to `extend_to_hz`, is filled
   by copying the top of the real spectrum upward by a shift that is a whole number of frame hops,
   so the copies' phases stay coherent and transients stay sharp. Each copy is attenuated by the
   frame's own spectral slope plus `rolloff_db` an octave, and less is added to tonal frames than
   to noisy ones.
4. **Peaks**: a look-ahead limiter with a smooth gain holds them under `knee`. Levels are not
   normalised, so the set keeps its original balance.

The cache keeps WAV. A compressed form (Opus) waits on the engine's encoder (M23d2); at 64 kbit/s
the whole set is about 2.5 MB against the original 3.2 MB.

## The models

The pack's models (`progs/*.mdl`) are vertex-animated: a few hundred triangles, a skin or a few,
and every frame a full set of positions. `plugins/morph` reads them (and the sprites) into one
view; the models stage (`plugins/classicenhance/models.hpp`) writes each as one binary glTF:

- **Skins** go through the texture enhancer at `--scale` (glowing texels as an emissive picture).
- **Shading** is smooth: each frame's own vertex normals, summed across the skin's seam so the
  seam never shows. Rounding the facets into point-normal patches is `--round=<n>`, off by
  default: on the real models the patches swell thin parts and soften silhouettes drawn on
  purpose, and smooth shading alone reads better.
- **Frames** are morph targets, one a frame in the file's order, and the frames play as clips of
  morph weights (a frame group at its own intervals, a run of frames sharing a name stem at
  10 a second). Units are metres, the same as a map's. The engine's glTF loader reads the targets
  and the weight clips back (`assets/pose.hpp`'s `sample_weights` and `morph_mesh`), so
  `labs models --model=<cache>/models/<name>.glb` draws a cached model's clips frame by frame.
- **Replaced effects**: `replaced.txt` lists the files the engine draws its own effect in place of
  (`plugins/classicenhance/replace.hpp`): the explosion, flare and bubble sprites, and the torches'
  painted flames, which become the brush renderer's fire. A torch model keeps its stick and cage
  and loses the triangles whose skin glows, so the engine's fire burns where the painted flame was.

Sprites are written frame by frame at their source size, their transparent index clear.

## The pictures

The `pictures` stage writes the pack's 2D art twice: the status bar, its digits and alarm digits,
the portraits, the weapon, key and powerup icons, the 128x128 font sheet, and the menus' pictures.

- **As it is**, under `pictures/`: unscaled, for a HUD that draws them at a whole number of screen
  pixels a pixel, sampled nearest (`ce_pictures 0`). Index 255 is see-through in a picture and
  index 0 in the font sheet.
- **Enhanced**, under `pictures/enhanced/` (`enhance_picture`, `plugins/classicenhance/pictures.hpp`):
  upscaled at the textures' scale (4) by the textures' upscaler (the trained network under
  `ce_upscaler`, else the edge-aware kernel; `upscale_albedo` in `texture.hpp`). Before the
  upscale the opaque texels' colours are spread into the see-through ones round them, so the
  see-through index's colour never bleeds into an edge, and the picture is padded by its own edge
  texels, so it is not read as a tile that wraps; the font sheet is done a glyph cell at a time, so
  no glyph reaches into its neighbour. The alpha is the see-through mask sampled bilinearly at the
  new size and steepened about one half: a stroke one texel wide stays one texel wide, a staircase
  becomes a smooth edge, and the edge is about a pixel and a quarter soft.

Shooter's Edge's window reads them by their names (`cached_picture` in `cached.hpp`) when
`ce_pictures` is on (the default): its classic status bar, the counts and the crosshair in the
glyph sheet, the classic menu's plaque, titles, strips, cursor, help pages and text box, the
glyph text of the menu pages, the intermission and the scoreboard all come from four times the
pixels, drawn at their classic sizes and filtered (`client/classic_pictures.hpp`). A picture the
cache lacks is the pack's own scaled up nearest, so one set never mixes sizes. The demo video's
status bar is drawn the same way. The `se-menu` lab (`--pack`) adds an `enhanced` set once the
stage has run on that pack, and the `se-hud` lab reads `--classic=<cache>/pictures/enhanced`.

## The air

Every classic level is played in the evening or at night, in a wind and under low cloud. The air
over each is a `wind::Climate` (`docs/climate.md`), held per level in `climate.hpp`:
`level_airs()` lists the pack's levels in the order they are played, each with what the level is
(a base by cold water, a castle, a cave, a hall by lava), its solar hour, its mean temperature,
swing and humidity, and its pools' water temperature. So its pools steam in the cold, the breath
shows in its cold halls, and what falls over its open yards is snow or rain by the wet bulb.
`level_air(map)` looks a map up by name in any case; a map the table does not hold (a level of the
full game, a player's own map) is played under `fallback_air()`. `lava_warmth` is the warmth a
lava pool adds to the air near its edge (`Site::warmth`). The `classic-climate` lab draws every
level's air and fog by the hour.
