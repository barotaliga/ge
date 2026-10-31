// LAB `se-weapon-stage`: a held weapon STAGED for replacing. The game's model of a weapon
// (games/shooters_edge/view: content/models/view, a built asset or a dropped-in .glb) and, with
// `--candidate=<file.glb>`, the model meant to replace it, mounted by the same weapon's look
// (`load_weapon_model`: muzzle forward, its held length, its grip) and drawn side by side, so a
// replacement is judged exactly as the game would hold it before it goes in:
//
//   - STAGE: a row a model: held at rest in first person through the game's hand by day, by the
//     torch, under the red and blue lamps, and on the frame of a shot in the dark lit by its own
//     muzzle flash (viewmodel/stage.hpp's corridor).
//   - BENCH: a row a model: the model alone on a plain backdrop from its right, its left, above,
//     and three-quarters from the front, lit from the upper left, at one scale a row: its form
//     and its texture where the first-person view does not reach.
//   - WIRE: each model's every triangle (lab/model_sheets.hpp's topology sheet), white.
//   - SKIN: each model's texture as the game draws it, its glowing texels outlined in cyan.
//   - ANIMATION: every clip of each model (the `fire` clip from a shot, through the hand), first
//     person beside the bench's right side, as a strip, and as `<weapon>-<clip>.gif` and `.webm`
//     beside the report to watch.
//   - ARMS: a model built round its hands, its arms measured at rest and every 1/60 s of every clip
//     by the engine's anatomy (viewmodel/arm.hpp): each joint's angle at rest, its least and most,
//     every frame outside a range named and failed, the bones drawn from the side and above.
//   - THE TABLE: triangles, vertices, the texture's size, the file's size, the held size (length,
//     width, height, m), the share of the picture its box covers and where its muzzle lands; the
//     sizes and the share are the weapon's alone, without the hands holding it.
//
// `--weapon=all` stages every weapon of the set (the game's models only). CPU pictures from the
// engine's own arithmetic, so in the digest; the file sizes are notes, not metrics.
//
// ge:summary Lab: a held weapon staged for replacing: the game's model beside a candidate .glb
// ge:keywords lab se-weapon-stage shooters-edge viewmodel held-weapon staging candidate replace
// ge:keywords bench turntable wireframe topology skin texture glow blender built-asset compare
// ge:keywords animation gif webm clip fire-clip record watch arms elbow wrist shoulder anatomy
// ge:keywords axe shotgun minigun railgun grenade-launcher rocket-launcher lightning-gun
// ge:skill held-weapons render labs assets
// ge:test tests/shooters_edge_view_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

#include "assets/gltf.hpp"
#include "assets/pose.hpp"
#include "camera/pose.hpp"
#include "capture/clip.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "lab/gif.hpp"
#include "lab/lab.hpp"
#include "lab/model_sheets.hpp"
#include "lab/raster.hpp"
#include "shooters_edge/view/hand.hpp"
#include "shooters_edge/view/weapons.hpp"
#include "viewmodel/arm.hpp"
#include "viewmodel/raster.hpp"
#include "viewmodel/stage.hpp"

