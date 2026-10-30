// LAB `gpu-flames`: FLAMES DRAWN AND THE LIGHT THEY CAST (render/flame.hpp), on the software
// adapter. A stone hall at night lit only by fire: four wall torches on iron brackets, a brazier,
// three candles on a table; seen with the flames' lights off and on; a torch close up over half a
// second; the four kinds (a candle, a torch, a brazier, a camp fire) side by side; a torch in
// still air, a breeze and a draught; and the blackbody's colours the gas is painted with. It
// holds a flame behind a wall to adding nothing (the march stops at the depth), a flame's light
// to lighting the hall and the gas to burning warm and bright, and fails on a GPU error or a
// pipeline made after load.
//
// ge:summary Lab: flames drawn as gas and their light: a torch-lit hall, kinds, wind, blackbody
// ge:keywords lab gpu flames fire torch candle brazier campfire blackbody flicker wind hall
// ge:keywords flame-light filmstrip occlusion
// ge:skill render labs
// ge:test tests/render_flame_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "gpu/device.hpp"
#include "lab/lab.hpp"
#include "lab/labs/lit_scene.hpp"
#include "lab/labs/water_scene.hpp"
#include "lab/raster.hpp"
#include "render/flame.hpp"
#include "render/lights.hpp"
#include "render/surface.hpp"

