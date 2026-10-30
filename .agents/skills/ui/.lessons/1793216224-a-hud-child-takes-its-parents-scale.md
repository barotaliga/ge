---
title: A HUD child takes its parent's scale, so hang a picture from a picture
date: 2026-10-28
scope: engine/hud/layout.cpp
concepts: [hud, layout, parent, attach, scale, unit, strip, readout]
---

An element with a `parent` is drawn at the parent's drawn scale times its own. Hang a strip of
icons from a readout drawn at scale 0.6 and the icons come out at 0.6 too: 12 px where 20 were
meant, and the layout check still passes because nothing overlaps. Look at the pictures, not
only at `problems`. Either hang the child from a parent of its own size, or undo the parent's
scale in the child's (`e.scale = 1 / 0.6`) and say why beside it. A child's `x` and `y` are in
the parent's units too: a picture parent's art pixels, any other kind's frame scale.
