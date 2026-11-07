---
title: Into a frame with a mirror in it, a rotation's axis turns by -M, and the winding by the normals
date: 2026-11-06
scope: engine/assets/iqm.cpp
concepts: [iqm, handedness, mirror, quaternion, winding, skeletal-model]
---

The skeletal model file is +z up with its front +x; the engine is +y up, front +z, right +x. The
map (x, y, z) -> (-y, z, x) has determinant -1, so it is a mirror as well as a turn. A rotation's
quaternion is an axial vector: its (x, y, z) goes by det(M) M, not M, so (x, y, z, w) comes in as
(y, -z, -x, w); a scale is only permuted. Taking the vector by M alone turns every joint the
wrong way and the limb folds back through itself. Which way a file's triangles wind is not worth
guessing from the format: count the triangles whose face agrees with their vertices' normals
after the mirror's swap and turn them all over when most disagree. A round trip through the
writer proves only that the reader and writer agree; check the reader on a real exported file
through `labs models --model=<file>.iqm` before trusting it.
