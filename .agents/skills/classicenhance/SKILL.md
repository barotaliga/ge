---
name: classicenhance
description: "Use for plugins/classicenhance in gameengine2: the classic packs (the shareware pack or the player's own full game) enhanced on the player's own machine. Finding the pack (the release zip and its self-extracting LHA archive, an installed folder, the pack file) the full game's later packs layered over the first, known releases reported by SHA-256 and any other pack taken unverified; the local cache and its stages (pack, textures, lightmaps, sounds, models); the texture enhancer (the trained upscaler, a dual aggregation transformer on the CPU and on the GPU through compute shaders, its weights fetched at bake time; the edge-aware kernel; the height, normal, roughness and glow maps); the sound enhancer (noise reduction, 48 kHz, the rebuilt top band); the models as glTF and the effects the engine draws in their place; the lightmap stage's slot for the rebake; the `classicenhance` tool run by `ge tool`; and the line that keeps it legal: ship the pipeline, never its output."
---

# The classic-enhance plugin

`plugins/classicenhance/` (`ge_classicenhance`, namespace `ge::classicenhance`) takes the
classic shareware pack a player already has and enhances it on their machine into a local
cache. It depends on the core, `assets` (the inflater, SHA-256), `audio` (the spectrum and the
resampler), `lab` (the PNG writer), `vfs`
(the pack and zip archives), `brush` (the maps) and `morph` (the models and sprites):
`ge_add_plugin(classicenhance ENGINE core assets audio lab vfs PLUGINS brush morph)`.

## The line that keeps it legal

- **Ship the pipeline, never its output.** The shareware pack may be passed on only unchanged;
  its textures, sounds and maps stay its authors'. Everything this plugin derives (upscaled
  textures, generated maps, rebaked light) is written to the player's cache and nowhere else.
  Nothing derived from the pack is ever committed, attached to a PR or put in a release.
  Pictures for a PR come from the tests' own fixtures. The user may ask to see the real pack's
  results: those go on a PRIVATE Artifact page for them alone, never linked from the PR.
- **Read any pack that reads; name the ones that are known.** `source.hpp`'s `kKnownPacks` lists
  known releases by SHA-256 and they are reported as such; any other pack (the full game the
  player owns, another release) is taken as their own copy and reported unverified. A pack that
  does not read is refused with its digest and nothing is written. The full game's later packs
  (`pak1.pak` ...) are layered over the first (`Source::content`). No stage may list one
  release's files: find assets by path and kind, so the full game works whole. Never fetch,
  commit or publish the full game's data; only the shareware pack is ever fetched here.
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
| `classicenhance/upscaler.hpp/.cpp` | The trained texture upscaler (M41g): `kUpscalerWeights` (where the tool fetches them, their SHA-256), `load_upscaler`, `open_upscaler`, `upscale` on the CPU, `seeded_upscaler_weights` (the small network the tests and labs run), `UpscaleDevice` |
| `classicenhance/net.hpp/.cpp` | The upscaler's CPU layers: blocked linear and 3 × 3 kernels, depthwise, layer norm, batch norm folded, GELU, softmax, pixel shuffle |
| `classicenhance/safetensors.hpp/.cpp` | The weights file's reader and writer |
| `classicenhance/app/upscaler_gpu.hpp/.cpp`, `app/upscaler.wgsl` | The upscaler on the GPU: the weights uploaded once, a texture's dispatches recorded and submitted at once; the tool links it when the build has a GPU |
| `classicenhance/planes.hpp/.cpp` | The wrapping float planes the enhancer works in: blur, percentile, the sRGB tables |
| `classicenhance/cache.hpp/.cpp` | The cache's root (`GE_CLASSIC_CACHE`, else the per-user cache folder), the per-pack layout, stage records, whole-file writes, `safe_name` |
| `classicenhance/lightmaps.hpp/.cpp` | The lightmap stage: `light_map` writes a map's light; until the rebake (M40) lands, the map's own lightmaps shelf-packed into one picture |
| `classicenhance/sound.hpp/.cpp` | The sound enhancer: `read_wav`, `write_wav` (the loop start in a `cue ` chunk), `reduce_noise`, `extend_band`, `enhance_sound` |
| `classicenhance/models.hpp/.cpp` | The models stage: `enhance_models` (every model as glTF with enhanced skins, sprites' frames as PNGs, the index and `replaced.txt`), `drop_glowing` |
| `classicenhance/replace.hpp` | `kReplacements`: the pack files the engine draws its own effect in place of (file, effect id, keep the glowing parts, why) |
| `classicenhance/pipeline.hpp/.cpp` | `run_pipeline`: the stages pack → textures → lightmaps → sounds → models, each skipped when its recorded key is current; `pack_textures` |
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
ge tool classicenhance -- run <path> --upscaler=kernel --device=cpu   # no download, no GPU
make lab LAB=gpu-upscaler ARGS="--weights=<file> --limit=2"           # CPU against GPU, timed
make lab LAB=enhance ARGS="--weights=<file>"                          # the network's column
```

The trained upscaler's weights are fetched by the tool into the cache's `weights/` folder (never
into the tree); in this repository's container fetch them into the scratchpad and pass
`--weights`.

`<path>` is the shareware release zip as published, an installed game folder (its `id1/pak0.pak`
and any later packs, in any case), the pack file itself, or a folder holding the release zip. In this repository's
container the pack is fetched into the scratchpad only, never into the tree.

## Changing a stage

1. A stage's output depends on its version and options: bump `kTextureStageVersion`,
   `kLightmapStageVersion`, `kSoundStageVersion` or `kModelStageVersion` when what it writes changes, so every cache
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
   Upscaler work: check a change against the original network on a real texture, numbers not
   looks (the CPU path agrees within 1.2e-6; a layer wrong by a transpose or a missed fold is off
   by tenths), then the GPU against the CPU (`gpu-upscaler` lab, `classicenhance_upscaler_gpu_test`).
   A buffer too small for what a kernel writes shows as a wrong picture, not an error: the GPU
   clamps the access. Size every map for the widest thing written into it.
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
