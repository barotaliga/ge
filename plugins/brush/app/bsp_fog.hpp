// A CLASSIC MAP'S OWN AREAS OF FOG (render/fog.hpp's `FogVolume`), read off the map when it
// loads, and the BSP renderer's sun map handed to the fog as its casters:
//
// - PLACED BY THE MAP: a `fog_volume` entity, a brush entity (`model "*n"`: its brush model's box)
//   or a point one (`origin` and `size "x y z"`, map units, z up), with the keys `density` (its
//   extinction at its floor, 1/m), `albedo "r g b"`, `glow "r g b"` (its own light, cd/m2 a metre
//   of extinction), `falloff` (1/m), `edge` (m), `phase`, `noise`, `noise_size` (m), `wind "x y z"`
//   (m/s, map axes), `flicker` and `sky` (0..1; unset, measured). A hall's dust, a crypt's mist.
// - OVER ITS LIQUIDS: a glowing haze over each pool of lava, thickest at the melt and rising on its
//   heat, flickering as it glows, and a faint green one over each pool of slime (`FogRows`).
// - FROM THE AIR (M51c): with the day's air handed in (`FogRows::air`, a `wind::Climate`, and the
//   solar hour), the fog that air lays over each pool of water and slime (wind/mist.hpp's
//   `mist_at` at its surface, the site seeing the pool's share of the sky): steam rising in
//   wisps off a pool far warmer than the air (`Climate::water_c` is the pools'), a radiation fog
//   on a clear, still night over an open one, drawn by render/mist_fog.hpp's `mist_volume`. A
//   windy night mixes the radiation fog away; steam rises in any wind. Without the air, none.
// - THE SKY'S SHARE (`FogVolume::sky`) of each: rays straight up from a grid of 8 x 8 points over
//   its floor, open where the first face they meet is a sky face, so a closed hall's dust takes no
//   light from the sky and a yard's all of it.
// - THE SUN'S CASTERS: the fog's light past the BSP renderer's own sun map (bsp_draw.hpp's first
//   orthographic map, `Options::fog_sun` draws it with the lightmaps' light too), so a hall's dust
//   shows the shafts the sun throws through its windows (`BspFogShadow`).
//
// The volumes are a pure function of the scene; the frame hands them to `render::prepare_fog`.
//
// ge:summary A classic map's own areas of fog: placed fog volumes, the haze over its lava and slime
// ge:keywords bsp brush fog fog-volume fog_volume dust haze lava-haze slime-haze sky-share
// ge:keywords bsp_fog sun-map casters shafts window bsp_fog_shadow steam-fog pool_mist climate
// ge:skill brush render
// ge:test tests/brush_bsp_fog_test.cpp
#pragma once

#include <vector>

#include "brush/app/bsp_draw.hpp"
#include "brush/app/draw_data.hpp"
#include "gpu/device.hpp"
#include "render/fog.hpp"
#include "render/shadows.hpp"
#include "wind/climate.hpp"
#include "wind/mist.hpp"

namespace ge::brush::app {

struct FogRows {
  bool placed = true;  // the map's `fog_volume` entities
  // The haze over a pool of lava: its height (m), its extinction at the melt (1/m) thinning by
  // `falloff` (1/m) upward, its own light (cd/m2 a metre of extinction) and its flicker.
  bool lava = true;
  double lava_height = 1.6, lava_density = 0.25, lava_falloff = 1.5;
  double lava_glow[3] = {25000, 7000, 1000};
  float lava_flicker = 0.2f;
  // The faint green haze over a pool of slime.
  bool slime = true;
  double slime_height = 1, slime_density = 0.08;
  double slime_glow[3] = {1000, 4000, 700};
  // The day's air (null: none) and the solar hour it is asked at: the fog it lays over each pool
  // of water and slime. Its wind drifts the fog along the map's +x, by the pool's sky share.
  const wind::Climate* air = nullptr;
  double hour = 21;
};

// One `fog_volume` entity as a volume (its sky share left at 1); false when it is not one, or has
// no box (neither a brush model nor a size).
bool fog_volume_of(const Map& map, const Entity& entity, render::FogVolume* out);

// The share of the volume's floor open to the sky straight up (1 when nothing is over it).
double sky_share(const Scene& scene, const render::FogVolume& volume);

// The fog the day's air lays over the pool `pool` at `hour`, seen from a site that sees `sky` of
// the sky (`MistKind::none` when the pool is lava or nothing lies).
wind::Mist pool_mist(const LiquidPatch& pool, const wind::Climate& air, double hour, double sky);

// Every volume of the scene: the placed, then the lava's, then the slime's, then the air's over
// the pools (with `rows.air`), each with its sky share (a placed one's `sky` key kept).
std::vector<render::FogVolume> bsp_fog(const Scene& scene, const FogRows& rows = {});

// The BSP renderer's sun map (its first orthographic map) as the fog's casters.
render::OrthoShadow bsp_sun_map(const BspDraw& draw);

struct BspFogShadow {
  wgpu::Buffer params;
  wgpu::BindGroup group;  // render/shadows.hpp's layout: bind it with render::bind_fog_shadows
};
// The block and the group over `draw`'s maps (after create_bsp).
void create_bsp_fog_shadow(gpu::Device& device, BspFogShadow& shadow, const BspDraw& draw);
// The block for this frame's eye (after prepare_bsp).
void prepare_bsp_fog_shadow(gpu::Device& device, BspFogShadow& shadow, const BspDraw& draw,
                            const render::FrameView& view);

}  // namespace ge::brush::app
