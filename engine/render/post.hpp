// THE POST PASS (docs/rendering.md § Post): the frame's HDR target to the output (an offscreen
// target or the surface's texture) in one full-screen pass: the bloom mixed in (render/bloom.hpp),
// the exposure read off the sky's light (frame.wgsl's `exposure`), the tone curve, the colour
// grade, a dither and the output encoding. It reads the frame at the output's size: TAA-U's
// resolve (render/taa.hpp) or, without it, the HDR target itself.
//
// THE GRADE is a kGradeSize³ lookup table the game bakes from its look (`set_grade`): an
// sRGB-encoded colour in, the graded sRGB-encoded colour out, read trilinearly after the tone
// curve, so it works where the eye judges colour and its 32 steps a channel are evenly spent.
// It starts as the identity (`identity_grade`).
//
// THE LENS AND THE FILM (M35a): what a camera does to the picture past the scene, each off at 0
// and each its own `override` switch, so a game that leaves one off compiles it out and pays
// nothing; every one is folded into this one pass (no full-screen pass of its own):
//
//   sharpen    contrast-adaptive sharpening over the four neighbours, strongest where the
//              picture is flat (TAA-U's softness), held back at edges so nothing rings
//   chromatic  the lens's lateral colour: red and blue pulled apart toward the corners
//   vignette   the corners darker, a cos⁴-like fall-off
//   grain      luminance-weighted film grain, new every frame, finest in the middle tones
//   dirt       the lens's dirt lit by the glow (the bloom and the lens pass): smudges show only
//              where something bright is in the picture
//   motion blur  the picture smeared along the motion target for the shutter's share of a
//              frame (M35c): a gather of kBlurTaps taps along the fastest motion of the
//              pixel's tile and its neighbours (render/motion_blur.hpp), each weighted by
//              whether its own motion reaches the pixel and which stands in front, so a figure
//              the camera follows stays sharp; a still tile costs one load
//
// The STRENGTHS are a small uniform (`set_post_look`), so a strength turns live; whether each
// is COMPILED is decided when the pipeline is made (PostSettings::look's strength above 0), so
// turning one on from 0 means making the pass again (Fall Line's frame is remade). The lens
// pass's light (render/lens.hpp: the sun's rays through what stands, the flare's ghosts) is
// mixed in here before the exposure, one bilinear tap of a quarter-size target.
//
// A fragment pass rather than a compute one: it writes the surface's texture, which a compute
// shader may not store to without an optional feature (bgra8unorm storage), and a tile GPU
// resolves a full-screen fragment pass in tile memory.
//
// ge:summary The post pass: bloom mixed in, exposure from the sky, tone curve, the colour grade's
// lookup table, dither, output encoding
// ge:keywords render post exposure tone-mapping tonemap pbr-neutral dither srgb encode output
// ge:keywords bloom colour-grade color-grade lut lookup-table 3d-lut grade
// ge:keywords create_post post_group encode_post set_grade identity_grade set_post_look
// ge:keywords sharpen cas contrast-adaptive-sharpening chromatic-aberration vignette film-grain
// ge:keywords lens-dirt post-look lens-effects motion-blur shutter post-motion
// ge:keywords screen-blend set_post_blend tint damage-flash
// ge:skill render
// ge:test tests/render_post_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/dof.hpp"
#include "render/frame.hpp"

