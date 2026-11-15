---
type: Removed
title: No props scattered on a level's floors
---

`ge::detail` no longer scatters props: the seeded rubble, debris and moss (`props.hpp`,
`prop_shapes.hpp`, `scatter_props`, `default_rules`) and the floor clutter by surface class
(`clutter_rules`, `ScatterEnv`, `classify_surface` by colour, the test yard) are gone, with the
`detail-props` and `detail-clutter` labs and the classic-enhance switches `ce_rubble` and
`ce_clutter`. None of it was ever drawn in a game; a classic map's floors stay as their maker left
them. `classify_name`, a surface's class by its texture's name, stays for the impacts of hits.
