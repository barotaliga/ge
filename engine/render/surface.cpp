// The surface presets and the GPU's packing.
//
// ge:summary The surface presets (stone, metal, wood, slime, wet floor, lamp) and their packing
// ge:keywords render surface preset pack_surfaces stone_surface metal_surface wood_surface
// ge:keywords slime_surface wet_floor_surface lamp_surface matte_surface
// ge:skill render
// ge:test tests/render_lights_test.cpp
#include "render/surface.hpp"

namespace ge::render {

namespace {
Surface with(const float rgb[3], float rough, float metal, float spec, float porous) {
  Surface s;
  for (int i = 0; i < 3; ++i) s.albedo[i] = rgb[i];
  s.rough = rough;
  s.metal = metal;
  s.spec = spec;
  s.porous = porous;
  return s;
}
}  // namespace

Surface matte_surface(const float rgb[3]) { return with(rgb, 1, 0, 0, 0.6f); }
Surface stone_surface(const float rgb[3]) { return with(rgb, 0.85f, 0, 0.45f, 0.9f); }
Surface metal_surface(const float rgb[3], float rough) { return with(rgb, rough, 1, 0.5f, 0); }
Surface wood_surface(const float rgb[3]) { return with(rgb, 0.55f, 0, 0.5f, 0.7f); }
Surface wet_floor_surface(const float rgb[3]) { return with(rgb, 0.08f, 0, 0.25f, 0); }

Surface slime_surface(const float rgb[3]) {
  Surface s = with(rgb, 0.12f, 0, 0.3f, 0);
  s.emissive_nits = 0.08f;
  return s;
}

Surface lamp_surface(const float rgb[3], float nits) {
  Surface s = with(rgb, 0.3f, 0, 0.5f, 0);
  s.emissive_nits = nits;
  return s;
}

void pack_surfaces(std::span<const Surface> surfaces, std::vector<float>& out) {
  for (const Surface& s : surfaces) {
    out.insert(out.end(), {s.albedo[0], s.albedo[1], s.albedo[2], s.rough, s.metal, s.spec,
                           s.emissive_nits, s.porous});
  }
}

}  // namespace ge::render
