---
type: Added
title: Sight, steering and choosing for bots
---

`ge::ai` gains sight (`ai/sight.hpp`: the ground line over any height sampler, hearing, range and an occluder callback) and memory (`ai/memory.hpp`: the last place and time seen), steering (`ai/steer.hpp`: pure pursuit along a line, a fan of headings costed, separation over a spatial grid that visits in index order, the stuck check) and choosing (`ai/choose.hpp`: a utility selector and a flat state machine, each a decision record `ge why` reads); Fall Line's wanted level and army now stand on them with every digest unchanged, and the `sight` and `steer` labs draw a pursuer's sheds and a machine's fan.
