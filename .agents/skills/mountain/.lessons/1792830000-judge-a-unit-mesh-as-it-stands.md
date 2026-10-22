---
title: Judge a unit mesh's faces as the instance will stand it, not in its own box
date: 2026-10-22
scope: plugins/mountain/app/trees.cpp
concepts: [trees, unit-mesh, instance-scale, aspect, snow-by-facing, normals, limb]
---

The tree meshes are unit-sized (1 tall, crown radius 1) and an instance stretches them about
4.5 times as tall as wide, so a face that looks steep in the mesh's own box is nearly flat once
standing. Classifying snow by the unit normal painted almost every tier white; a limb's round
cross-section came out flattened into a ribbon. Judge a face by the standing normal (its y
divided by the aspect, renormalised) and build any cross-section that must stay round with its
y offsets divided by the aspect. Whenever a mesh is built for a non-uniform scale, decide each
shape and each rule in the space it will be seen in.
