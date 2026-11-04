---
title: Call set_night once a frame; a second call takes the moon standing in as the key for the sun
date: 2026-11-04
scope: engine/render/night.cpp, games/shooters_edge/client/title.cpp, plugins/brush/app/bsp_sky.cpp
concepts: [night, moon, set-night, key-moon, eclipse, umbra, dress-sky, title]
---

`set_night` makes the moon the key light by moving it into the view's `sun_*` fields (`key_moon`). Called again on the same view (a `dress_sky` that already set the night, then `set_night` with an eclipse's shadow), the second call reads the moon's place as the sun's: the "sun" is up, so the night is a day's (white cloud, a white moon, no eclipse). Build the view's sky once: the style, the sun, the clouds (`clouds_of`), then one `set_night` with every field of the `Moon` (its `shadow`, `umbra` and `penumbra` for an eclipse).

An eclipsed moon gives a few thousandths of a full one's light, so the eye adapts far down: at its true size `adapt_cd` of about 0.001 cd/m^2 keeps the sky a night's and the cloud as dark shapes, where 0.5 is black; drawn 32 times nearer (a third of the frame) its light grows with its disc, and 0.01 is the night. The Earth's air lights the cloud's edge a moonlit white; the `hell` sky style's air takes the eclipse's red. A disc drawn nearer (`distance_km` smaller) keeps its surface brightness; grow the umbra and penumbra by the same factor.
