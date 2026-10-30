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
ge tool classicenhance -- status <src>               # what the cache holds for this pack
```

`ge tool <plugin>` builds the plugin's `tool/` executable and relays its one JSON answer. The
cache is `GE_CLASSIC_CACHE` when set, else the platform's cache folder (`XDG_CACHE_HOME` or
`~/.cache`, `~/Library/Caches`, `%LOCALAPPDATA%`) under `ge/classicenhance/<digest>/`.

## The stages

| Stage | Writes | Rerun when |
| --- | --- | --- |
| `pack` | `pack/pak0.pak`, `NOTICE.txt`, `source.txt` | the source changes |
| `textures` | `textures/<name>/{albedo,height,normal,roughness,emissive}.png` | `kTextureStageVersion` or the options move |
| `lightmaps` | `lightmaps/<map>.png` (the map's own light, packed); the rebake's file once M41b lands | `kLightmapStageVersion` moves |
| `sounds` | `sounds/<path>.wav` (48 kHz, 16-bit, the loop start in a `cue ` chunk), `sounds/index.txt` | `kSoundStageVersion` or the options move |
| `models` | `models/<name>.glb` (every model: enhanced skins, smooth normals, a morph target a frame, its clips), `sprites/<name>/<frame>.png`, `models/index.txt`, `replaced.txt` | `kModelStageVersion` or the options move |

Each stage records a key in `stages.txt`; a file is written whole (to `.part`, then renamed), so a
stopped run never leaves half a texture that looks finished.

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
  10 a second). Units are metres, the same as a map's.
- **Replaced effects**: `replaced.txt` lists the files the engine draws its own effect in place of
  (`plugins/classicenhance/replace.hpp`): the explosion, flare and bubble sprites, and the torches'
  painted flames, which become the brush renderer's fire. A torch model keeps its stick and cage
  and loses the triangles whose skin glows, so the engine's fire burns where the painted flame was.

Sprites are written frame by frame at their source size, their transparent index clear.

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
