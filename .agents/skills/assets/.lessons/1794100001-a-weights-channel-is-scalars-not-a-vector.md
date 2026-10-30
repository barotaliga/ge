---
title: A morph weights channel is n scalars a key, not one vector
date: 2026-10-30
scope: engine/assets/gltf.cpp, engine/assets/pose.cpp
concepts: [morph-target, weights-channel, sampler, cgltf, accessor]
---

A glTF weights channel's output accessor is SCALAR, with a mesh's n target weights side by side
for each key (three runs of n for a cubic key: in-tangents, values, out-tangents). The
translation and rotation sampler reads one VEC3 or VEC4 element a key, and cgltf's
`cgltf_accessor_read_float(acc, k, out, n)` on a SCALAR accessor reads ONE float, so reusing it
for weights silently reads the first weight of each key. `sample_weights` in the loader reads
scalar `k * n + i` (cubic: `(k * 3 + part) * n + i`) instead. A track also starts from the mesh's
own weights so a clip that moves only some meshes leaves the rest at theirs; the vertices the
texture bake and the faceting copy carry their target offsets with them (`copy_vertex`).
