---
title: Close a reach's mist into the horizon's sky, keep the cloud out of it, and judge the target by pictures
date: 2026-10-23
scope: engine/render/wgsl/frame.wgsl
concepts: [mist, reach, draw-distance, haze, sky, clouds, horizon, ladder]
---

The reach's mist was tried three ways. Into the sky behind the pixel as the sky pass draws it,
the land past the reach turned into the sky table's dark under-horizon air: a mountain standing
in a dark sea. Into the horizon's sky (the direction raised to the horizon, and the sky pass
drawing the same under the horizon), the land fades into a bright haze, which reads as distance.
In both, the first build put the mist inside `haze`, which the cloud's colour also calls, and
every cloud vanished, because a cloud is kilometres away; the cloud goes through `haze_air`, the
air alone. Whichever way the mist closes, the sky pass must draw the same colour wherever no
surface is, or the reach shows as an edge. Look at the vista, the aerial view and the lookback at
every stop before believing the formula.