namespace {

namespace se = shooters_edge::view;
namespace vm = ge::viewmodel;
using ge::lab::Picture;
using Inputs = std::vector<std::pair<std::string, std::string>>;

constexpr double kDt = 1.0 / 60;

// One model on the stage: the game's, or the candidate for its weapon.
struct Staged {
  std::string label;  // GAME or CANDIDATE
  std::string path;
  vm::HeldModel held;       // mounted
  vm::HeldModel bare;       // the weapon without its hands, mounted the same
  ge::assets::Model model;  // as loaded, for the wire
};

ge::camera::Pose stage_view() {
  ge::camera::Pose v;
  v.eye = {0, 1.6, 0};
  v.pitch = -0.04;
  v.fov_y = ge::camera::fov_y_of(90.0 * ge::core::kPi / 180.0, 16.0 / 9.0);
  return v;
}

Picture to_picture(std::vector<uint8_t> rgba, int w, int h) {
  Picture p;
  p.width = w;
  p.height = h;
  p.rgba = std::move(rgba);
  return p;
}

// Rows of panels, each row under its label on the left, each panel under its own label.
Picture rows_sheet(const std::vector<std::vector<Picture>>& rows,
                   const std::vector<std::string>& row_labels,
                   const std::vector<std::string>& panel_labels) {
  const int w = rows[0][0].width, h = rows[0][0].height, gap = 4, head = 24;
  size_t cols = 0;
  for (const auto& r : rows) cols = std::max(cols, r.size());
  Picture out =
      ge::lab::create_picture(gap + static_cast<int>(cols) * (w + gap),
                              gap + static_cast<int>(rows.size()) * (h + head + gap), {16, 17, 20});
  for (size_t r = 0; r < rows.size(); ++r) {
    const int y = gap + static_cast<int>(r) * (h + head + gap);
    ge::lab::draw_text(out, gap, y + 6, row_labels[r], {250, 210, 120}, 2);
    for (size_t c = 0; c < rows[r].size(); ++c) {
      const int x = gap + static_cast<int>(c) * (w + gap);
      ge::lab::blit(out, rows[r][c], x, y + head);
      if (c < panel_labels.size()) {
        const std::string& l = panel_labels[c];
        ge::lab::fill_rect(out, x, y + head, ge::lab::text_width(l, 2) + 8, 22, {0, 0, 0, 150});
        ge::lab::draw_text(out, x + 4, y + head + 4, l, {240, 240, 240}, 2);
      }
    }
  }
  return out;
}

// A view point on the screen (0..1 across, 0..1 down) through the lens `fov_y`.
void to_screen(const float v[3], double fov_y, double aspect, double out[2]) {
  const double ty = ge::core::libm::tan(0.5 * fov_y);
  const double z = std::max(1e-4, static_cast<double>(v[2]));
  out[0] = 0.5 + 0.5 * v[0] / (z * ty * aspect);
  out[1] = 0.5 - 0.5 * v[1] / (z * ty);
}

// The share of the screen the model's placed box at rest covers (clipped to the screen).
double screen_share(const vm::HeldModel& m, const vm::Placement& p, double fov_y, double aspect) {
  float lo[3], hi[3];
  vm::held_bounds(m, lo, hi, 0);
  double x0 = 1e9, x1 = -1e9, y0 = 1e9, y1 = -1e9;
  for (int c = 0; c < 8; ++c) {
    const float corner[3] = {(c & 1) ? hi[0] : lo[0], (c & 2) ? hi[1] : lo[1],
                             (c & 4) ? hi[2] : lo[2]};
    float v[3];
    vm::place_point(p, corner, v);
    double s[2];
    to_screen(v, fov_y, aspect, s);
    x0 = std::min(x0, s[0]);
    x1 = std::max(x1, s[0]);
    y0 = std::min(y0, s[1]);
    y1 = std::max(y1, s[1]);
  }
  const double w = std::clamp(x1, 0.0, 1.0) - std::clamp(x0, 0.0, 1.0);
  const double h = std::clamp(y1, 0.0, 1.0) - std::clamp(y0, 0.0, 1.0);
  return std::max(0.0, w) * std::max(0.0, h);
}

void muzzle_view(const vm::HeldModel& m, const vm::Placement& p, float out[3]) {
  float tip[3];
  vm::muzzle_at(m, 0, 0, 0, tip);
  vm::place_point(p, tip, out);
}

// The hand settled at rest on `w` for half a second, then one more frame (fired or not).
se::HandFrame settle(se::Weapon w, const std::vector<vm::HeldModel>& models,
                     const ge::camera::Pose& view, bool fire_last) {
  se::Hand hand;
  se::HandInput in;
  in.weapon = w;
  const se::ViewSettings game;
  se::HandFrame f;
  for (int i = 0; i < 30; ++i) {
    in.fired = fire_last && i == 29;
    f = se::step_hand(hand, in, game, models, view, kDt);
  }
  return f;
}

// The model alone, turned by `yaw` about its up axis then `pitch` about the view's x, its box's
// middle `dist` m ahead, on a grey backdrop, lit from the upper left of the eye.
Picture bench(const vm::HeldModel& m, double yaw, double pitch, double dist, double fov_y, int w,
              int h, int fa = 0, int fb = 0, double ft = 0) {
  float lo[3], hi[3];
  vm::held_bounds(m, lo, hi, 0);
  const float mid[3] = {(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2};
  const double cy = ge::core::libm::cos(yaw), sy = ge::core::libm::sin(yaw);
  const double cp = ge::core::libm::cos(pitch), sp = ge::core::libm::sin(pitch);
  auto turn = [&](const float* in, float* out, bool point) {
    const double x = in[0] - (point ? mid[0] : 0), y = in[1] - (point ? mid[1] : 0),
                 z = in[2] - (point ? mid[2] : 0);
    const double x1 = x * cy + z * sy, z1 = -x * sy + z * cy;
    const double y2 = y * cp - z1 * sp, z2 = y * sp + z1 * cp;
    out[0] = static_cast<float>(x1);
    out[1] = static_cast<float>(y2);
    out[2] = static_cast<float>(z2 + (point ? dist : 0));
  };
  std::vector<float> fp, fn;
  vm::pose_frames(m, fa, fb, ft, fp, fn);
  std::vector<float> p(fp.size()), n(fn.size());
  for (size_t i = 0; i + 2 < p.size(); i += 3) {
    turn(&fp[i], &p[i], true);
    turn(&fn[i], &n[i], false);
  }
  const float backdrop[3] = {0.045f, 0.05f, 0.06f};
  vm::ViewTarget t = vm::make_target(w, h, 2, backdrop);
  vm::Lighting light;
  for (int k = 0; k < 3; ++k) light.ambient[k] = 0.35f;
  for (int k = 0; k < 3; ++k) light.direct[k] = 1.7f;
  const float toward[3] = {-0.45f, 0.7f, -0.55f};
  const float len =
      std::sqrt(toward[0] * toward[0] + toward[1] * toward[1] + toward[2] * toward[2]);
  for (int k = 0; k < 3; ++k) light.toward[k] = toward[k] / len;
  vm::DrawLit d;
  d.positions = p;
  d.normals = n;
  d.indices = m.indices;
  d.colour = m.colour;
  d.uv = m.uv;
  d.skin = m.skin.image.rgba.empty() ? nullptr : &m.skin;
  d.fov_y = fov_y;
  d.near_m = 0.01;
  vm::draw_lit(t, d, light);
  return to_picture(vm::resolve(t), w, h);
}

// The skin scaled to fit `w` x `h` (nearest), its glowing texels outlined in cyan; a model with no
// skin draws a note instead.
Picture skin_panel(const vm::HeldModel& m, int w, int h) {
  Picture out = ge::lab::create_picture(w, h, {24, 24, 28});
  const ge::assets::Image& img = m.skin.image;
  if (img.rgba.empty()) {
    ge::lab::draw_text(out, 8, 8, "NO TEXTURE: A COLOUR A VERTEX", {200, 200, 200}, 2);
    return out;
  }
  const double s =
      std::min(static_cast<double>(w) / img.width, static_cast<double>(h) / img.height);
  const int dw = std::max(1, static_cast<int>(img.width * s)),
            dh = std::max(1, static_cast<int>(img.height * s));
  auto glows = [&](int x, int y) {
    if (x < 0 || y < 0 || x >= img.width || y >= img.height || m.skin.glow.empty()) return false;
    return m.skin.glow[size_t(y) * img.width + x] != 0;
  };
  for (int y = 0; y < dh; ++y) {
    for (int x = 0; x < dw; ++x) {
      const int sx = std::min(img.width - 1, static_cast<int>(x / s));
      const int sy = std::min(img.height - 1, static_cast<int>(y / s));
      const uint8_t* px = &img.rgba[(size_t(sy) * img.width + sx) * 4];
      ge::lab::Rgba c{px[0], px[1], px[2], 255};
      if (glows(sx, sy) &&
          (!glows(sx - 1, sy) || !glows(sx + 1, sy) || !glows(sx, sy - 1) || !glows(sx, sy + 1)))
        c = {0, 255, 255, 255};
      ge::lab::set_pixel(out, x, y, c);
    }
  }
  return out;
}

// Every clip of every staged model, frame by frame at the clip's rate: first person through the
// game's hand by the torch (the `fire` clip from a shot, so the kick and the flash ride on it;
// any other clip drawn at rest) beside the bench's view from the right, written as an animated
// GIF and a WebM a person can watch, and as a strip of six frames for the digest.
void animations(ge::lab::Lab& lab, const se::WeaponLook& look, const std::vector<Staged>& staged,
                const std::vector<vm::HeldModel>& game_set, int width, double bench_dist,
                double bench_fov, const std::vector<std::string>& row_labels,
                const Inputs& inputs) {
  const int height = width * 9 / 16;
  const std::string key(look.name);
  const vm::Stage stage = vm::corridor_stage();
  const ge::camera::Pose view = stage_view();
  for (size_t si = 0; si < staged.size(); ++si) {
    const Staged& s = staged[si];
    std::vector<vm::HeldModel> set = game_set;
    set[static_cast<size_t>(look.weapon)] = s.held;
    for (const vm::HeldClip& clip : s.held.clips) {
      const bool fire = clip.name == "fire";
      const double fps = clip.fps;
      const int frames = clip.count + static_cast<int>(fps * 0.25);  // and a rest after
      se::Hand hand;
      se::HandInput in;
      in.weapon = look.weapon;
      const se::ViewSettings game;
      for (int i = 0; i < 30; ++i) se::step_hand(hand, in, game, set, view, kDt);
      std::vector<Picture> video, strip;
      std::vector<std::string> strip_labels;
      ge::lab::LabTable path{key + " " + s.label + " " + clip.name + ": the tip on the screen",
                             {"frame", "tip x", "tip y"},
                             {}};
      const int every = std::max(1, frames / 6);
      for (int k = 0; k < frames; ++k) {
        in.fired = fire && k == 0;
        se::HandFrame f = se::step_hand(hand, in, game, set, view, 1.0 / fps);
        if (!fire)
          vm::held_clip_frames(s.held, find_held_clip(s.held, clip.name), k / fps, false, f.frame_a,
                               f.frame_b, f.frame_t);
        float mv[3];
        muzzle_view(s.held, f.placement, mv);
        vm::StageLights l = vm::stage_lights(vm::StageLight::torch, view, mv);
        if (f.flashing) {
          l.lights.push_back(f.flash);
          l.flare = true;
          muzzle_view(s.held, f.placement, l.flare_at);
        }
        vm::StageShot shot;
        shot.width = width;
        shot.height = height;
        shot.frame_a = f.frame_a;
        shot.frame_b = f.frame_b;
        shot.frame_t = f.frame_t;
        const Picture fp =
            to_picture(vm::draw_stage(stage, l, view, s.held, f.placement, shot), width, height);
        const Picture bp = bench(s.held, ge::core::kPi / 2, 0, bench_dist, bench_fov, width, height,
                                 f.frame_a, f.frame_b, f.frame_t);
        Picture both = ge::lab::create_picture(2 * width, height, {0, 0, 0});
        ge::lab::blit(both, fp, 0, 0);
        ge::lab::blit(both, bp, width, 0);
        char t[32];
        std::snprintf(t, sizeof t, "%s %.3f S", clip.name.c_str(), k / fps);
        ge::lab::draw_text(both, 6, 6, t, {240, 240, 240}, 2);
        video.push_back(both);
        if (k % every == 0 && strip.size() < 6) {
          strip.push_back(both);
          strip_labels.emplace_back(t);
          // Where the weapon's tip (its muzzle, an axe's edge) is on the screen this frame.
          float tip[3], v[3];
          vm::muzzle_at(s.held, f.frame_a, f.frame_b, f.frame_t, tip);
          vm::place_point(f.placement, tip, v);
          double at[2];
          to_screen(v, f.placement.fov_y > 0 ? f.placement.fov_y : view.fov_y,
                    static_cast<double>(width) / height, at);
          char cx[16], cy[16];
          std::snprintf(cx, sizeof cx, "%.3f", at[0]);
          std::snprintf(cy, sizeof cy, "%.3f", at[1]);
          path.rows.push_back({t, cx, cy});
        }
      }
      const std::string stem = key + (s.label == "GAME" ? "" : "-candidate") + "-" + clip.name;
      lab.attach(stem + ".gif", ge::lab::encode_gif_frames(
                                    video, static_cast<int>(ge::core::num::round(100.0 / fps))));
      const std::string tmp =
          (std::filesystem::temp_directory_path() / ("ge-stage-" + stem + ".webm")).string();
      std::string error;
      if (ge::capture::write_clip(tmp, video, fps, error)) {
        std::ifstream f(tmp, std::ios::binary);
        lab.attach(stem + ".webm",
                   std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),
                                        std::istreambuf_iterator<char>()),
                   ge::lab::Det::measured);
        std::error_code ec;
        std::filesystem::remove(tmp, ec);
      } else {
        std::string why = stem;
        why += ": no WebM (";
        why += error;
        why += "); the GIF has the same frames";
        lab.note(why);
      }
      lab.metric(stem + ".frames", clip.count, "count");
      lab.table(path);
      std::vector<std::vector<Picture>> one;
      one.reserve(strip.size());
      for (const Picture& p : strip) one.push_back({p});
      std::string caption = row_labels[si];
      caption += ": the clip `" + clip.name + "` (" + std::to_string(clip.count);
      caption +=
          " frames at " + std::to_string(int(fps)) + " a second), first person by the torch ";
      caption += fire ? "from the shot (the kick and the flash on it)" : "at rest";
      caption += " beside the model from its right; six frames down the page. The whole clip is ";
      caption += stem;
      caption += ".gif and ";
      caption += stem;
      caption += ".webm beside this picture.";
      lab.shot({"anim-" + stem, "ANIMATION: " + key + " " + clip.name + ", " + s.label, caption,
                "the moving parts move as one piece and come back to rest; nothing tears, "
                "flickers or passes through another part; the clip ends where the next shot "
                "would start",
                inputs},
               rows_sheet(one, strip_labels, {}));
    }
  }
}