namespace ge::render {

inline constexpr uint32_t kGradeSize = 32;  // the grade's lookup table, a side

// The lens's and the film's strengths, each 0..1 (0: off). What each looks like at 1 is stated
// beside it; a game's defaults are its own (Fall Line's: cfg/cvars.hpp).
struct PostLook {
  double sharpen = 0;      // 1: CAS at its strongest (a flat area's detail about doubled)
  double chromatic = 0;    // 1: red and blue 0.5 % of the diagonal apart at the corners
  double vignette = 0;     // 1: the corners at 40 % of the centre's light
  double grain = 0;        // 1: grain of about 4 % of the middle tones' level
  double dirt = 0;         // 1: the dirt doubles the glow where it lies
  double motion_blur = 0;  // the shutter's share of a frame: 0.5 a 180-degree film shutter
};

struct PostSettings {
  double bloom = 0;   // the share of the bloom's glow mixed in (BloomSettings::strength); 0: none
  PostLook look;      // which of the lens and film are compiled (above 0), and their start
  bool lens = false;  // mix in the lens pass's target (render/lens.hpp)
  bool dof = false;   // mix in depth of field's gather (render/dof.hpp)
};

struct Post {
  PostSettings settings;
  wgpu::BindGroupLayout layout;
  wgpu::TextureFormat output = wgpu::TextureFormat::RGBA8Unorm;
  wgpu::Sampler sampler;
  wgpu::Texture grade;
  wgpu::TextureView grade_view;
  wgpu::Texture black;  // 1 x 1: the bloom the pass reads when there is none
  wgpu::TextureView black_view;
  wgpu::Texture dirt;  // the lens's dirt (`lens_dirt`), or black when the dirt is off
  wgpu::TextureView dirt_view;
  wgpu::Texture flat;  // 1 x 1 depth: the depth the pass reads when nothing blurs
  wgpu::TextureView flat_view;
  wgpu::Buffer look;  // PostLook's strengths, as the shader reads them
  uint32_t p_draw = 0;
};

// Add the post pipeline for an output of `format` to the manifest; the grade starts as the
// identity.
void create_post(gpu::Device& device, gpu::PipelineSet& pipelines, Post& post,
                 const wgpu::BindGroupLayout& frame, wgpu::TextureFormat output,
                 const PostSettings& settings = {});
// What motion blur and depth of field read: at the render size the motion and depth targets and
// the tiles' neighbours' fastest (render/motion_blur.hpp's `near_view`), and depth of field's
// half-size gather (render/dof.hpp's `view`). None: neither.
struct PostReads {
  wgpu::TextureView motion, depth, near, dof;
};

// The pass's own group over the frame it reads (an output-size HDR view: TAA-U's output, or the
// HDR target when it renders at the output's size), the bloom's top level, the lens pass's
// target and what motion blur and depth of field read (null: none), made once a set of targets.
wgpu::BindGroup post_group(gpu::Device& device, const Post& post, const wgpu::TextureView& hdr,
                           const wgpu::TextureView& bloom = nullptr,
                           const wgpu::TextureView& lens = nullptr, const PostReads& reads = {});
// The strengths, live; a feature not compiled at create_post stays off whatever it is given.
void set_post_look(gpu::Device& device, const Post& post, const PostLook& look);
// THE SCREEN'S BLEND: the whole picture mixed toward one colour after the grade (a game's flash
// when its player is hurt, the tint of a liquid round the eye): `rgba` an sRGB-encoded colour and
// its share, 0..1; a share of 0 (the start) leaves the picture as it is.
void set_post_blend(gpu::Device& device, const Post& post, const double rgba[4]);
// Depth of field's focus, sharp share and strength (render/dof.hpp's), live: the post pass mixes
// the gather in by each pixel's own circle under the same lens the dof pass gathered with.
void set_post_focus(gpu::Device& device, const Post& post, const DofSettings& dof);

// The lens's dirt: a w x h RGBA8 picture of soft smudges, rings and a few streaks, the same for
// a seed on every machine (labs and tests read it).
std::vector<uint8_t> lens_dirt(uint32_t w, uint32_t h, uint32_t seed = 1);
// The pass ("post"): the frame into `output` (cleared, then every pixel written).
void encode_post(const Post& post, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                 wgpu::CommandEncoder& encoder, const wgpu::BindGroup& frame,
                 const wgpu::BindGroup& group, const wgpu::TextureView& output);

// The grade's table: kGradeSize³ RGB triples, red fastest, then green, then blue; entry (r, g, b)
// is what the sRGB-encoded colour (r, g, b) / (kGradeSize - 1) becomes.
std::vector<float> identity_grade();
// Upload a table (identity_grade's layout); any group made since reads it.
void set_grade(gpu::Device& device, Post& post, std::span<const float> table);

}  // namespace ge::render
