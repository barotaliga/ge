# Classic content enhanced locally

`plugins/classicenhance` (row M41) takes a player's own copy of the classic shareware pack and
enhances it on their machine into a local cache. The enhanced textures look sharper, and the
generated maps give a modern renderer normals, height, roughness and glow to light them with.

## The line that keeps it legal

The shareware pack may be used unchanged, as it comes in its release zip or as an installed game
leaves it. So the pipeline:

- starts **only** from a pack whose SHA-256 is a known unchanged one (`kKnownPacks` in
  `source.hpp`), read from the release zip (and the self-extracting LHA installer inside it), an
  installed folder, or the pack file itself. Anything else is refused, and the refusal prints the
  digest it saw;
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
ge tool classicenhance -- verify <zip|folder|pack>   # which route, the digest, known or not
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
