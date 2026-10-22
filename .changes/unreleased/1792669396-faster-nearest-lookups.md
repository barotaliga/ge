---
type: Changed
title: Map generation about a quarter faster again, through its nearest-piste and nearest-run lookups
---

A ski area builds about 28% faster again (a single piste about 8%): the piste's nearest-point index is a dense grid over the cells it touches, and both it and the ski area's network index walk each search ring only where it crosses an occupied cell, with every map's bits unchanged.
