---
title: A soft shadow's taps round a point let rain and snow in through a roof; march the cone of ways instead
date: 2026-11-01
scope: engine/render/wgsl/fall_shelter.wgsl, plugins/brush/app/bsp.wgsl, engine/render/precip.wgsl
concepts: [shelter, precip, pcss, roof, indoor, depth-map, march, acne, bias]
---

The fall's shelter map holds, per texel, the surface nearest the sky along the fall. The old test filtered it like a contact-hardening soft shadow: the edge's width grew with how far below the blocker a point lies (metres, for snow driven at 75 degrees with a wide scatter) and its taps were compared against the point's OWN depth. Under a roof those taps reach out over the open ground beside the building, which lies deeper along the fall, and read as open, so snow fell and settled in roofed rooms. The fixture's store leaked too, unseen, because the lab's probes were CPU rays and nothing looked at the GPU's picture.

`fall_share` marches each way of the cone back up toward the sky (seven steps, 0.12 m to 10 m) and shuts a way at its first covered step: the air of a roofed room is covered at once. This is conservative near windows (a tilted way through a window crosses covered room air), so a window's patch is a little narrower than a soft shadow would draw it.

The bias must scale with the slant: a floor seen along a fall 75 degrees from the vertical changes depth by 3.7 texels' width per texel. `0.03 + 1.5 * texel * (1 + run)` m holds. The old filter's acne speckled the open yard; the march leaves it evenly wet.

`bsp.wgsl` resolves close to the embed's 60000-byte cap: a long debugging line pushed it over (`WgslEmbed.cmake`), so debug by writing a value into `w.albedo` in one short line.
