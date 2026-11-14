// THE MOON'S FACE: its albedo over the whole sphere, as the sky pass draws its disc
// (render/sky.wgsl's `moon_disc`), so the seas, the bright highlands and the rayed craters are
// where they are on the real moon, turned as core/moon.hpp's pole and prime meridian say.
//
// - THE MAP is a public-domain colour mosaic of the moon from a lunar orbiter's wide-angle
//   camera (normalised to one lighting, so it is the surface's albedo and not its shading), its
//   poles filled from the orbiter's laser altimeter's reflectance, cut to kMoonMapWidth x
//   kMoonMapHeight: selenographic longitude -180..180 east along a row, latitude 90 N..90 S down
//   the rows. It is stored as linear luminance over its brightest texel, one byte a texel.
// - THE SCALE (`moon_albedo_scale`): a byte over 255 times it is the albedo against the near
//   side's mean as a full moon shows it (each point weighted by the area it covers on the disc),
//   so the drawn disc keeps the phase law's light (render/night.hpp) to the texel.
// - THE TABLE is generated (render/moon_data.cpp, by the `moon` lab's --bake off a PNG of the
//   mosaic) and decoded once here; the GPU's copy carries its whole mip chain, each level
//   the box filter of the one above in linear light, read at the level the disc's size asks.
// - A WEB BUILD leaves the table out (a quarter of a viewer's download for a sky many matches
//   never show) and builds it as a file beside the viewer (`kMoonFaceFile`, cmake/Web.cmake). Until
//   it comes the face is the near side's mean everywhere, a plain disc with the phase's light; the
//   sky asks for it the first night the moon is up (`want_moon_face`), the viewer's loop takes
//   that ask (`take_moon_face_want`), fetches the file (`fetch_moon_face`, web builds only) and
//   sets it, and the sky uploads the new face before its next frame.
//
// ge:summary The moon's face: its albedo map decoded, its scale, its mipmapped texture, its bake
// ge:keywords render moon albedo moon-map moon-face selenographic mosaic mipmap bake
// ge:keywords moon_albedo moon_albedo_scale albedo_at create_moon_texture
// ge:keywords lazy-moon moon-face-file moon.data set_moon_face take_moon_face_want web-download
// ge:skill render
// ge:test tests/render_moon_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "gpu/device.hpp"

namespace ge::render {

inline constexpr int kMoonMapWidth = 1024;
inline constexpr int kMoonMapHeight = 512;

// The map's bytes, row-major from 90 N, decoded once from the generated table.
const std::vector<uint8_t>& moon_albedo();
// A byte over 255 times this is the albedo against the full near side's mean.
double moon_albedo_scale();

// ── The face fetched late (a web build) ──

// The file a web build writes beside its viewer: the table's base64 as one text.
inline constexpr const char* kMoonFaceFile = "moon.data";
// True while the face is the plain stand-in (a web build before its fetch).
bool moon_face_missing();
// Counts the faces set; a texture whose count differs uploads again.
uint32_t moon_face_generation();
// The sky's ask when the moon is up and the face is missing; nothing otherwise.
void want_moon_face();
// True once after an ask, for the caller to fetch the face; later calls answer false, so a fetch
// that fails is not tried again every frame.
bool take_moon_face_want();
// The face from the table's base64 (the file's text); false when it is not a whole map.
bool set_moon_face(std::string_view table);
// A web build's: when the sky asked for the face, fetch `kMoonFaceFile` from the hub's viewer
// folder and set it (a failed fetch keeps the plain disc and says why on the page's console).
void fetch_moon_face();
// The albedo (against the near side's mean) at selenographic `longitude` (east) and `latitude`,
// degrees, sampled bilinearly as the GPU samples its top level.
double albedo_at(double longitude, double latitude);

// The map on the GPU: R8Unorm, kMoonMapWidth x kMoonMapHeight and every mip level under it.
struct MoonTexture {
  wgpu::Texture texture;
  wgpu::TextureView view;
  wgpu::Queue queue;        // the device's, for a face set later
  uint32_t generation = 0;  // the face uploaded (moon_face_generation)
};
MoonTexture create_moon_texture(gpu::Device& device);
// The face as it is now, every level, into `t` (the sky calls it when the face's count moved).
void upload_moon_texture(MoonTexture& t);

// ── The table's bake (the `moon` lab) ──

// The linear luminance of an sRGB-encoded colour, each channel 0..1.
double linear_luminance(double r, double g, double b);
// The moon's albedo as linear luminance (equirectangular, twice as wide as high, its height a
// whole multiple of kMoonMapHeight) as the map's bytes and its scale; false for another size.
bool bake_moon_map(const std::vector<double>& linear, int width, int height,
                   std::vector<uint8_t>& bytes, double& scale);
// The map's bytes as the generated table's base64, and back.
std::string encode_moon_map(const std::vector<uint8_t>& bytes);
bool decode_moon_map(std::string_view base64, std::vector<uint8_t>& out);

}  // namespace ge::render
