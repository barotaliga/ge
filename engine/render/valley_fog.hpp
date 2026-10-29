// THE SEA OF FOG IN THE VALLEYS (docs/rendering.md § The valley fog): the fog that fills the
// valleys up to a level top on a still morning, seen from a peak as a white sea with the ridges
// standing out of it as islands, and from under its top as a grey gloom. It is drawn per pixel at
// any range, so its edge on the ground is as sharp at 10 km as at 10 m; the froxels of
// render/fog.hpp are the fog round the eye, this is the fog out to the horizon.
//
// - THE MEDIUM: an even extinction `density` everywhere under the top, thinning to none over
//   `edge_m` above it (a linear ramp). Along a ray its optical depth is closed-form: the integral
//   of the share above a height, G(y), is 0 over the ramp, a square across it and a line under
//   it, so the depth between two heights is density x |G(a) - G(b)| / |the ray's slope|
//   (`valley_depth`). No march, no texture: a few instructions a pixel.
// - THE TOP waves: it rises and falls `wave_m` on two octaves of the cloud's tiled noise
//   (frame.wgsl's `cloud_noise`), `wave_size_m` across, read where the ray crosses the top and
//   drifting on `drift`.
// - THE LIGHT: a white face under this sun and sky, its `albedo` of it: the sun on the top's
//   billows (the first octave's slope, laid level with range before it would flicker) past the
//   cloud's shadow where the top is, and the sky over them; dimmed under the top by how deep the
//   eye is in it, to kValleyGloom at the deepest; seen through the fog's own transmittance:
//   colour x exp(-depth) + light x (1 - exp(-depth)).
//
// wgsl/valley_fog.wgsl is the GPU's half, called by frame.wgsl's `haze` (before the air's haze,
// so a far sea is veiled by the air in front of it), by the sky and by the cloud. With `density`
// 0 nothing reads it and a frame draws the bits it drew before.
//
// ge:summary The sea of fog in the valleys: a level, wavy, lit top,
// ge:summary its depth along a ray in closed form
// ge:keywords render valley-fog sea-of-fog inversion fog-top optical-depth analytic closed-form
// ge:keywords valley_rows valley_share valley_column valley_depth gloom
// ge:skill render
// ge:test tests/render_valley_fog_test.cpp
#pragma once

#include "render/view.hpp"

namespace ge::render {

// The light under the deepest fog against the light on its top.
inline constexpr double kValleyGloom = 0.3;

// The frame block's two rows (render/frame.hpp's `FrameUniforms::valley`): the top over the eye,
// the extinction, the edge, the wave; one over the noise's tile, the noise's offset at the eye (its
// share of the tile, so it stays small at any eye and time), the albedo. All zero with no fog.
void valley_rows(const ValleyFog& fog, const FrameView& view, float rows[2][4]);

// The share of the full density at `y` m over a top at `top` (the same frame): 1 under top -
// edge / 2, 0 over top + edge / 2, linear between.
double valley_share(double top, double edge, double y);

// The integral of the share from `y` up to infinity, m.
double valley_column(double top, double edge, double y);

// The optical depth along a ray from height 0 (the eye) with slope `dy` (the unit direction's y)
// out to `dist` m, through a fog of extinction `density` under a top `top` m over the eye.
double valley_depth(double top, double edge, double density, double dy, double dist);

}  // namespace ge::render
