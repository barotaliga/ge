---
type: Added
title: Floor clutter by what a floor is made of and whether it is under the sky
---

`ge::detail::classify_surface` reads a surface's class off its texture's name and mean colour, and a `PropRule` may now keep to some classes and weigh the open sky against a roof (`ScatterEnv`). `ge::detail::clutter_rules` lays grass tufts on grass out in the open, twigs and branches under the sky, pebbles and rocks on stone and earth, clods on earth, moss in the damp, debris under roofs, and spent shells and bones on every floor, with eight new prop shapes built in code. The `detail-clutter` lab shows them over a test yard. The default moss no longer sinks out of sight into the floor.