namespace {

namespace gl = ge::lab;
namespace gr = ge::render;
namespace libm = ge::core::libm;
using ge::lab::lit_scene::at;
using ge::lab::lit_scene::draw;
using ge::lab::lit_scene::kCx;
using ge::lab::lit_scene::kCz;
using ge::lab::lit_scene::Scene;
using ge::lab::water_scene::fixed;
using ge::lab::water_scene::gpu_required;
using ge::lab::water_scene::paste;
using ge::lab::water_scene::placeholders;
using gl::Det;

enum Mat : uint8_t { kStone, kIron, kWood, kPitch, kWax, kWetFloor, kEmber, kAsh };

std::vector<gr::Surface> surfaces() {
  const float stone[3] = {0.34f, 0.31f, 0.27f}, iron[3] = {0.35f, 0.33f, 0.31f};
  const float wood[3] = {0.28f, 0.16f, 0.08f}, pitch[3] = {0.05f, 0.04f, 0.035f};
  const float wax[3] = {0.85f, 0.82f, 0.72f}, floor[3] = {0.10f, 0.10f, 0.11f};
  const float ember[3] = {1.0f, 0.32f, 0.06f}, ash[3] = {0.12f, 0.11f, 0.10f};
  return {gr::stone_surface(stone),     gr::metal_surface(iron, 0.55f),
          gr::wood_surface(wood),       gr::matte_surface(pitch),
          gr::matte_surface(wax),       gr::wet_floor_surface(floor),
          gr::lamp_surface(ember, 120), gr::stone_surface(ash)};
}

// Where the fires burn.
constexpr double kTorchX = 5.75, kTorchY = 2.47;
constexpr double kTorchZ[2] = {4.0, 8.5};
const double kBrazier[3] = {kCx + 1.5, 1.08, kCz + 8.0};
const double kTable[3] = {kCx - 2.2, 0.9, kCz + 4.5};
constexpr double kCandleDx[3] = {-0.45, 0.0, 0.38};
constexpr double kCandleH[3] = {0.18, 0.24, 0.14};
const double kCamp[3] = {kCx + 30, 0.12, kCz + 30};
const double kPost[3] = {kCx - 30, 0, kCz + 30};  // the torch in the wind, on its post

std::vector<gr::Instance> hall() {
  std::vector<gr::Instance> v;
  v.push_back(at(kCx, 0, kCz + 12.5, 14, 6, 1, 0, kStone));     // the back wall
  v.push_back(at(kCx - 6.5, 0, kCz + 6, 1, 6, 13, 0, kStone));  // the side walls
  v.push_back(at(kCx + 6.5, 0, kCz + 6, 1, 6, 13, 0, kStone));
  v.push_back(at(kCx, 6, kCz + 6, 14, 0.5f, 13, 0, kStone));  // the ceiling
  for (int side = -1; side <= 1; side += 2) {
    for (double z : kTorchZ) {
      const double x = kCx + side * kTorchX;
      v.push_back(at(kCx + side * 5.88, 1.96, kCz + z, 0.26f, 0.04f, 0.05f, 0, kIron));  // bracket
      v.push_back(at(x, 1.9, kCz + z, 0.07f, 0.45f, 0.07f, 2, kWood));                   // handle
      v.push_back(at(x, 2.33, kCz + z, 0.11f, 0.14f, 0.11f, 2, kPitch));                 // its head
    }
  }
  // The brazier: a post, a bowl and its coals.
  v.push_back(at(kBrazier[0], 0, kBrazier[2], 0.12f, 0.9f, 0.12f, 2, kIron));
  v.push_back(at(kBrazier[0], 0.9, kBrazier[2], 0.7f, 0.18f, 0.7f, 2, kIron));
  v.push_back(at(kBrazier[0], 1.06, kBrazier[2], 0.3f, 0.05f, 0.3f, 1, kEmber));
  // The table and its candles.
  v.push_back(at(kTable[0], 0, kTable[2], 1.6f, 0.9f, 0.8f, 0, kWood));
  for (int i = 0; i < 3; ++i) {
    v.push_back(at(kTable[0] + kCandleDx[i], kTable[1], kTable[2], 0.05f,
                   static_cast<float>(kCandleH[i]), 0.05f, 2, kWax));
  }
  // The wet flags in the middle of the floor.
  v.push_back(at(kCx, -0.03, kCz + 6.3, 3.4f, 0.04f, 2.6f, 0, kWetFloor));
  // Outside: the camp fire's ring of stones and its logs, and the torch on its post.
  for (int i = 0; i < 9; ++i) {
    const double a = i * 2 * 3.14159265 / 9;
    v.push_back(at(kCamp[0] + 0.75 * libm::cos(a), 0, kCamp[2] + 0.75 * libm::sin(a), 0.22f, 0.16f,
                   0.2f, 1, kAsh));
  }
  gr::Instance log = at(kCamp[0], 0.1, kCamp[2] - 0.5, 0.16f, 1.0f, 0.16f, 2, kWood);
  // Tipped 86 degrees about x: a log lying on the flags.
  const auto s = static_cast<float>(libm::sin(0.75)), c = static_cast<float>(libm::cos(0.75));
  log.rotation[0] = s;
  log.rotation[3] = c;
  v.push_back(log);
  gr::Instance log2 = log;
  log2.position[2] = kCamp[2] + 0.5;
  log2.rotation[0] = -s;
  v.push_back(log2);
  v.push_back(at(kCamp[0], 0, kCamp[2], 0.55f, 0.08f, 0.55f, 1, kEmber));
  v.push_back(at(kPost[0], 0, kPost[2], 0.07f, 2.33f, 0.07f, 2, kWood));
  v.push_back(at(kPost[0], 2.33, kPost[2], 0.11f, 0.14f, 0.11f, 2, kPitch));
  return v;
}

std::vector<gr::Flame> fires() {
  std::vector<gr::Flame> v;
  uint32_t seed = 1;
  for (int side = -1; side <= 1; side += 2) {
    for (double z : kTorchZ) {
      const double base[3] = {kCx + side * kTorchX, kTorchY, kCz + z};
      v.push_back(gr::torch_flame(base, seed++));
    }
  }
  v.push_back(gr::brazier_flame(kBrazier, seed++));
  for (int i = 0; i < 3; ++i) {
    const double base[3] = {kTable[0] + kCandleDx[i], kTable[1] + kCandleH[i] + 0.006, kTable[2]};
    v.push_back(gr::candle_flame(base, seed++));
  }
  v.push_back(gr::campfire_flame(kCamp, seed++));
  return v;
}

// The torch on its post, in the air moving along +x at `wind` m/s.
gr::Flame post_torch(double wind) {
  const double base[3] = {kPost[0], kPost[1] + 2.47, kPost[2]};
  gr::Flame f = gr::torch_flame(base, 99);
  f.wind[0] = static_cast<float>(wind);
  return f;
}

std::vector<gr::LocalLight> lights_of(const std::vector<gr::Flame>& flames) {
  std::vector<gr::LocalLight> v;
  v.reserve(flames.size());
  for (const gr::Flame& f : flames) v.push_back(gr::flame_light(f));
  return v;
}

// A view from `eye` toward `target`.
gr::FrameView look(const double eye[3], const double target[3], double t) {
  const double dx = target[0] - eye[0], dy = target[1] - eye[1], dz = target[2] - eye[2];
  const double heading = libm::atan2(dx, dz), pitch = libm::atan2(dy, ge::core::hypot(dx, dz));
  return ge::lab::lit_scene::view_at(eye[0], eye[1], eye[2], heading, pitch, true, t);
}

// An eye at night never quite adapts to firelight: it settles as if the room were this much
// brighter than it is, so a fire-lit room reads dim and its fires bright.
constexpr double kNightKey = 3;

// The light an eye adapts to round `points`: the geometric mean of the lamps' on a white face.
double adapted(const std::vector<gr::LocalLight>& lamps,
               const std::vector<std::vector<double>>& pts, double t) {
  double e = 0;
  for (const auto& p : pts) e += libm::log(std::max(gr::lamp_lux_at(lamps, p.data(), t), 1e-3));
  return kNightKey * libm::exp(e / static_cast<double>(pts.size()));
}

gl::Picture half(const gl::Picture& p) {
  gl::Picture o = gl::create_picture(p.width / 2, p.height / 2, {0, 0, 0});
  const auto ow = static_cast<size_t>(o.width), pw = static_cast<size_t>(p.width);
  for (size_t y = 0; y < static_cast<size_t>(o.height); ++y) {
    for (size_t x = 0; x < ow; ++x) {
      for (size_t c = 0; c < 4; ++c) {
        int sum = 0;
        for (size_t q = 0; q < 4; ++q)
          sum += p.rgba[((y * 2 + q / 2) * pw + x * 2 + q % 2) * 4 + c];
        o.rgba[(y * ow + x) * 4 + c] = static_cast<uint8_t>(sum / 4);
      }
    }
  }
  return o;
}

double luma_at(const gl::Picture& p, size_t i) {
  return 0.2126 * p.rgba[i] + 0.7152 * p.rgba[i + 1] + 0.0722 * p.rgba[i + 2];
}

uint8_t srgb8(double v) {
  v = std::clamp(v, 0.0, 1.0);
  const double e = v <= 0.0031308 ? 12.92 * v : 1.055 * libm::pow(v, 1 / 2.4) - 0.055;
  return static_cast<uint8_t>(std::lround(e * 255));
}

// THE BLACKBODY: the table's colours across, each at full brightness (top) and at its brightness
// beside a torch's hottest gas (bottom), the torch's span of temperatures marked.
void blackbody_sheet(gl::Lab& lab, uint32_t w, uint32_t h) {
  gl::Picture p = gl::create_picture(static_cast<int>(w), static_cast<int>(h), {18, 18, 22});
  const int x0 = 40, x1 = static_cast<int>(w) - 40;
  const int band = static_cast<int>(h) / 4;
  const double torch = gr::blackbody(1900).luminance;
  for (int x = x0; x < x1; ++x) {
    const double k =
        gr::kBlackbodyLoK + (gr::kBlackbodyHiK - gr::kBlackbodyLoK) * (x - x0) / double(x1 - x0);
    const gr::Blackbody b = gr::blackbody(k);
    const double peak = std::max({b.rgb[0], b.rgb[1], b.rgb[2], 1e-6f});
    const double dim = libm::pow(std::min(b.luminance / torch, 1.0), 0.45);
    const gl::Rgba top{srgb8(b.rgb[0] / peak), srgb8(b.rgb[1] / peak), srgb8(b.rgb[2] / peak)};
    const gl::Rgba bot{srgb8(b.rgb[0] / peak * dim), srgb8(b.rgb[1] / peak * dim),
                       srgb8(b.rgb[2] / peak * dim)};
    gl::fill_rect(p, x, band, 1, band, top);
    gl::fill_rect(p, x, band * 2 + 8, 1, band, bot);
  }
  for (int k = 1000; k <= 3000; k += 500) {
    const double x =
        x0 + (x1 - x0) * (k - gr::kBlackbodyLoK) / (gr::kBlackbodyHiK - gr::kBlackbodyLoK);
    gl::draw_line(p, x, band - 6, x, band * 3 + 14, {200, 200, 200});
    gl::draw_text(p, x - 12, band - 18, std::to_string(k) + " K", {220, 220, 220});
  }
  // The torch's gas: 55 % to 100 % of 1900 K.
  const auto xk = [&](double k) {
    return x0 + (x1 - x0) * (k - gr::kBlackbodyLoK) / (gr::kBlackbodyHiK - gr::kBlackbodyLoK);
  };
  gl::fill_rect(p, xk(0.55 * 1900 * 0.75), band * 3 + 20, xk(1900) - xk(0.55 * 1900 * 0.75), 4,
                {255, 170, 60});
  gl::draw_text(p, xk(0.55 * 1900 * 0.75), band * 3 + 30, "A TORCH'S GAS, RIM AND TIP TO CORE",
                {255, 190, 110});
  lab.shot({"blackbody",
            "THE COLOURS THE GAS IS PAINTED WITH",
            "Planck's law integrated against the CIE 1931 observer, 700 to 3200 K "
            "(render/blackbody.hpp's table). Top: each temperature's colour at full "
            "brightness. Bottom: as bright as it burns beside a torch's hottest gas (1900 K), "
            "the visible radiance falling five orders of magnitude to 1000 K, softened to the "
            "eye's 0.45 power as the shader does. The orange bar: the span a torch's gas covers.",
            "deep red at the left through orange and yellow to a warm white at the right; the "
            "bottom strip dark until about 1300 K and glowing from there up",
            {}},
           p, Det::measured);
}

void checked(gl::Lab& lab, int errors, int late) {
  lab.metric("gpu-errors", errors, "count");
  lab.metric("pipelines-after-load", late, "count");
  if (errors != 0 || late != 0) {
    lab.fail(std::to_string(errors) + " GPU errors, " + std::to_string(late) +
                 " pipelines after the load",
             "Read the gpu: lines above; add any pipeline to the scene's manifest "
             "(engine/lab/labs/lit_scene.hpp). Skill: render.");
  }
}

void run(gl::Lab& lab) {
  const auto w = static_cast<uint32_t>(lab.arg_num("width"));
  const uint32_t h = w * 9 / 16;
  blackbody_sheet(lab, w, h);

  // The torch's numbers, from the CPU: what a game puts in its lights list.
  const double origin[3] = {0, 0, 0};
  const gr::LocalLight tl = gr::flame_light(gr::torch_flame(origin, 1));
  lab.metric("torch.candela", tl.candela, "cd");
  lab.metric("torch.range", tl.range_m, "m");
  lab.metric("candle.candela", gr::flame_light(gr::candle_flame(origin, 1)).candela, "cd");
  lab.metric("campfire.candela", gr::flame_light(gr::campfire_flame(origin, 1)).candela, "cd");

  const std::vector<std::string> ids = {"hall-dark", "hall-lit", "torch-close", "kinds", "wind"};
  auto device = ge::gpu::create_device({.label = "gpu-flames", .validation = 1});
  if (!device) {
    if (gpu_required()) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    placeholders(lab, ids, w, h);
    return;
  }
  ge::gpu::Device& d = *device;
  Scene s;
  const std::vector<gr::Surface> surf = surfaces();
  if (!ge::lab::lit_scene::make_scene(d, s, surf, hall(), w, h, "gpu-flames")) {
    lab.fail("the scene's pipelines did not build", "Read the gpu: lines above. Skill: render.");
    return;
  }

  const double t = 2.0;
  const std::vector<gr::Flame> burning = fires();
  const std::vector<gr::LocalLight> lamps = lights_of(burning);
  const double eye[3] = {kCx, 1.65, kCz - 1.0}, ahead[3] = {kCx, 1.5, kCz + 10};
  gr::FrameView v = look(eye, ahead, t);
  v.lamp_lux = adapted(lamps,
                       {{kCx - 5, 1.5, kCz + 6},
                        {kCx + 5, 1.5, kCz + 6},
                        {kCx, 0.2, kCz + 5},
                        {kCx, 2.0, kCz + 12},
                        {kCx + 1.5, 0.3, kCz + 7},
                        {kTable[0], 0.95, kTable[2] - 0.5}},
                       t);
  lab.metric("hall.lamp-lux", v.lamp_lux, "lx", Det::measured);
  const gl::Picture dark = draw(d, s, v, {}, burning);
  const gl::Picture lit = draw(d, s, v, lamps, burning);
  lab.metric("hall.lights", s.lights.bins.head.info[0], "count", Det::measured);
  lab.metric("hall.flames", s.flames.count, "count", Det::measured);
  const std::vector<std::pair<std::string, std::string>> inputs = {
      {"night", "moonless, sun 20 deg down"}, {"t", "2 s"}};
  lab.shot({"hall-dark", "THE HALL'S FLAMES WITHOUT THEIR LIGHT",
            "A stone hall at the bottom of a moonless night: four torches on iron brackets, a "
            "brazier and three candles on a table, the gas drawn (render/flame.hpp) but the "
            "lights they cast left out, at the exposure of an eye adapted to the lit hall.",
            "the flames floating in a black room: yellow-white cores, orange rims, red tongues "
            "at their tips, the candles' small blue bases; nothing round them lit",
            inputs},
           dark, Det::measured);
  lab.shot({"hall-lit", "THE HALL BY FIRELIGHT",
            "The same frame with each flame's own light (flame_light: at the gas's centre, the "
            "colour of its mean temperature, its candela its luminance over its area, flickering "
            "with the flame's seed) in the clustered lights (render/lights.hpp).",
            "warm pools on the walls round each torch falling off by the inverse square, the "
            "brazier lighting the floor and the wet flags mirroring it, the table and its "
            "candles in a small glow; brighter where the fires are near and no darker anywhere",
            inputs},
           lit, Det::measured);
  // Lit is never darker, and brighter round the torches.
  {
    double gain = 0;
    int darker = 0;
    for (size_t i = 0; i < lit.rgba.size(); i += 4) {
      const double a = luma_at(lit, i), b = luma_at(dark, i);
      gain += a - b;
      // The flames themselves excepted: the tone curve may shift a bright flame's hue a level.
      if (a + 2 < b && b < 160) ++darker;
    }
    gain /= static_cast<double>(lit.rgba.size()) / 4.0;
    lab.metric("hall.mean-gain", gain, "levels", Det::measured);
    lab.metric("hall.darker-pixels", darker, "count", Det::measured);
    if (gain < 3 || darker > 0) {
      lab.fail("the flames' light did not lighten the hall (mean gain " + fixed(gain, 1) + ", " +
                   std::to_string(darker) + " pixels darker)",
               "Check flame_light's candela and that the lights reach the hall "
               "(render/flame.cpp). Skill: render.");
    }
  }

  // A flame behind the back wall adds nothing: the march stops at the depth.
  {
    std::vector<gr::Flame> hidden = burning;
    const double behind[3] = {kCx, 1.0, kCz + 14};
    hidden.push_back(gr::brazier_flame(behind, 500));
    const gl::Picture with = draw(d, s, v, lamps, hidden);
    const gl::Picture without = draw(d, s, v, lamps, burning);
    int differ = 0;
    for (size_t i = 0; i < with.rgba.size(); ++i) differ += with.rgba[i] != without.rgba[i] ? 1 : 0;
    lab.metric("occluded.differing-bytes", differ, "count", Det::measured);
    if (differ != 0) {
      lab.fail("a flame behind the wall changed " + std::to_string(differ) + " bytes",
               "The march must stop at the scene's depth (render/flame.wgsl's fs_main). "
               "Skill: render.");
    }
  }

  // A torch close up, over half a second.
  {
    const double torch[3] = {kCx - kTorchX, kTorchY + 0.15, kCz + kTorchZ[1]};
    const double near[3] = {kCx - kTorchX + 0.6, kTorchY + 0.1, kCz + kTorchZ[1] - 0.45};
    gl::Picture sheet = gl::create_picture(static_cast<int>(w) * 2, static_cast<int>(h), {0, 0, 0});
    double hottest = 0, warm = 0;
    for (int k = 0; k < 8; ++k) {
      const double tk = t + k / 15.0;
      gr::FrameView c = look(near, torch, tk);
      const double wall[3] = {kCx - 5.95, kTorchY + 0.2, kCz + kTorchZ[1]};
      const double floor[3] = {kCx - 5.2, 0.1, kCz + kTorchZ[1]};
      c.lamp_lux =
          kNightKey * std::sqrt(gr::lamp_lux_at(lamps, wall, t) * gr::lamp_lux_at(lamps, floor, t));
      const gl::Picture p = draw(d, s, c, lamps, burning);
      for (size_t i = 0; i < p.rgba.size(); i += 4) {
        const double l = luma_at(p, i);
        if (l > hottest) {
          hottest = l;
          warm = static_cast<double>(p.rgba[i]) - p.rgba[i + 2];
        }
      }
      paste(sheet, half(p), (k % 4) * static_cast<int>(w) / 2, (k / 4) * static_cast<int>(h) / 2);
    }
    lab.metric("torch.brightest", hottest, "levels", Det::measured);
    lab.metric("torch.warmth", warm, "levels", Det::measured);
    if (hottest < 200) {
      lab.fail("the torch's flame never got bright (" + fixed(hottest, 0) + " of 255)",
               "Check the flame's luminance and the march (render/flame.wgsl). Skill: render.");
    }
    lab.shot({"torch-close", "A TORCH OVER HALF A SECOND",
              "A wall torch from a metre away, eight frames a fifteenth of a second apart (left to "
              "right, top to bottom): the gas carried up by its turbulence, swelling and shrinking "
              "with its flicker, its light on the wall and the bracket flickering with it.",
              "a teardrop of fire on the torch's black head, yellow-white low in its core, orange "
              "at its rim, tearing into red tongues at the top that differ frame to frame; the "
              "wall behind lit warm and brightening and dimming with the flame's size",
              inputs},
             sheet, Det::measured);
  }

  // The kinds: a candle, a torch, a brazier, a camp fire, each framed by its size.
  {
    gl::Picture sheet = gl::create_picture(static_cast<int>(w), static_cast<int>(h), {0, 0, 0});
    struct Pick {
      gr::Flame f;
      double back;
    };
    const Pick picks[4] = {
        {burning[6], 0.35}, {burning[1], 1.4}, {burning[4], 2.6}, {burning[8], 4.4}};
    for (int k = 0; k < 4; ++k) {
      const gr::Flame& f = picks[k].f;
      const double mid[3] = {f.base[0], f.base[1] + f.height_m * 0.45, f.base[2]};
      // From the hall's side for the indoor ones, from the south-west for the camp fire.
      const double eye_at[3] = {mid[0] + 0.3 * picks[k].back, mid[1] + 0.12 * picks[k].back,
                                mid[2] - picks[k].back};
      gr::FrameView c = look(eye_at, mid, t);
      const double near_floor[3] = {mid[0], f.base[1] - 0.5 * f.height_m,
                                    mid[2] - 0.6 * picks[k].back};
      c.lamp_lux = kNightKey * std::max(gr::lamp_lux_at(lamps, near_floor, t), 0.5);
      paste(sheet, half(draw(d, s, c, lamps, burning)), (k % 2) * static_cast<int>(w) / 2,
            (k / 2) * static_cast<int>(h) / 2);
    }
    lab.shot({"kinds", "FOUR KINDS OF FIRE",
              "Top: a candle (4 cm, 1 cd, a blue base) and a wall torch (40 cm). Bottom: a "
              "brazier (80 cm) and a camp fire (1.2 m) out on the flags, each framed by its size "
              "and seen by an eye adapted to the light it casts.",
              "the candle a small steady teardrop with a blue foot; the torch and brazier taller "
              "and torn at the top; the camp fire broad and slow, licking up off its embers, its "
              "ring of stones lit",
              inputs},
             sheet, Det::measured);
  }

  // The wind: the torch on its post in still air, a breeze and a draught.
  {
    gl::Picture sheet = gl::create_picture(static_cast<int>(w), static_cast<int>(h), {0, 0, 0});
    const double winds[3] = {0, 1, 3};
    for (int k = 0; k < 3; ++k) {
      std::vector<gr::Flame> one = {post_torch(winds[k])};
      const std::vector<gr::LocalLight> its = lights_of(one);
      const double mid[3] = {kPost[0] + 0.15, kPost[1] + 2.6, kPost[2]};
      const double eye_at[3] = {mid[0], mid[1] + 0.1, mid[2] - 1.6};
      gr::FrameView c = look(eye_at, mid, t);
      const double ground[3] = {kPost[0], 0.1, kPost[2] - 0.6};
      c.lamp_lux = std::max(gr::lamp_lux_at(its, ground, t), 0.5);
      const gl::Picture p = draw(d, s, c, its, one);
      // The middle third of each view, side by side.
      gl::Picture third =
          gl::create_picture(static_cast<int>(w) / 3, static_cast<int>(h), {0, 0, 0});
      for (int y = 0; y < third.height; ++y) {
        for (int x = 0; x < third.width; ++x) {
          const size_t from = (static_cast<size_t>(y) * p.width + x + third.width) * 4;
          const size_t to = (static_cast<size_t>(y) * third.width + x) * 4;
          for (size_t q = 0; q < 4; ++q) third.rgba[to + q] = p.rgba[from + q];
        }
      }
      paste(sheet, third, k * third.width, 0);
    }
    lab.shot({"wind", "A TORCH IN STILL AIR, A BREEZE AND A DRAUGHT",
              "The torch on its post with the air moving left to right at 0, 1 and 3 m/s "
              "(Flame::wind): the flame bends downwind against its 1 m/s rise, more toward its "
              "tip, and its light leans with it.",
              "upright on the left; leaning a little right in the middle; laid over to the right "
              "and stretched on the right",
              inputs},
             sheet, Det::measured);
  }
  checked(lab, static_cast<int>(d.errors.load()), static_cast<int>(d.pipelines_after_load));
}

const gl::LabSpec kSpec{
    "gpu-flames",
    gl::LabKind::picture,
    "engine",
    "engine/render/flame.hpp",
    "render",
    "Does fire read as burning gas (hot core, cool rim, torn tongues, a blackbody's colours), "
    "flicker with the light it casts, light a room by firelight, lean in the wind, and stop at "
    "what stands in front of it?",
    {{"width", "640", "a view's width, px (16:9)"}},
    "--width=256",
    run};
GE_LAB(kSpec);

}  // namespace
