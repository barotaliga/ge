---
name: classicenhance
description: "Use for plugins/classicenhance in gameengine2: the classic shareware pack enhanced on the player's own machine. Finding the pack (the release zip and its self-extracting LHA archive, an installed folder, the pack file) and refusing anything but a known unchanged pack by SHA-256; the local cache and its stages (pack, textures, lightmaps, sounds, pictures); the texture enhancer (the edge-aware upscale, the height, normal, roughness and glow maps); the sound enhancer (noise reduction, 48 kHz, the rebuilt top band); the lightmap stage's slot for the rebake; the `classicenhance` tool run by `ge tool`; and the line that keeps it legal: ship the pipeline, never its output."
---

# The classic-enhance plugin

`plugins/classicenhance/` (`ge_classicenhance`, namespace `ge::classicenhance`) takes the
classic shareware pack a player already has and enhances it on their machine into a local
cache. It depends on the core, `assets` (the inflater, SHA-256), `audio` (the spectrum and the
resampler), `lab` (the PNG writer), `vfs`
(the pack and zip archives) and `brush` (the maps):
`ge_add_plugin(classicenhance ENGINE core assets audio lab vfs PLUGINS brush)`.

## The line that keeps it legal

- **Ship the pipeline, never its output.** The shareware pack may be passed on only unchanged;
  its textures, sounds and maps stay its authors'. Everything this plugin derives (upscaled
  textures, generated maps, rebaked light) is written to the player's cache and nowhere else.
  Nothing derived from the pack is ever committed, attached to a PR or put in a release.
  Pictures for a PR come from the tests' own fixtures. The user may ask to see the real pack's
  results: those go on a PRIVATE Artifact page for them alone, never linked from the PR.
- **Read only a known unchanged pack.** `source.hpp`'s `kKnownPacks` lists the packs by SHA-256.
  A pack not on it (another release, a modded or damaged copy) is refused with its digest and
  nothing is written. Adding a row needs the release named and its licence checked first; ask
  the user.
- **Only the classic pack's own steps live here**: finding it, its palette's conventions (the
  fullbright range), what its texture names mean, and wiring its maps to the cache. A technique
  any game could use goes in the engine or the plugin that owns it (the light baker: `engine/`
  and `plugins/brush`, row M40; fog, reflections, glow, eye adaptation: the renderer), and this
  plugin calls it.
- **Never name the classic game** outside `porting/shooters-edge/` (`make agents-check`): say
  "the classic shareware pack", "the release zip". The release's own file names inside its
  archive (`resource.1`, `id1/pak0.pak`) are matched by the code and are fine.

| File | What |
| --- | --- |
| `classicenhance/source.hpp/.cpp` | `open_source`: a path to the release zip, an installed folder or the pack, found by every route and verified (`kKnownPacks`, `kKnownZips`); `pack_from_zip`, `verify_pack` |
| `classicenhance/lha.hpp/.cpp` | The LHA archive the release's installer is: headers of level 0 and 1 found behind a self-extractor's program, `-lh0-` and `-lh5-` unpacked, CRC-16 checked |
| `classicenhance/texture.hpp/.cpp` | One texture enhanced: the kernel-regression upscale with one round of back-projection, sharpening and grain; height, normal, roughness and emissive; `material_of`, `never_drawn` |
| `classicenhance/planes.hpp/.cpp` | The wrapping float planes the enhancer works in: blur, percentile, the sRGB tables |
| `classicenhance/cache.hpp/.cpp` | The cache's root (`GE_CLASSIC_CACHE`, else the per-user cache folder), the per-pack layout, stage records, whole-file writes, `safe_name` |
| `classicenhance/lightmaps.hpp/.cpp` | The lightmap stage: `light_map` writes a map's light; until the rebake (M40) lands, the map's own lightmaps shelf-packed into one picture |
| `classicenhance/sound.hpp/.cpp` | The sound enhancer: `read_wav`, `write_wav` (the loop start in a `cue ` chunk), `reduce_noise`, `extend_band`, `enhance_sound` |
| `classicenhance/pictures.hpp/.cpp` | The pictures stage's reading: `pack_pictures` (the wad's pictures and font sheet, the `gfx/` pictures; the palette and colour map left out), `picture_rgba` (index 255 see-through, index 0 in the font sheet) |
| `classicenhance/pipeline.hpp/.cpp` | `run_pipeline`: the stages pack → textures → lightmaps → sounds → pictures, each skipped when its recorded key is current; `pack_textures` |
| `classicenhance/tool/main.cpp` | The `classicenhance` executable (`verify`, `run`, `status`), one JSON object an answer |

Beside it in the engine: `engine/vfs/zip.hpp` (the zip reader, also the `.pk3` container),
`engine/assets/inflate.hpp`'s `inflate_raw`, `engine/assets/sha256.hpp`, and for the sounds
`engine/audio/fft.hpp` (the short-time spectrum) and `engine/audio/resample.hpp`.

## Running it

```sh
ge tool classicenhance -- verify <path>      # where the pack is, its digest, which release
ge tool classicenhance -- run <path>         # every stage into the cache
ge tool classicenhance -- run <path> --only=textures --scale=2 --limit=20   # a quick look
ge tool classicenhance -- status             # the cache's packs and their stages
```

`<path>` is the shareware release zip as published, an installed game folder (its `id1/pak0.pak`
in any case), the pack file itself, or a folder holding the release zip. In this repository's
container the pack is fetched into the scratchpad only, never into the tree.

## Changing a stage

1. A stage's output depends on its version and options: bump `kTextureStageVersion`,
   `kLightmapStageVersion` or `kSoundStageVersion` when what it writes changes, so every cache
   reruns it.
2. Texture work: measure before and after. Upscale each texture's own quarter-size mip back by
   4× and compare with its full-size picture (PSNR; the palette's quantisation caps it near
   24 dB), and look at zoomed crops beside nearest and Catmull-Rom. PSNR alone hides edge
   quality: a wider kernel scores the same and looks soft.
   Sound work: measure on synthetic signals in the test (a band-limited burst, a voice-like
   chirp, a hum with hiss), each resampled from a full-band original: the log-spectral distance
   above the source's band for the extension, and segmental SNR for the noise reduction, which
   must never lower it on a clean signal. Then listen: render the real set to a private page with
   before and after players. Never a per-sample `tanh` on the output: it leaves bars of
   harmonics in the rebuilt band; the look-ahead limiter does not.
3. Tests: `tests/classicenhance_*_test.cpp` on fixtures made in the test (an LHA archive from a
   small `-lh5-` encoder, packs from `vfs::write_pak`, maps from `brush::write_bsp`). A test
   passes its fixture's digest as the known list; the real list stays the default.

## The lightmap stage's owner

The rebake's baker and its `plugins/brush` adapter belong to the lightmaps work (rows M40a, the
baker in `engine/bake`, and M40b, `plugins/brush/rebake.hpp`). When `brush::rebake_map(map, input,
options, &out, &err)` lands, `light_map` (row M41b) fills `RebakeInput` with the enhanced albedo
(`texture_means`), the map's lights (`bake_lights`, both in `plugins/brush/app/draw_data.hpp`) and
the sky colour, and writes `write_rebaked`'s bytes (magic `GERB`, version 1; the light's direction
in world space) as `lightmaps/<map>.gerb`; `kLightmapStageVersion` moves to 2 so every cache reruns
the stage. Nothing else in the pipeline changes.
