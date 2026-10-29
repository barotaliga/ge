// THE LIGHT BAKE: a scene's static light, worked out once on the CPU into a lightmap atlas.
// For every luxel of every chart (scene.hpp, chart.hpp):
//
// - DIRECT LIGHT from each point, spot and sun light, its shadow traced against every triangle
//   (soft: `light_samples` points over the light's radius); the sun reaches a luxel only through
//   a sky face or past the scene.
// - THE BOUNCE, `bounces` passes: `samples` cosine-weighted rays a luxel, each carrying what it
//   meets: a lit face's light (its albedo × the last pass's irradiance there, plus its own glow),
//   or the sky through a sky face or past the scene. Colour bleeds because the albedo is a
//   colour. Each pass's gathered light is smoothed over its chart (a box of `filter` luxels) to
//   take the sampling's grain out.
// - THE DIRECTION: the luminance-weighted mean of the directions the light came from, direct and
//   gathered (`Lightmap::dir`). Its length is how much the light comes from one way: 1 under a
//   single small lamp, about 2/3 along the normal under an even sky, 0 when lit equally from
//   opposite sides. A renderer shades a surface's detailed normal with it
//   (`directional_scale`), so bumps catch the room's real lights.
//
// Luxels a surface does not cover, and luxels whose rays mostly meet the backs of faces (a
// luxel inside a wall), take the mean of their chart's good neighbours, so a bilinear read at an
// edge never fetches black.
//
// DETERMINISTIC. Every luxel draws from its own stream (`core::Rng`, seeded by the options'
// seed, its index and the pass), the luxels run in bands on the worker pool
// (core/parallel.hpp) and nothing is summed across bands, so the bake is the same bits for any
// number of workers (held by tests/bake_bake_test.cpp).
//
// ge:summary The light bake: direct light, colour bounce and the light's direction a luxel
// ge:keywords bake lightmap light-bake gi global-illumination bounce radiosity directional
// ge:keywords dominant-direction deluxemap directional-lightmap irradiance gather path-trace
// ge:keywords bake_scene directional_scale sample_lightmap luxel dilate denoise
// ge:skill render perf
// ge:test tests/bake_bake_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "bake/scene.hpp"

namespace ge::bake {

struct Options {
  int bounces = 2;        // gather passes after the direct light (0: direct light only)
  int samples = 64;       // gather rays a luxel a pass
  int light_samples = 4;  // shadow rays a light a luxel (1: hard shadows from its centre)
  int filter = 1;         // the gathered light smoothed over (2 filter + 1)^2 luxels; 0: none
  uint32_t seed = 1;
  size_t workers = 0;  // 0: the pool's default
};

struct Lightmap {
  uint32_t width = 0, height = 0;
  std::vector<float> rgb;  // irradiance, 3 a luxel, the scene's units (shown = albedo × E)
  // 4 a luxel: x, y, z the light's mean direction (world, pointing TO the light), its length the
  // share that comes from that way (0..1); w 1 where a surface covers the luxel, 0 where it was
  // filled in from neighbours.
  std::vector<float> dir;
  std::vector<float> direct;  // the direct light alone, 3 a luxel (labs and tests)
};

struct Stats {
  uint64_t luxels = 0;  // covered
  uint64_t filled = 0;  // filled in from neighbours (uncovered, or inside a wall)
  uint64_t rays = 0;    // traced: gather and shadow
};

// The scene's light baked. The scene's charts must be packed (pack_charts).
Lightmap bake_scene(const Scene& scene, const Options& options, Stats* stats = nullptr);

// The irradiance at the atlas point (x, y) in luxels (0..width), read bilinearly.
void sample_lightmap(const Lightmap& map, double x, double y, float out[3]);

// THE DIRECTIONAL SHADE, stated once (a renderer's shader restates it and names this function):
// what a lightmap's irradiance is multiplied by for a surface whose detailed normal is `n` and
// whose flat normal is `ng`, under the stored direction `dir` (length = directionality). It is a
// half-Lambert lobe renormalised so the flat normal gets exactly the baked irradiance:
// (n · dir / 2 + 1/2) / (ng · dir / 2 + 1/2). With no direction (length 0) it is 1.
inline double directional_scale(const double dir[3], const double n[3], const double ng[3]) {
  const double a = (n[0] * dir[0] + n[1] * dir[1] + n[2] * dir[2]) * 0.5 + 0.5;
  const double b = (ng[0] * dir[0] + ng[1] * dir[1] + ng[2] * dir[2]) * 0.5 + 0.5;
  return (a > 0 ? a : 0) / (b > 0.05 ? b : 0.05);
}

}  // namespace ge::bake
