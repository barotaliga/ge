---
type: Added
title: The weather in the picture
---

`FrameView::weather` (`render/view.hpp`'s `Weather`: the sun's pass through the cloud, a deck's cover, tint and light, the visual range of a fog or a fall) is read once in `wgsl/frame.wgsl`: the sun's light, a deck's grey over the sky, the flat light it spreads, the air's grey under it, the fog closing every surface into the sky at its visual range, and a lower exposure key under a deck. A clear `Weather`, the default, changes nothing. `plugins/mountain/app/weather_look.hpp` gives the mountain's eight skies in these numbers (`weather_look`, `weather_at`), and the `weather` lab draws all eight.
