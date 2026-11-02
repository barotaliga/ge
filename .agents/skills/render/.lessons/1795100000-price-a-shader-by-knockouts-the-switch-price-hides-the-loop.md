---
title: A switch's price hides what a shader's pixels spend; knock parts out in a scratch build and time the GPU span
date: 2026-11-02
scope: plugins/brush/app/bsp.wgsl, plugins/brush/app/bsp_draw.cpp, games/shooters_edge/labs/gpu_se_timedemo_lab.cpp
concepts: [gpu-cost, knockout, light-loop, light-grid, bilinear, wrap, division, select, gather, metal, picture-check]
---

- `se-timedemo --price=1` priced `ce_textures`, `ce_relief` and `ce_parallax` as the next cuts, but
  the biggest cost was in no switch: the loop over every light near the eye (up to 160) was 2.6
  of the M3 Max's 7.5 ms at 1440p. Find it by knockouts: edit the shader (a part off, a count
  capped), `make build PRESET=release`, run `se-timedemo --size=2560,1440` three times and read
  `GPU span: p50`; about 35 s a try. 1440p clears noise that 720p does not, and at 720p the
  frame waits on the CPU, so a GPU saving barely moves it.
- What did not pay: `textureGather` for the parallax's heights (no change: loads are not the
  cost); `select(x, y, c)` to skip work (it evaluates both); integer `%` for the wrap (1 ms
  SLOWER: integer division is dear). What paid: one float wrap a bilinear tap instead of four.
- A cull that keeps the picture to the byte: list the lights in ascending order and leave out only
  those whose reach misses the cell by a margin, so every pixel sums the same lights in the same
  order. `se-timedemo`'s pictures differ run to run on Metal (the flames and particles); judge
  sameness with `se-client --game=<packs> --map=e1m1` and `--game=fixture --map=arena`, which
  repeat byte for byte, against a build of `main`.
- Register pressure is cheap to test: delete the paths a frame never takes and time it. In
  `bsp.wgsl` the lightmaps', rebake's and weather's code cost 0.16 ms at 1440p though no pixel ran
  them; a second pipeline from the same file with a WGSL `override` (`kLitOnly`) takes it back
  exactly. Keep per-face paths (the teleporter's liquid) in both: only frame-wide modes can go.
- A loop's mere presence costs too: the parallax march run for one step still cost a third of
  the full march. Fewer steps by the texels crossed saved little (the upscaled textures are dense).
