---
title: Under a deck, key the exposure lower, or the flat light reads as a white-out
date: 2026-10-23
scope: engine/render/wgsl/frame.wgsl
concepts: [weather, overcast, deck, exposure, flat-light, white-out, fog, haze]
---

The exposure keys a white face under the day's light to the same grey whatever that light is.
Under an overcast, with the sun's light gone and the deck's grey spread from the clear day's
light, the snow then comes out as bright as sunlit snow and the deck as bright again, and the
picture is one white sheet. Key a white face under a deck lower (0.6 rather than 0.78): the snow
reads as the light grey of a grey day and the trees and lifts keep their contrast. Spread the
deck's own grey from the clear day's light on a level face over pi (the sun's and the sky's), so
it follows the hour and the night with no table of its own. And close a fog into the sky BEHIND
each surface (the sky's luminance along the view ray, the horizon's when looking down), so the
far ground melts into exactly the sky past it and no seam shows at the horizon.
