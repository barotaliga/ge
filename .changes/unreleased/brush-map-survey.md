---
type: Added
title: The brush tool surveys a map
---

`ge tool brush -- ents <map> --near=x,y,z [--radius=<units>]` lists the entities round a point, nearest first, a brush entity measured to its box and given the box; `--class=` takes a glob (`light_torch*`). `liquids <map> [--deepest]` lists the map's pools of water, slime and lava with their boxes, depths and narrowest sides (`--deepest` the deepest an eye fits in). `slice <map> --x=|--y=|--z=<units> [--step=<units>]` prints the leaf contents on a plane as rows of characters, solid, empty, liquid and sky told apart.