// THE ARMS: every staged model built round its hands, posed at rest and every 1/60 s of every
// clip, each arm measured by the engine's anatomy (viewmodel/arm.hpp): a table of each joint at
// rest and its least and most over the clips, every frame outside a range named (and failed), and
// the arm's bones drawn from the side and from above over every frame. Returns the faults.
int arms(ge::lab::Lab& lab, const se::WeaponLook& look, const std::vector<Staged>& staged,
         int width, const std::vector<std::string>& row_labels, const Inputs& inputs) {
  const int height = width * 9 / 16;
  const std::string key(look.name);
  // The built model's frame after the loader: the view's right is -x, its forward -z.
  const double right[3] = {-1, 0, 0}, up[3] = {0, 1, 0}, forward[3] = {0, 0, -1};
  int faults = 0;
  std::vector<std::vector<Picture>> rows;
  std::vector<std::string> labels;
  ge::lab::LabTable table{key + ": the arms' joints, deg (rest, least, most)",
                          {"model", "arm", "shoulder up", "shoulder swing", "elbow bend",
                           "elbow point", "wrist back", "wrist thumbward", "forearm twist"},
                          {}};
  for (size_t si = 0; si < staged.size(); ++si) {
    const ge::assets::Model& m = staged[si].model;
    const size_t nj = m.joints.size();
    std::vector<ge::assets::Trs> local(nj);
    std::vector<ge::assets::Affine> global(nj);
    vm::ArmPose poses[2];
    ge::assets::rest_pose(m, local);
    ge::assets::pose_globals(m, local, global);
    const int count = vm::arm_poses(m, global, right, up, forward, poses);
    if (count == 0) continue;
    Picture side = ge::lab::create_picture(width, height, {20, 22, 26});
    Picture top = ge::lab::create_picture(width, height, {20, 22, 26});
    // Side: forward to the right, up up; above: right to the right, forward up. True metres, one
    // scale for both, framing every joint of every frame with a margin.
    double lo[2][2] = {{1e9, 1e9}, {1e9, 1e9}}, hi[2][2] = {{-1e9, -1e9}, {-1e9, -1e9}};
    auto plane = [](int pass, const double v[3], double& a, double& b) {
      if (pass == 0) {
        a = -v[2];  // forward
        b = v[1];   // up
      } else {
        a = -v[0];  // right
        b = -v[2];  // forward
      }
    };
    auto grow = [&](const vm::ArmPose* p, int n) {
      for (int a = 0; a < n; ++a)
        for (const double* q : {p[a].shoulder, p[a].elbow, p[a].wrist})
          for (int pass = 0; pass < 2; ++pass) {
            double x, y;
            plane(pass, q, x, y);
            lo[pass][0] = std::min(lo[pass][0], x), hi[pass][0] = std::max(hi[pass][0], x);
            lo[pass][1] = std::min(lo[pass][1], y), hi[pass][1] = std::max(hi[pass][1], y);
          }
    };
    grow(poses, count);
    for (const ge::assets::Clip& clip : m.clips)
      for (uint32_t f = 0; f < clip.frames; ++f) {
        ge::assets::sample_clip(m, clip, f / 60.0, false, local);
        ge::assets::pose_globals(m, local, global);
        vm::ArmPose p[2];
        grow(p, vm::arm_poses(m, global, right, up, forward, p));
      }
    double scale = 1e9;
    for (int pass = 0; pass < 2; ++pass)
      scale = std::min({scale, width * 0.8 / std::max(0.05, hi[pass][0] - lo[pass][0] + 0.12),
                        height * 0.8 / std::max(0.05, hi[pass][1] - lo[pass][1] + 0.12)});
    // pass 0 from the right side, 1 from above.
    auto px = [&](int pass, const double v[3], double& x, double& y) {
      double a, b;
      plane(pass, v, a, b);
      x = width * 0.5 + (a - 0.5 * (lo[pass][0] + hi[pass][0])) * scale;
      y = height * 0.5 - (b - 0.5 * (lo[pass][1] + hi[pass][1])) * scale;
    };
    struct Range {
      double rest[7], lo[7], hi[7];
    } range[2];
    for (Range& r : range)
      for (int k = 0; k < 7; ++k) r.rest[k] = 0, r.lo[k] = 1e9, r.hi[k] = -1e9;
    std::string first_fault[2];
    auto values = [](const vm::ArmAngles& a, double out[7]) {
      const double v[7] = {a.shoulder_elevation, a.shoulder_yaw, a.elbow_flex,   a.elbow_roll,
                           a.wrist_extension,    a.wrist_radial, a.forearm_twist};
      for (int k = 0; k < 7; ++k) out[k] = v[k];
    };
    auto draw = [&](const vm::ArmPose* p, int n, ge::lab::Rgba c) {
      for (int a = 0; a < n; ++a) {
        double knuckle[3];
        const double dl =
            std::sqrt(p[a].distal[0] * p[a].distal[0] + p[a].distal[1] * p[a].distal[1] +
                      p[a].distal[2] * p[a].distal[2]);
        for (int k = 0; k < 3; ++k) knuckle[k] = p[a].wrist[k] + p[a].distal[k] / dl * 0.09;
        const double* pts[4] = {p[a].shoulder, p[a].elbow, p[a].wrist, knuckle};
        for (int pass = 0; pass < 2; ++pass) {
          Picture& pic = pass == 0 ? side : top;
          for (int k = 0; k < 3; ++k) {
            double x0, y0, x1, y1;
            px(pass, pts[k], x0, y0);
            px(pass, pts[k + 1], x1, y1);
            ge::lab::draw_line(pic, x0, y0, x1, y1, c);
          }
        }
      }
    };
    // Every clip, every 1/60 s, in a faint colour by time; then the rest pose bold on top.
    for (const ge::assets::Clip& clip : m.clips) {
      for (uint32_t f = 0; f < clip.frames; ++f) {
        ge::assets::sample_clip(m, clip, f / 60.0, false, local);
        ge::assets::pose_globals(m, local, global);
        vm::ArmPose p[2];
        const int n = vm::arm_poses(m, global, right, up, forward, p);
        const double t = clip.frames > 1 ? static_cast<double>(f) / (clip.frames - 1) : 0;
        draw(p, n,
             {static_cast<uint8_t>(90 + 120 * t), 120, static_cast<uint8_t>(210 - 120 * t), 110});
        for (int a = 0; a < n; ++a) {
          const vm::ArmAngles ang = vm::arm_angles(p[a]);
          double v[7];
          values(ang, v);
          for (int k = 0; k < 7; ++k) {
            range[a].lo[k] = std::min(range[a].lo[k], v[k]);
            range[a].hi[k] = std::max(range[a].hi[k], v[k]);
          }
          const std::vector<std::string> bad = vm::arm_faults(ang);
          if (!bad.empty() && first_fault[a].empty()) {
            char at[64];
            std::snprintf(at, sizeof at, " (clip %s at %.3f s)", clip.name.c_str(), f / 60.0);
            first_fault[a] = bad[0] + at;
          }
        }
      }
    }
    for (int a = 0; a < count; ++a) {
      const vm::ArmAngles ang = vm::arm_angles(poses[a]);
      values(ang, range[a].rest);
      for (int k = 0; k < 7; ++k) {
        range[a].lo[k] = std::min(range[a].lo[k], range[a].rest[k]);
        range[a].hi[k] = std::max(range[a].hi[k], range[a].rest[k]);
      }
      const std::vector<std::string> bad = vm::arm_faults(ang);
      if (!bad.empty()) first_fault[a] = bad[0] + " (at rest)";
    }
    draw(poses, count, {245, 245, 245, 255});
    for (int a = 0; a < count; ++a) {
      for (int pass = 0; pass < 2; ++pass) {
        double x, y;
        px(pass, poses[a].shoulder, x, y);
        ge::lab::fill_disc(pass == 0 ? side : top, x, y, 4, {230, 80, 70});
        px(pass, poses[a].elbow, x, y);
        ge::lab::fill_disc(pass == 0 ? side : top, x, y, 4, {90, 220, 110});
        px(pass, poses[a].wrist, x, y);
        ge::lab::fill_disc(pass == 0 ? side : top, x, y, 4, {90, 150, 240});
      }
      const char* name = poses[a].side > 0 ? "right" : "left";
      std::vector<std::string> row = {staged[si].label, name};
      for (int k = 0; k < 7; ++k) {
        char c[40];
        std::snprintf(c, sizeof c, "%.0f (%.0f..%.0f)", range[a].rest[k], range[a].lo[k],
                      range[a].hi[k]);
        row.emplace_back(c);
      }
      table.rows.push_back(row);
      if (!first_fault[a].empty()) {
        ++faults;
        lab.note(key + " " + staged[si].label + ", the " + name + " arm: " + first_fault[a]);
      }
    }
    rows.push_back({side, top});
    labels.push_back(row_labels[si]);
  }
  if (rows.empty()) return 0;
  lab.table(table);
  lab.shot({"arm-" + key, "ARMS: " + key,
            "Each model's arms from the right side (forward to the right) and from above (forward "
            "up): shoulder (red), elbow (green), wrist (blue) and the hand to its knuckles, the "
            "rest pose in white over every frame of every clip, blue to orange through each.",
            "every elbow points down and out from the line between its shoulder and its wrist, "
            "never up and never in across the body; the shoulders stay where they are unless the "
            "body turns; the wrist runs on nearly straight from the forearm into the hand",
            inputs},
           rows_sheet(rows, labels, {"FROM THE RIGHT", "FROM ABOVE"}));
  return faults;
}

