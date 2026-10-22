---
type: Added
title: The picture diff and ge lab diff
---

`labs <id> --compare=<tag>` (and `--diff=<tag>`, which runs nothing) now diffs every picture against the kept run: PSNR, SSIM, the mean and largest change and the share of pixels changed, over the whole picture and per distance band when a lab hands a distance map with its shot (`Lab::shot`'s new last argument, read off a frame's depth with `render::read_distance`), each band held to its own floor, with a four-panel sheet a shot (before, after, the change as a heatmap with the bands' contours, the bands in metres) and `diff.json`. `ge lab diff <id>` runs it in a game repo, every band under its floor a finding. The labs' PNG module reads PNGs back (`decode_png`, `read_png`).
