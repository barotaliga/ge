---
type: Fixed
title: No black blocks over the picture from one broken pixel
---

A pixel whose texture coordinate the rasterizer left as no number (a sliver of a wall seen edge on, now and then) no longer turns a block of the picture or the whole of it black: a map's faces read that pixel's texture at its corner, and the bloom leaves any texel that is not a finite number out of its blur. Seen on the sequel's maps under the modern look, in a recording's video and in play.
