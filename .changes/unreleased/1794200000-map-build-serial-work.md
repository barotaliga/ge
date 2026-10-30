---
type: Changed
title: Map generation about 9% faster, with less of it on one thread
---

A ski area builds about 9% faster, every map's bits unchanged: the summit ramps' candidate lines are walked on several threads, a run's walk passes over a heading as soon as what it has read makes it dearer than the best so far, the ski routes read their grids and work every step's cost once on several threads, and a lane's search keeps each row's cheapest cost instead of rescanning the rows.
