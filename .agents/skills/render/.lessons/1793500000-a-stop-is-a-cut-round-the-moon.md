---
title: Round the moon a march's early stop is a cut: the glow is seen as the disc is
date: 2026-10-28
scope: engine/render/clouds.wgsl
concepts: [cloud-edge, glow, glow-seen, march-stop, extrapolation, feather, moon, hard-edge]
---

A cloud's glow round the moon is lifted to the disc's brightness as seen (`glow_seen`, the ratio to the disc to the power 0.35), so a thousandth of it is plainly visible. Any threshold in the cloud pass that drops more than that is a line across the moon. Three did: the march's stop at a hundredth with the rest of the path taken as thick (a ray past a thin wisp kept its hundredth, its neighbour stopped and lost it), the next layer dropped at a hundredth, and a layer under a ten-thousandth of cover skipped.

Feathering the density did NOT remove the line, because it was not in the density. To find a line like this, take out one stage at a time: `kGlowSeen = 1` made it vanish (so it was in the glow), and zeroing the glow of one layer at a time named the layer. Then look for every `if (...) break` and `continue` in that layer's path.

The longer march costs about half again in a thick cloud on lavapipe, so it is paid only within 0.1 to 0.3 rad of the key light, where the glow is. Away from the light the stop is a hundredth as before.
