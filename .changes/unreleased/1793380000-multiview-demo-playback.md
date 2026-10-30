---
type: Added
title: Shooter's Edge plays multiview recordings and watches demos at their own pace
---

The `shooters_edge` window reads the server's multiview recording of every player (`.mvd`, and `.mvd.gz` unpacked on the way in) beside the other demo formats, for both `benchmark` and the new `playdemo <demo>` (with `cl_demospeed` and `demo_track <slot>` to pick the player the view rides); `ge::core::inflate_gzip` reads gzip files, and the `se-demo` lab draws a demo's players and their health without a GPU.
