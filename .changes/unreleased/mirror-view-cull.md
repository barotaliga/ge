---
type: Changed
title: A pool's reflection costs less to draw
---

The planar mirror of a pool (and the view up from under it) drew the whole map again every frame; it now draws only the faces that can reach its picture: inside its frustum and on the kept side of the water's cut. The picture is the same; on demo1 the mirror draws between a sixth and two thirds of the map's faces.
