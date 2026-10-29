// A LOW-POLYGON MORPH MODEL ROUNDED: every triangle replaced by a curved patch (a cubic Bézier
// triangle built from its corners and their normals, the "point-normal" construction) cut into
// `level * level` triangles, every frame alike. The patch passes through the original corners
// and its edges depend only on the two corners they join, so neighbours meet without cracks and
// the model keeps its proportions; only the facets round off. Texture coordinates are spread
// linearly over each patch, so the skin stays where it was painted.
//
// A crease keeps its edge: where the corners' normals differ by more than `crease_deg` (a claw's
// tip, a blade's edge), the corner's normal along that triangle is its face's own, so the edge
// stays straight and sharp.
//
// Normals are a frame's own: each frame's vertex normals are its triangles' area-weighted normals
// summed over every copy of a position (seam copies share one), recomputed from the rounded
// frame for the draw.
//
// ge:summary Rounds a morph model's facets: point-normal triangles, every frame, creases kept
// ge:keywords smooth subdivide pn-triangles bezier patch crease normals morph frames tessellate
// ge:skill morph
// ge:test tests/morph_smooth_test.cpp
#pragma once

#include <vector>

#include "morph/model.hpp"

namespace ge::morph {

struct SmoothOptions {
  int level = 3;             // each triangle becomes level * level
  double crease_deg = 60.0;  // corners whose normals differ more than this keep a sharp edge
};

// Every frame's vertex normals, three floats a vertex.
std::vector<float> frame_normals(const Model& m, const Frame& f);

Model smooth_model(const Model& m, const SmoothOptions& opts = {});

}  // namespace ge::morph
