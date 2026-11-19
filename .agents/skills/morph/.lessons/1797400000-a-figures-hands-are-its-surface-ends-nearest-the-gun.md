---
title: A figure's hands are the ends of its surface nearest the gun, measured along the surface
date: 2026-11-19
scope: plugins/morph/carry.cpp, plugins/classic/vweps.cpp
concepts: [hands, surface-ends, geodesic, dijkstra, grip, underside, carried-by, vwep]
---

Straight-line distance to the gun finds only the hand that touches it; the other hand, a forearm's
length away, loses to the chest. Walk the body's surface instead (`surface_distance`, Dijkstra over
triangle edges, the seam's copies at zero distance) from its middle (the vertex whose farthest reach
is smallest, not the one nearest the centroid, which can sit in a hand) and keep the ends
(`surface_ends`): the two nearest the gun are the hands, the second more than two hand-reaches along
the surface from the first, since one hand's box has several end corners. A pickup's grip is a
piece hanging under it (tall, taller than long, its foot near the floor); its rear one goes in the
rear hand. Measure the underside where the front hand rests along triangle EDGES, not by nearby
vertices: a long box has none near the middle. Aim the weapon's own long axis, corrected by the
underside's rise, or a sloped stock tilts it 45°. Where the hands come apart (deaths), fall back to
the gun's rigid motion.
