---
type: Added
title: A real face's lakes, off the open map data
---

Every real face now carries its lakes, reservoirs and ponds as the open map data draws them
(the `real-hints` lab's `--bake --water`), and `mountain/tarn/mapped.hpp` lays them on the face's
map as the free ride lays its tarn: the surface set into the ground under the shore, frozen flat
where the map's day gives ice that bears a skier, a bowl of open water otherwise, the banks graded
to the ground round it. A lake's water is drawn only inside its shore (`render/water.hpp`'s
`shore_mask`), Fall Line freezes every lake on its own ice sheet, and the `world` lab takes
`--face` to photograph a face's lakes.
