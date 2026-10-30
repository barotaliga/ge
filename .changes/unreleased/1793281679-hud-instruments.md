---
type: Added
title: The HUD's instruments
---

`engine/hud/instruments.hpp` draws a needle `dial` (a range, a knee, a red band, ticks, figures
and the reading, its needle eased by a critically damped spring), a heading-up `wind` meter (the
felt wind's arrow, the weather's mark on the rim, the alarm colour past a gale) and a `compass`
strip with a game's marks, each a `ge::hud::Layout` kind. Fall Line's HUD gains a speedometer dial
(`hud_speedo_style dial` or `number`, `hud_speedo_max`), a wind meter off the mountain's air, and
a compass the player can show with `hud_compass_show 1`.
