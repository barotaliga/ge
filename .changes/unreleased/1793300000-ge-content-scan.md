---
type: Added
title: ge content scan: a classic content set held to a game's asset map
---

`ge content scan <pak|dir>` lists every sound and model a classic content set ships or its maps name (`noise`, `noise1`..`noise4`, `model`) with where each is used, and every numbered sound set its maps pick (`sounds`, and the world's `worldtype`, mapped by the rows of the classes that pick by it); with `--assetmap` and `--registry` it holds them to a game's asset map and registry and writes the unmapped ones as asset-map lines to fill (`assetmap-draft.txt`), with a picture of what is mapped. It is the `content-scan` lab over the brush plugin's `scan_content`, `parse_asset_map` and `compare_content`; the fixture maps can now carry more of the world's keys and entities.
