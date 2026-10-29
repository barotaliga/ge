// OTHER WORLDS' MOONS' FACES: each kind's albedo over its whole sphere, as the sky pass draws it
// (render/wgsl/moons.wgsl), so Io's volcanoes, Europa's cracks, Ganymede's grooved lanes and
// Stickney on Phobos are where they are on the real moon.
//
// - THE MAPS are the spacecraft mosaics the planetary mapping programmes publish in the public
//   domain (render/moon_maps/SOURCES.md names each), cut to kMoonMapsWidth x kMoonMapsHeight:
//   east longitude -180..180 along a row (0 E the sub-planet meridian a locked moon turns toward
//   its planet), latitude 90 N..90 S down the rows, one byte of luminance a texel, scaled so its
//   mean over the sphere (each row weighted by the area it covers) is kMoonMapMean. Where a
//   mosaic has no picture (a hemisphere no spacecraft saw lit) the gap is filled from the ground
//   beside it, reflected about the edge of the data, so the seam is continuous.
// - THE COLOUR of a kind whose mosaic is in colour (Io, Ganymede, Triton): a smaller map of its
//   colour over its luminance, kMoonColourWidth x kMoonColourHeight, 128 a byte of 1 (so it only
//   colours); a kind without one is drawn in its tint (render/moons.hpp).
// - ON THE GPU: one R8Unorm array of every kind's map and one RGBA8Unorm array of their colours
//   (white for a kind with none), each layer a kind, with their whole mip chains (each level the
//   box filter of the one above).
//
// ge:summary Other moons' faces: real mosaics decoded, scaled, mipmapped and uploaded as arrays
// ge:keywords render moons moon-maps albedo mosaic texture-array mipmap io europa ganymede
// ge:keywords phobos titan triton create_moon_maps moon_kind_map moon_kind_colour
// ge:skill render
// ge:test tests/render_moons_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "gpu/device.hpp"

namespace ge::render {

inline constexpr int kMoonMapsWidth = 1024;
inline constexpr int kMoonMapsHeight = 512;
inline constexpr int kMoonColourWidth = 256;
inline constexpr int kMoonColourHeight = 128;
// A map's mean byte over the sphere: a byte over it is the albedo against the kind's mean.
inline constexpr int kMoonMapMean = 100;

// The kind's map (moon_kinds()'s order): kMoonMapsWidth x kMoonMapsHeight bytes from 90 N; empty
// when its PNG does not decode.
std::vector<uint8_t> moon_kind_map(int kind);
// The kind's colour: kMoonColourWidth x kMoonColourHeight texels of three bytes from 90 N; empty
// for a kind with none.
std::vector<uint8_t> moon_kind_colour(int kind);

struct MoonMaps {
  wgpu::Texture albedo, colour;
  wgpu::TextureView albedo_view, colour_view;  // 2D-array views, a layer a kind
};
MoonMaps create_moon_maps(gpu::Device& device);

}  // namespace ge::render
