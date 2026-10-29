// A MATERIAL'S SURFACE (docs/rendering.md § Materials): what wgsl/surface.wgsl's physically
// based model reads of one material, as a game writes it. The default is the matte surface
// every material was before the model (Lambert, no lobe); the presets are starting points that
// name what each surface is like, for a game to tint.
//
// The GPU's row is two vec4s a material (`pack_surfaces`): albedo and roughness, then metal,
// spec, emissive and porous, as render/instances.hpp's families read it.
//
// ge:summary A material's physically based surface: albedo, roughness, metal, spec, emissive
// ge:keywords render surface material pbr roughness metalness metal specular emissive porous
// ge:keywords preset stone metal wood slime wet-floor glass matte pack_surfaces
// ge:skill render
// ge:test tests/render_lights_test.cpp
#pragma once

#include <span>
#include <vector>

namespace ge::render {

struct Surface {
  float albedo[3] = {1, 1, 1};  // linear; a metal's reflectance
  float rough = 1;              // perceptual roughness, 0 a mirror .. 1 chalk
  float metal = 0;              // 0 a dielectric .. 1 a metal
  float spec = 0;           // a dielectric's reflectance (0.5 = 4 % head on); 0 and metal 0: matte
  float emissive_nits = 0;  // its own light, cd/m^2, tinted by the albedo
  float porous = 0.6f;      // how much a soaking darkens it (0 sealed .. 1 a sponge)
};

// Starting points, by what the surface is (each takes the albedo given).
Surface matte_surface(const float rgb[3]);
Surface stone_surface(const float rgb[3]);               // rough, a dielectric, porous
Surface metal_surface(const float rgb[3], float rough);  // a metal of that roughness
Surface wood_surface(const float rgb[3]);                // worn varnish: a soft sheen
Surface slime_surface(const float rgb[3]);               // a wet film, glossy, a faint glow
Surface wet_floor_surface(const float rgb[3]);         // a sealed floor under water: a near mirror
Surface lamp_surface(const float rgb[3], float nits);  // a bulb's or a flame's glowing face

// Two vec4s a surface, as the GPU reads them.
void pack_surfaces(std::span<const Surface> surfaces, std::vector<float>& out);

}  // namespace ge::render