bool stage_one(ge::lab::Lab& lab, const se::WeaponLook& look, const std::vector<Staged>& staged,
               const std::vector<vm::HeldModel>& game_set, int width, const Inputs& inputs) {
  const int height = width * 9 / 16;
  const double aspect = static_cast<double>(width) / height;
  const std::string key(look.name);
  const vm::Stage stage = vm::corridor_stage();
  const ge::camera::Pose view = stage_view();
  vm::StageShot shot;
  shot.width = width;
  shot.height = height;
  bool ok = true;

  // STAGE: through the hand, as the game holds it.
  std::vector<std::vector<Picture>> stage_rows, bench_rows;
  std::vector<std::string> row_labels;
  ge::lab::LabTable table{key + " staged",
                          {"model", "triangles", "vertices", "texture", "length m", "width m",
                           "height m", "screen share", "muzzle x", "muzzle y"},
                          {}};
  double bench_radius = 0;
  for (const Staged& s : staged) {
    float lo[3], hi[3];
    vm::held_bounds(s.held, lo, hi, 0);
    const double dx = hi[0] - lo[0], dy = hi[1] - lo[1], dz = hi[2] - lo[2];
    bench_radius = std::max(bench_radius, 0.5 * std::sqrt(dx * dx + dy * dy + dz * dz));
  }
  const double bench_fov = 30.0 * ge::core::kPi / 180.0;
  const double bench_dist = bench_radius / ge::core::libm::tan(0.5 * bench_fov) * 1.08;
  for (const Staged& s : staged) {
    std::vector<vm::HeldModel> set = game_set;
    set[static_cast<size_t>(look.weapon)] = s.held;
    const se::HandFrame rest = settle(look.weapon, set, view, false);
    const se::HandFrame fired = settle(look.weapon, set, view, true);
    float mv[3];
    muzzle_view(s.held, rest.placement, mv);
    std::vector<Picture> row;
    for (vm::StageLight kind :
         {vm::StageLight::day, vm::StageLight::torch, vm::StageLight::coloured}) {
      const vm::StageLights l = vm::stage_lights(kind, view, mv);
      row.push_back(
          to_picture(vm::draw_stage(stage, l, view, s.held, rest.placement, shot), width, height));
    }
    vm::StageLights dark = vm::stage_lights(vm::StageLight::dark, view, mv);
    if (fired.flashing) {
      dark.lights.push_back(fired.flash);
      dark.flare = true;
      muzzle_view(s.held, fired.placement, dark.flare_at);
    }
    vm::StageShot fired_shot = shot;
    fired_shot.frame_a = fired.frame_a;
    fired_shot.frame_b = fired.frame_b;
    fired_shot.frame_t = fired.frame_t;
    row.push_back(to_picture(vm::draw_stage(stage, dark, view, s.held, fired.placement, fired_shot),
                             width, height));
    stage_rows.push_back(std::move(row));
    row_labels.push_back(s.label + ": " + std::filesystem::path(s.path).filename().string());

    const double q = ge::core::kPi / 2;
    bench_rows.push_back({bench(s.held, q, 0, bench_dist, bench_fov, width, height),
                          bench(s.held, -q, 0, bench_dist, bench_fov, width, height),
                          bench(s.held, q, -q, bench_dist, bench_fov, width, height),
                          bench(s.held, 2.4, -0.3, bench_dist, bench_fov, width, height)});

    float lo[3], hi[3];
    vm::held_bounds(s.bare, lo, hi, 0);
    const double dx = hi[0] - lo[0], dy = hi[1] - lo[1], dz = hi[2] - lo[2];
    const double share = screen_share(s.bare, rest.placement, view.fov_y, aspect);
    double at[2];
    to_screen(mv, view.fov_y, aspect, at);
    const std::string mkey = key + (s.label == "GAME" ? "" : "-candidate");
    lab.metric(mkey + ".share", share, "share");
    lab.metric(mkey + ".triangles", static_cast<double>(s.held.indices.size()) / 3, "count");
    char c[7][24];
    std::snprintf(c[0], 24, "%zu", s.held.indices.size() / 3);
    std::snprintf(c[1], 24, "%u", s.held.vertex_count());
    if (s.held.skin.image.rgba.empty())
      std::snprintf(c[2], 24, "none");
    else
      std::snprintf(c[2], 24, "%dx%d", s.held.skin.image.width, s.held.skin.image.height);
    std::snprintf(c[3], 24, "%.3f", dz);
    std::snprintf(c[4], 24, "%.3f", dx);
    std::snprintf(c[5], 24, "%.3f", dy);
    std::snprintf(c[6], 24, "%.3f", share);
    char mx[16], my[16];
    std::snprintf(mx, 16, "%.3f", at[0]);
    std::snprintf(my, 16, "%.3f", at[1]);
    table.rows.push_back({s.label, c[0], c[1], c[2], c[3], c[4], c[5], c[6], mx, my});
    std::error_code ec;
    const auto bytes = std::filesystem::file_size(s.path, ec);
    if (!ec)
      lab.note(key + " " + s.label + ": " + s.path + " is " +
               std::to_string((bytes + 1023) / 1024) + " KiB");
    // Held, not a toy and not the whole picture (a built weapon at its true size in hands covers
    // more than a toy-scale one did); the muzzle on the screen.
    if (share < 0.04 || share > 0.4) {
      ok = false;
      lab.note(key + " " + s.label + ": its box covers " + c[6] +
               " of the picture (stated "
               "0.04..0.4)");
    }
    if (!look.upright && (at[0] < 0.02 || at[0] > 0.98 || at[1] < 0.02 || at[1] > 0.98)) {
      ok = false;
      lab.note(key + " " + s.label + ": its muzzle is off the picture");
    }
  }
  lab.table(table);
  const bool pair = staged.size() > 1;
  lab.shot(
      {"stage-" + key, "STAGED: " + key + (pair ? ", GAME AND CANDIDATE" : ""),
       "The " + key +
           " held through the game's hand at rest by day, by the torch on the "
           "left wall, under a red lamp left and a blue one right, and on the frame of a shot in "
           "the dark lit by its own muzzle flash. " +
           (pair ? "Top row the game's model, below it the candidate mounted by the same look."
                 : "The game's model."),
       "the weapon sits low right at the size of the set and points down the corridor; the "
       "torch's warm light and the lamps' colours land on the near side of its metal; the "
       "flash lights it from the muzzle; the texture reads at this size with no swimming "
       "pattern or stretched texels",
       inputs},
      rows_sheet(stage_rows, row_labels, {"DAY", "TORCH", "LAMPS", "FIRED, DARK"}));
  lab.shot({"bench-" + key, "BENCH: " + key,
            "Each model alone on a dark backdrop at one scale for the row: from its right (the "
            "muzzle to the right), its left, above, and three-quarters from the front and above, "
            "lit from the upper left of the eye.",
            "a closed, solid form with no holes or floating parts; the parts a person knows the "
            "weapon by (barrels, grip, stock, magazine, sights) where they should be and in "
            "proportion; the texture's wear on the edges and the grain or scratches along the "
            "parts, not across them",
            inputs},
           rows_sheet(bench_rows, row_labels, {"RIGHT", "LEFT", "ABOVE", "FRONT 3/4"}));
  {
    std::vector<std::vector<Picture>> wires, skins;
    for (const Staged& s : staged) {
      const ge::lab::Shape shape = ge::lab::shape_at(s.model, -1, 0);
      double floor_y = 0;
      for (size_t i = 1; i < shape.pos.size(); i += 3)
        floor_y = i == 1 ? shape.pos[i] : std::min(floor_y, static_cast<double>(shape.pos[i]));
      wires.push_back({ge::lab::topology_sheet(s.model, shape, floor_y, width)});
      skins.push_back({skin_panel(s.held, width * 2, width)});
    }
    lab.shot({"wire-" + key, "WIRE: " + key,
              "Every triangle of each model as loaded (its own frame, before the mount), white, "
              "with its nodes named.",
              "triangles spent where the silhouette bends (round barrels, a drum, a guard), few "
              "on flat faces; bevels as thin strips along the hard edges",
              inputs},
             rows_sheet(wires, row_labels, {}));
    lab.shot({"skin-" + key, "SKIN: " + key,
              "Each model's texture as the game draws it (nearest texel), scaled to fit; cyan "
              "outlines the texels that light themselves (the file's emissive texture).",
              "the islands packed with little empty space; wear lighter on the edges, the grain "
              "and scratches running along each part; glow only on what should shine (a coil, an "
              "indicator, a sight)",
              inputs},
             rows_sheet(skins, row_labels, {}));
  }
  animations(lab, look, staged, game_set, width, bench_dist, bench_fov, row_labels, inputs);
  if (arms(lab, look, staged, width, row_labels, inputs) > 0) ok = false;
  return ok;
}

