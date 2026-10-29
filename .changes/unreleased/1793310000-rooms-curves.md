---
type: Added
title: Arches and pillars found in a brush level and drawn round
---
`rooms::find_curves` finds the runs of faces in a brush level that facet a round surface (an octagonal pillar, a rounded corner, an arch's ring or soffit, a vault) and fits each with its axis, centre, radius and sweep, leaving flat walls and chamfers out. `rooms::round_curves` draws them round: every triangle of a curve's faces is cut on one angular grid a curve and moved onto the curve's cylinder with round normals, while the faces a body meets stay faceted. `BrushMesh` now names each triangle's face. The `clone` lab counts the curves on each map and draws the largest close up, faceted and round.
