---
type: Added
title: Demo series (.geds): a clan fight's maps or a run's levels in one file
---

A demo series keeps replay files in the order they play. Shooter's Edge plays one recording after another as one run; the window's `record <name>` now writes `<name>.geds` and keeps recording across level changes until `stop`, a disconnect or quitting; `secli demos merge` makes a series from replay files, series and classic recordings such as `.mvd` (packed first), and `secli demos split` gives its replay files back byte for byte.
