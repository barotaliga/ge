// THE EYE'S ADAPTATION (docs/rendering.md § Post): the exposure follows what the eye sees, so a
// dark hall opens up over a few seconds and the daylight through its door dazzles, then closes
// again outside. The sky's exposure (frame.wgsl's `sky_exposure`) stays the eye AT REST: it is
// read off the sky's light, so it never pumps outdoors. This pass meters the picture and opens
// the eye past it only where the world is darker than the sky lights it (a room, a cave, the
// woods at dusk), by the stops the metered average sits under the key beyond a band of
// `tolerance`, at most `max_ev` (and, with a `min_ev` under zero, closes it where it is brighter).
//
// THE PASS ("eye", compute, after bloom and before post), two dispatches:
//   meter (eye.wgsl)        the post pass's input on a 128 x 72 grid of bilinear taps, at the
//                           sky's exposure, into a 64-bin log2 histogram (a quarter stop a bin
//                           from 2^-12), the middle of the picture weighed four times the corners
//   adapt (eye_adapt.wgsl)  the histogram averaged between the `low` and `high` shares of its
//                           weight and cleared; the eye stepped toward its target with a half-life
//                           of `to_dark_s` opening and `to_light_s` closing (the eye closes to
//                           light in a fraction of a second and opens to the dark over seconds)
//
// WHERE THE EYE IS KEPT: the fifth row of the sky's light buffer (frame.wgsl's SkyLight.eye),
// which every pass already binds, so the post pass, TAA-U's weights, the clouds' white and the
// moon as seen all read the same adapted exposure with no new binding. The GPU keeps it from
// frame to frame and never reads it back in a frame; the buffer starts zeroed, which is the eye
// at rest, so a frame that never runs this pass keeps its bits.
//
// ge:summary The eye's adaptation: the frame metered into a log histogram, the exposure opened
// over seconds where the world is darker than the sky lights it
// ge:keywords render eye eye-adaptation auto-exposure exposure histogram luminance meter hdr
// ge:keywords adapt dark-adaptation light-adaptation half-life dazzle create_eye eye_group
// ge:keywords prepare_eye encode_eye read_eye eye-settings key tolerance
// ge:skill render
// ge:test tests/render_eye_test.cpp
#pragma once

#include <cstdint>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"
#include "render/sky.hpp"

namespace ge::render {

struct EyeSettings {
  // The average the eye adapts the picture to, as the tone curve's input (a white surface under
  // the sky's own light is 0.78); a darker average opens the eye.
  double key = 0.18;
  double tolerance_ev = 1.0;  // stops either side of the key the eye leaves alone
  double min_ev = 0.0;        // the most it closes past the sky's exposure (<= 0)
  double max_ev = 4.0;        // the most it opens past the sky's exposure
  double strength = 0.85;     // the share of the stops past the band it makes up
  double to_light_s = 0.35;   // half-life closing toward a brighter world, s
  double to_dark_s = 2.5;     // half-life opening toward a darker one, s
  double low = 0.4;           // the share of the metered weight left out at the dark end
  double high = 0.9;          // the share at which the bright end is cut off
};

struct Eye {
  EyeSettings settings;
  wgpu::BindGroupLayout meter_layout, adapt_layout;
  wgpu::Buffer bins;  // 64 u32, cleared by each adapt
  wgpu::Buffer step;  // the adapt kernel's uniform (eye_adapt.wgsl's EyeStep)
  wgpu::Sampler sampler;
  wgpu::BindGroup adapt;
  uint32_t p_meter = 0, p_adapt = 0;
  bool cut = true;  // the next step jumps straight to its target
};

// The pipelines (added to the manifest), the histogram and the adapt group over the sky's light.
void create_eye(gpu::Device& device, gpu::PipelineSet& pipelines, Eye& eye,
                const wgpu::BindGroupLayout& frame, const Sky& sky,
                const EyeSettings& settings = {});
// The meter's group over the frame it reads (the post pass's input), made once a set of targets.
wgpu::BindGroup eye_group(gpu::Device& device, const Eye& eye, const wgpu::TextureView& hdr);
// The step's uniform for a frame `dt` s after the last; a cut (`Eye::cut`, set by `cut_eye`)
// jumps to the target and clears itself.
void prepare_eye(gpu::Device& device, Eye& eye, double dt);
// The next frame adapts at once (a camera cut, a teleport, a level loaded).
inline void cut_eye(Eye& eye) { eye.cut = true; }
// The pass ("eye"): meter `group`'s frame, then step the eye.
void encode_eye(const Eye& eye, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                wgpu::CommandEncoder& encoder, const wgpu::BindGroup& frame,
                const wgpu::BindGroup& group);

// What the eye holds after the last pass, read back blocking: for tests and labs, never in a frame.
struct EyeReading {
  double open_ev = 0;    // the stops it has opened past the sky's exposure
  double target_ev = 0;  // where it is stepping to
  double average = 0;    // the metered average, log2 at the sky's exposure
  double weight = 0;     // the weight metered (0: never metered)
};
EyeReading read_eye(gpu::Device& device, const Sky& sky);

}  // namespace ge::render