void run(ge::lab::Lab& lab) {
  const int width = static_cast<int>(lab.arg_num("width"));
  if (width < 96 || width > 1920) {
    lab.fail("--width must be 96..1920", "next: rerun with --width=480.");
    return;
  }
  const std::string dir = lab.arg("models");
  const std::string which = lab.arg("weapon");
  const std::string candidate = lab.arg("candidate");
  std::vector<vm::HeldModel> game_set;
  std::string error;
  if (!se::load_weapon_models(dir, game_set, &error)) {
    lab.fail(error, "next: rerun from the repository's root, or pass --models=<folder>.");
    return;
  }
  std::vector<const se::WeaponLook*> looks;
  for (const se::WeaponLook& look : se::weapon_looks()) {
    if (which == "all" || which == look.name) looks.push_back(&look);
  }
  if (looks.empty()) {
    std::string names;
    for (const se::WeaponLook& look : se::weapon_looks()) names += " " + std::string(look.name);
    lab.fail("--weapon=" + which + " names no weapon of the set",
             "next: pass one of" + names + ", or all.");
    return;
  }
  if (!candidate.empty() && looks.size() != 1) {
    lab.fail("--candidate needs one weapon to stand in for",
             "next: pass --weapon=<name> with it (the set's names are in weapons.cpp).");
    return;
  }
  const Inputs inputs = {{"models", dir},
                         {"weapon", which},
                         {"candidate", candidate},
                         {"width", std::to_string(width)}};
  int problems = 0;
  for (const se::WeaponLook* look : looks) {
    std::vector<Staged> staged;
    auto add = [&](const std::string& label, const std::string& path) {
      Staged s;
      s.label = label;
      s.path = path;
      if (!se::load_weapon_model(path, *look, s.held, &error, &s.bare)) {
        lab.fail(error,
                 "next: check the file opens as glTF 2.0 (make lab LAB=models "
                 "ARGS=--model=<file>), or rebuild it (ge asset build <name>).");
        return false;
      }
      ge::assets::LoadOptions keep;
      keep.keep_textures = true;
      ge::assets::LoadResult r = ge::assets::load_gltf_file(path, keep);
      for (const std::string& n : r.notes)
        lab.note(std::string(look->name).append(" ").append(label).append(": ").append(n));
      s.model = std::move(r.model);
      staged.push_back(std::move(s));
      return true;
    };
    if (!add("GAME", se::weapon_model_path(dir, *look))) return;
    if (!candidate.empty() && !add("CANDIDATE", candidate)) return;
    if (!stage_one(lab, *look, staged, game_set, width, inputs)) ++problems;
  }
  lab.metric("problems", problems, "count");
  if (problems > 0)
    lab.fail(std::to_string(problems) + " weapons missed a stated limit (the notes name them)",
             "next: change the weapon's length or offset in games/shooters_edge/view/weapons.cpp, "
             "or the model's proportions in its builder, and rerun.");
}

const ge::lab::LabSpec kSpec{
    "se-weapon-stage",
    ge::lab::LabKind::picture,
    "game:shooters_edge",
    "games/shooters_edge/view (a weapon's model as the game loads and mounts it) and a candidate "
    "model for it",
    "held-weapons",
    "How does a held weapon's model, and a candidate meant to replace it, look as the game holds "
    "it: in first person under the corridor's lights, alone from every side, in wire, and its "
    "texture?",
    {{"weapon", "all", "the weapon to stage by its model's name (weapons.cpp), or all"},
     {"candidate", "", "a .glb to stage beside the game's model of --weapon"},
     {"models", "games/shooters_edge/content/models/view", "the folder of the view models"},
     {"width", "480", "a panel's width, px (its height is 9/16 of it)"}},
    "--weapon=shotgun --width=128",
    run,
};
GE_LAB(kSpec);

}  // namespace
