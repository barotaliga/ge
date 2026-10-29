// SPRITES DRAWN (docs/rendering.md § Sprites): flat pictures stood in the world, any number of
// them, each one frame of a set of images, turned to the eye by one of five rules, cut out against
// the depth or blended over the picture, lit by the sky or by themselves.
//
// - THE FRAMES (`SpriteFrame`): an image (sRGB colour, alpha its coverage) and its ORIGIN, the
//   texel that stands at the sprite's position (a flame's foot, a ball's middle). Every frame a
//   scene draws is uploaded once (`create_sprites`), packed into one atlas (`pack_sprite_atlas`),
//   so a frame of the game writes nothing but its draws. An animation is a run of frames; which
//   one shows is the game's.
// - HOW A SPRITE TURNS (`SpriteFacing`), the five ways flat pictures are stood:
//   - `parallel`: in the screen's plane, turning with the camera (a ball of fire, a glow).
//   - `upright`: standing on the world's up, turned about it to face the eye's point (a torch's
//     flame, a bush): it never leans, however high the eye.
//   - `parallel_upright`: standing on the world's up, turned with the camera's heading.
//   - `oriented`: fixed in the world by the draw's own rotation (a sign, a mark on a wall).
//   - `parallel_oriented`: in the screen's plane, turned about the view by the draw's `roll`.
// - HOW IT IS LAID (`SpriteBlend`): `cutout` (alpha tested at a half, depth written, its own
//   motion for TAA, drawn first), `over` (premultiplied over the picture) or `add` (light added).
//   The blended two are drawn after, sorted far to near in one run, with no depth written and the
//   motion target's reactive channel set as particles do, so TAA does not smear them.
// - THE LIGHT: `glow` times the scene's white (fullbright: a flame, an explosion) plus `outdoor`
//   of the frame's sky and sun on the face turned to the eye plus the draw's own `ambient`, all
//   times the texel's colour and the draw's `colour`. The haze is applied, an added sprite's only
//   its fading (the air adds nothing to light it adds).
// - SHARP or SMOOTH per draw: the texel the point falls in (the classic look), or bilinear over
//   premultiplied texels so a cutout's edge never darkens.
//
// The CPU twin `sprite_basis` is what the shader's corners are made from (the shader does no
// turning: each draw's right and up are staged), for the tests and the CPU pictures.
//
// ge:summary Sprites drawn: camera-facing and oriented pictures, cutout or blended, one atlas
// ge:keywords render sprite sprites billboard quad facing parallel upright oriented atlas
// ge:keywords cutout alpha-test additive premultiplied glow fullbright frames animation
// ge:keywords create_sprites prepare_sprites encode_sprites sprite_basis pack_sprite_atlas
// ge:skill render
// ge:test tests/render_sprites_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"
#include "render/view.hpp"

namespace ge::render {

inline constexpr uint32_t kMaxSprites = 4096;  // drawn a frame
inline constexpr uint32_t kSpriteAtlasSize = 2048;

struct SpriteFrame {
  uint32_t width = 0, height = 0;
  std::vector<uint8_t> rgba;  // sRGB colour, alpha coverage, four a texel, rows from the top
  float origin[2] = {0, 0};   // the texel at the sprite's position: x right, y down
};

enum class SpriteFacing : uint8_t {
  parallel,
  upright,
  parallel_upright,
  oriented,
  parallel_oriented
};
enum class SpriteBlend : uint8_t { cutout, over, add };

struct SpriteDraw {
  uint32_t frame = 0;
  double position[3] = {0, 0, 0};  // world, m
  double last[3] = {0, 0, 0};      // where it was the frame before (cutouts' motion)
  SpriteFacing facing = SpriteFacing::parallel;
  float rotation[4] = {0, 0, 0, 1};  // x, y, z, w: `oriented`'s frame (the picture's right
                                     // along x, its up along y, its face toward -z)
  float roll = 0;                    // rad, `parallel_oriented`'s turn, right side down
  float scale = 0.01f;               // m a texel
  SpriteBlend blend = SpriteBlend::cutout;
  float colour[4] = {1, 1, 1, 1};  // linear rgb times the texels; a: the opacity
  float glow = 0;                  // its own light over the white of the scene
  float outdoor = 1;               // the share of the frame's sun and sky
  float ambient[3] = {0, 0, 0};    // a game's light where it stands
  bool smooth = true;
};

// Where each frame sits in the atlas, texels.
struct SpriteRect {
  uint32_t x = 0, y = 0, w = 0, h = 0;
};
// Shelf-pack the frames (tallest first) into a kSpriteAtlasSize square, a texel apart. False when
// they do not fit.
bool pack_sprite_atlas(std::span<const SpriteFrame> frames, std::vector<SpriteRect>& rects);

// The CPU twin: the world directions of a sprite's right and up (unit) for a camera whose own
// right, up and forward are given (camera_matrices' rows), the eye at `eye`.
void sprite_basis(const SpriteDraw& d, const double eye[3], const double cam_right[3],
                  const double cam_up[3], const double cam_fwd[3], double right[3], double up[3]);

struct Sprites {
  wgpu::Buffer frames, draws;
  wgpu::Texture atlas;
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup group;
  uint32_t p_cutout = 0, p_blend = 0;
  std::vector<SpriteRect> rects;
  std::vector<SpriteFrame> sizes;  // each frame's size and origin, no texels
  std::vector<uint8_t> staged;     // kMaxSprites GpuSprites
  std::vector<uint32_t> order;     // scratch: the blended draws, far to near
  std::vector<float> depth;        // scratch: their distances
  std::vector<uint32_t> sorted;    // scratch: the order of `order`, far to near
  uint32_t cutouts = 0, blended = 0;
  uint32_t dropped = 0;  // past kMaxSprites this frame
};

// Upload `frames` once. False when they do not fit the atlas.
bool create_sprites(gpu::Device& device, gpu::PipelineSet& pipelines, Sprites& sprites,
                    const wgpu::BindGroupLayout& frame, std::span<const SpriteFrame> frames);
// This frame's draws: turned, sorted and staged relative to `view`'s eye.
void prepare_sprites(gpu::Device& device, Sprites& sprites, std::span<const SpriteDraw> draws,
                     const FrameView& view);
// The draws over the targets: after the sky and the clouds, where the particles are drawn (the
// blended ones lie over what is behind them, the sky included).
void encode_sprites(const Sprites& sprites, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                    wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                    const wgpu::BindGroup& frame);

}  // namespace ge::render
