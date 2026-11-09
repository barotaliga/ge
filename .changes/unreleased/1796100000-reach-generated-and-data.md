---
type: Fixed
title: ge digest --affected reaches the labs over a changed mod's QC
---

`ge digest --affected`, `ge test --affected` and precheck now follow a changed file that the build turns into source, such as a mod's QC embedded as a generated `.cpp`. Ninja's graph names the generated file, which counts as changed, so the library compiling it and the labs and tests over that library are reached. A changed data file that a lab reads from the tree at run time, such as the QC a lab compiles from its sources, now reaches every lab whose text names its directory. A change to the baseline mod's QC alone used to reach none of se-baseline, se-dedicated, se-dedicated-lobby or se-team; now it reaches all four.
