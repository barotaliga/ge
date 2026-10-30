// LAB `models`: an authored model (engine/assets) LOOKED AT the way an animation is debugged:
// the mesh at bind, shaded with its wireframe and its skeleton over it, from behind and from the
// right; each clip as a filmstrip of evenly spaced frames and as an onion skin of every frame's
// bones on one picture; and each clip's joint angles (from rest, degrees) and their angular
// acceleration graphed over time, where a pop, a jolt or a joint that twists past its range
// shows as a spike. The numbers say what the model is (joints, meshes, triangles, materials,
// clips), whether its weights hold (summing to 1, every joint index inside the skeleton) and
// each clip's largest angle and acceleration. A mesh's morph targets (a vertex-animated
// model's frames, a target a frame) are weighted as each clip weights them, so the filmstrips
// draw such a model frame by frame. With no `--model` it is the test model
// (assets/test_model.hpp) written as a .glb and loaded back, so the smoke run goes through the
// loader too. CPU pictures (an orthographic raster of the model), so in the digest.
//
// ge:summary Lab: a model's bind, filmstrips, onion skins and joint-angle graphs, its numbers
// ge:keywords lab models model gltf glb animation clip filmstrip onion-skin wireframe joints
// ge:keywords bones joint-angle angular-acceleration jolt pop skeleton weights outliers
// ge:skill assets labs
// ge:test tests/assets_pose_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "assets/gltf.hpp"
#include "assets/gltf_write.hpp"
#include "assets/pose.hpp"
#include "assets/test_model.hpp"
#include "lab/lab.hpp"

namespace {

namespace a = ge::assets;
using ge::lab::Picture;
using ge::lab::Rgba;

constexpr double kDeg = 57.29577951308232;
constexpr Rgba kBack{24, 26, 32};
constexpr Rgba kGrid{60, 64, 74};
constexpr Rgba kText{220, 224, 232};
constexpr Rgba kBone{255, 210, 60};
// Distinct colours for a joint's line on a graph, in turn.
constexpr Rgba kSeries[8] = {{240, 90, 80},   {90, 190, 240}, {120, 220, 110}, {240, 180, 60},
                             {200, 120, 240}, {80, 220, 200}, {240, 130, 190}, {180, 180, 180}};

// An orthographic view: screen x along `right`, screen y down along -up, depth along `toward`.
struct View {
  const char* name;
  int right;   // world axis to the screen's right (0 x, 2 z)
  float sign;  // +1 or -1 along it
  int depth;   // world axis toward the viewer
  float depth_sign;
};
constexpr View kBehind{"behind", 0, 1, 2, -1};  // from -z looking +z: +x to the right
constexpr View kRight{"right", 2, 1, 0, 1};     // from +x looking -x: +z to the right

struct Frame {
  double cx, cy, scale;  // the model's box centre on screen and px a metre
  double x0, y0;         // the panel's top left
};

void project(const View& v, const Frame& f, const float* p, double* sx, double* sy) {
  *sx = f.x0 + f.cx + v.sign * p[v.right] * f.scale;
  *sy = f.y0 + f.cy - p[1] * f.scale;
}

// One triangle filled flat (its pixel centres inside it), clipped to the picture.
void fill_triangle(Picture& pic, const double* x, const double* y, Rgba c) {
  const int lx = std::max(0, static_cast<int>(std::floor(std::min({x[0], x[1], x[2]}))));
  const int hx = std::min(pic.width - 1, static_cast<int>(std::ceil(std::max({x[0], x[1], x[2]}))));
  const int ly = std::max(0, static_cast<int>(std::floor(std::min({y[0], y[1], y[2]}))));
  const int hy =
      std::min(pic.height - 1, static_cast<int>(std::ceil(std::max({y[0], y[1], y[2]}))));
  const double area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);
  if (area == 0) return;
  for (int py = ly; py <= hy; ++py) {
    for (int px = lx; px <= hx; ++px) {
      const double cx = px + 0.5, cy = py + 0.5;
      const double w0 = (x[1] - cx) * (y[2] - cy) - (x[2] - cx) * (y[1] - cy);
      const double w1 = (x[2] - cx) * (y[0] - cy) - (x[0] - cx) * (y[2] - cy);
      const double w2 = (x[0] - cx) * (y[1] - cy) - (x[1] - cx) * (y[0] - cy);
      if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) {
        ge::lab::set_pixel(pic, px, py, c);
      }
    }
  }
}

struct Posed {
  std::vector<float> pos, nrm;
  std::vector<a::Affine> global;
};

Posed pose(const a::Model& m, int clip, double t) {
  std::vector<a::Trs> local(m.joints.size());
  Posed p;
  p.global.resize(m.joints.size());
  std::vector<a::Affine> palette(m.joints.size());
  if (clip < 0) {
    a::rest_pose(m, local);
  } else {
    a::sample_clip(m, m.clips[static_cast<size_t>(clip)], t, false, local);
  }
  a::pose_globals(m, local, p.global);
  a::skin_palette(m, p.global, palette);
  for (size_t k = 0; k < m.meshes.size(); ++k) {
    const a::Mesh& mesh = m.meshes[k];
    std::vector<float> pos(size_t{mesh.vertex_count()} * 3), nrm(pos.size());
    std::vector<float> bind(pos.size()), bind_n(pos.size()), weights(mesh.targets.size());
    a::sample_weights(m, clip < 0 ? nullptr : &m.clips[static_cast<size_t>(clip)], k, t, false,
                      weights);
    a::morph_mesh(mesh, weights, bind, bind_n);
    a::skin_mesh(mesh, palette, bind, bind_n, pos, nrm);
    p.pos.insert(p.pos.end(), pos.begin(), pos.end());
    p.nrm.insert(p.nrm.end(), nrm.begin(), nrm.end());
  }
  return p;
}

// The model shaded (back to front along the view, lit from above, behind and to the right),
// its edges over it faintly, and its bones over both.
void draw_model(Picture& pic, const a::Model& m, const Posed& p, const View& v, const Frame& f,
                bool mesh, Rgba bone, bool labels) {
  if (mesh) {
    struct Tri {
      float depth;
      uint32_t a, b, c;
      uint8_t part;
    };
    std::vector<Tri> tris;
    uint32_t base = 0;
    for (const a::Mesh& mm : m.meshes) {
      for (size_t t = 0; t + 2 < mm.indices.size(); t += 3) {
        const uint32_t i[3] = {base + mm.indices[t], base + mm.indices[t + 1],
                               base + mm.indices[t + 2]};
        float d = 0;
        for (uint32_t k : i) d += v.depth_sign * p.pos[size_t{k} * 3 + v.depth];
        tris.push_back({d, i[0], i[1], i[2], mm.parts[mm.indices[t]]});
      }
      base += mm.vertex_count();
    }
    // Farthest first: the larger depth toward the viewer is nearer.
    std::stable_sort(tris.begin(), tris.end(),
                     [](const Tri& x, const Tri& y) { return x.depth < y.depth; });
    const float light[3] = {0.45f, 0.75f, -0.48f};
    for (const Tri& t : tris) {
      double x[3], y[3];
      const uint32_t idx[3] = {t.a, t.b, t.c};
      float n[3] = {0, 0, 0};
      for (int k = 0; k < 3; ++k) {
        project(v, f, &p.pos[size_t{idx[k]} * 3], &x[k], &y[k]);
        for (int c = 0; c < 3; ++c) n[c] += p.nrm[size_t{idx[k]} * 3 + c];
      }
      // Faces turned away from the viewer are not drawn.
      if (v.depth_sign * n[v.depth] < 0) continue;
      const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
      const float lit =
          len > 0 ? std::max(0.0f, (n[0] * light[0] + n[1] * light[1] + n[2] * light[2]) / len) : 0;
      const float k = 0.3f + 0.7f * lit;
      const float* rgb = t.part < m.materials.size() ? m.materials[t.part].rgb : nullptr;
      auto ch = [&](int c) {
        const float lin = (rgb ? rgb[c] : 0.8f) * k;
        return static_cast<uint8_t>(std::clamp(std::sqrt(lin) * 255.0f, 0.0f, 255.0f));
      };
      fill_triangle(pic, x, y, {ch(0), ch(1), ch(2)});
      for (int e = 0; e < 3; ++e) {
        ge::lab::draw_line(pic, x[e], y[e], x[(e + 1) % 3], y[(e + 1) % 3], {255, 255, 255, 40});
      }
    }
  }
  for (size_t j = 0; j < m.joints.size(); ++j) {
    const float* g = p.global[j].m;
    const float at[3] = {g[3], g[7], g[11]};
    double x, y;
    project(v, f, at, &x, &y);
    const int32_t parent = m.joints[j].parent;
    if (parent >= 0) {
      const float* q = p.global[static_cast<size_t>(parent)].m;
      const float pa[3] = {q[3], q[7], q[11]};
      double px, py;
      project(v, f, pa, &px, &py);
      ge::lab::draw_line(pic, px, py, x, y, bone, 2);
    }
    ge::lab::fill_disc(pic, x, y, 3, bone);
    if (labels) ge::lab::draw_text(pic, x + 5, y - 3, m.joints[j].name, kText);
  }
}

// A panel's frame fitting `box` (the reach of every pose) into w x h with a margin.
Frame fit(const a::Box& box, const View& v, int x0, int y0, int w, int h) {
  const double span_x = box.max[v.right] - box.min[v.right];
  const double span_y = box.max[1] - box.min[1];
  const double scale = 0.85 * std::min(w / std::max(span_x, 1e-3), h / std::max(span_y, 1e-3));
  const double mid_x = (box.max[v.right] + box.min[v.right]) / 2.0;
  const double mid_y = (box.max[1] + box.min[1]) / 2.0;
  return {w / 2.0 - v.sign * mid_x * scale, h / 2.0 + mid_y * scale, scale, double(x0), double(y0)};
}

std::string fixed(double v, int places) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", places, v);
  return b;
}

// A line graph of `series` (one a joint, `samples` points each, over `seconds`) in a panel.
void graph(Picture& pic, int x0, int y0, int w, int h,
           const std::vector<std::vector<double>>& series, double seconds, const std::string& title,
           const std::string& unit) {
  double lo = 0, hi = 0;
  for (const auto& s : series) {
    for (double v : s) {
      lo = std::min(lo, v);
      hi = std::max(hi, v);
    }
  }
  if (hi - lo < 1e-9) hi = lo + 1;
  const int l = x0 + 52, r = x0 + w - 8, t = y0 + 18, b = y0 + h - 18;
  ge::lab::draw_text(pic, x0 + 4, y0 + 4, title, kText);
  for (int k = 0; k <= 4; ++k) {
    const double v = lo + (hi - lo) * k / 4;
    const double y = b - (b - t) * k / 4.0;
    ge::lab::draw_line(pic, l, y, r, y, kGrid);
    ge::lab::draw_text(pic, x0 + 2, y - 3, fixed(v, std::fabs(hi - lo) < 10 ? 2 : 0), kText);
  }
  ge::lab::draw_text(pic, l, b + 6, "0 S", kText);
  const std::string end = fixed(seconds, 2) + " S  (" + unit + ")";
  ge::lab::draw_text(pic, r - ge::lab::text_width(end), b + 6, end, kText);
  for (size_t j = 0; j < series.size(); ++j) {
    const auto& s = series[j];
    for (size_t i = 1; i < s.size(); ++i) {
      const double xa = l + (r - l) * double(i - 1) / double(s.size() - 1);
      const double xb = l + (r - l) * double(i) / double(s.size() - 1);
      const double ya = b - (b - t) * (s[i - 1] - lo) / (hi - lo);
      const double yb = b - (b - t) * (s[i] - lo) / (hi - lo);
      ge::lab::draw_line(pic, xa, ya, xb, yb, kSeries[j % 8], 1.5);
    }
  }
}

void run(ge::lab::Lab& lab) {
  const std::string path = lab.arg("model");
  const int panel = static_cast<int>(lab.arg_num("size"));
  const int strip = static_cast<int>(lab.arg_num("frames"));
  if (panel < 64 || panel > 1024 || strip < 2 || strip > 24) {
    lab.fail("--size must be 64..1024 and --frames 2..24",
             "next: rerun with --size=256 --frames=8.");
    return;
  }
  a::LoadResult loaded = path.empty()
                             ? a::load_gltf(a::write_glb(a::make_test_model()), "test_model.glb")
                             : a::load_gltf_file(path);
  if (!loaded.ok) {
    lab.fail(loaded.error,
             "next: fix the file the line names, or run without --model (skill: assets).");
    return;
  }
  for (const std::string& n : loaded.notes) lab.note("loader: " + n);
  const a::Model& m = loaded.model;
  const std::string name = path.empty() ? "test model" : path;
  const std::vector<std::pair<std::string, std::string>> inputs = {{"model", name}};

  // ── What the model is ──
  size_t vertices = 0, triangles = 0, bad_weights = 0, bad_joints = 0, targets = 0;
  for (const a::Mesh& mesh : m.meshes) {
    vertices += mesh.vertex_count();
    targets += mesh.targets.size();
    triangles += mesh.indices.size() / 3;
    for (uint32_t v = 0; v < mesh.vertex_count(); ++v) {
      float sum = 0;
      for (int k = 0; k < 4; ++k) {
        sum += mesh.weights[size_t{v} * 4 + k];
        if (mesh.weights[size_t{v} * 4 + k] > 0 &&
            mesh.joints[size_t{v} * 4 + k] >= m.joints.size())
          ++bad_joints;
      }
      if (std::fabs(sum - 1) > 1e-4f) ++bad_weights;
    }
  }
  lab.metric("joints", static_cast<double>(m.joints.size()), "count");
  lab.metric("meshes", static_cast<double>(m.meshes.size()), "count");
  lab.metric("vertices", static_cast<double>(vertices), "count");
  lab.metric("triangles", static_cast<double>(triangles), "count");
  lab.metric("materials", static_cast<double>(m.materials.size()), "count");
  lab.metric("clips", static_cast<double>(m.clips.size()), "count");
  // A morph model's frames: only a model that has them reports them.
  if (targets) lab.metric("morph-targets", static_cast<double>(targets), "count");
  lab.metric("weights-not-one", static_cast<double>(bad_weights), "count");
  lab.metric("joints-outside", static_cast<double>(bad_joints), "count");
  if (bad_weights || bad_joints) {
    lab.fail(std::to_string(bad_weights) + " vertices whose weights do not sum to 1 and " +
                 std::to_string(bad_joints) + " influences naming no joint",
             "next: normalise the weights and limit them to four before export (skill: assets).");
  }
  ge::lab::LabTable joints{"joints", {"joint", "name", "parent", "rest x", "rest y", "rest z"}, {}};
  for (size_t j = 0; j < m.joints.size(); ++j) {
    const a::Joint& jt = m.joints[j];
    joints.rows.push_back({std::to_string(j), jt.name,
                           jt.parent >= 0 ? m.joints[static_cast<size_t>(jt.parent)].name : "-",
                           fixed(jt.rest.t[0], 3), fixed(jt.rest.t[1], 3), fixed(jt.rest.t[2], 3)});
  }
  lab.table(joints);

  // Every pose's box, so every panel of every picture has one scale.
  a::Box box;
  for (size_t k = 0; k < m.meshes.size(); ++k) {
    const a::Box b = a::reach_box(m, m.meshes[k]);
    for (int c = 0; c < 3; ++c) {
      box.min[c] = k ? std::min(box.min[c], b.min[c]) : b.min[c];
      box.max[c] = k ? std::max(box.max[c], b.max[c]) : b.max[c];
    }
  }
  lab.metric("reach-x", box.max[0] - box.min[0], "m");
  lab.metric("reach-y", box.max[1] - box.min[1], "m");
  lab.metric("reach-z", box.max[2] - box.min[2], "m");

  // ── At bind ──
  {
    Picture pic = ge::lab::create_picture(panel * 2, panel, kBack);
    const Posed rest = pose(m, -1, 0);
    const View views[2] = {kBehind, kRight};
    for (int k = 0; k < 2; ++k) {
      draw_model(pic, m, rest, views[k], fit(box, views[k], panel * k, 0, panel, panel), true,
                 kBone, true);
      ge::lab::draw_text(pic, panel * k + 4, 4, std::string("FROM ") + views[k].name, kText);
    }
    lab.shot({"bind", "MODEL AT REST",
              "The model at its rest pose, shaded by material and lit from above, with its "
              "triangles' edges in faint white and its skeleton in yellow (a disc a joint, a line "
              "to its parent, its name): from behind (+x to the right) and from the right (+z, "
              "forward, to the right).",
              "the joints sit inside the mesh where it bends; no faces missing (a hole is a "
              "triangle wound inward); the model faces +z (to the right in the second panel)",
              inputs},
             pic);
  }

  // ── Each clip ──
  ge::lab::LabTable clips{"clips",
                          {"clip", "frames", "seconds", "max angle (deg)", "at joint",
                           "peak accel (deg/s2)", "at joint", "at s"},
                          {}};
  for (size_t c = 0; c < m.clips.size(); ++c) {
    const a::Clip& clip = m.clips[c];
    const int ci = static_cast<int>(c);
    const double d = clip.duration_s();
    std::vector<std::pair<std::string, std::string>> in = inputs;
    in.push_back({"clip", clip.name});

    // The filmstrip.
    Picture film = ge::lab::create_picture(panel / 2 * strip, panel / 2 + 14, kBack);
    for (int k = 0; k < strip; ++k) {
      const double t = d * k / (strip - 1);
      const Posed p = pose(m, ci, t);
      const int x0 = panel / 2 * k;
      draw_model(film, m, p, kBehind, fit(box, kBehind, x0, 14, panel / 2, panel / 2), true, kBone,
                 false);
      ge::lab::draw_text(film, x0 + 4, 4, fixed(t, 2) + " S", kText);
    }
    lab.shot({"clip-" + clip.name + "-film", "CLIP " + clip.name + ", FILMSTRIP",
              "Clip '" + clip.name + "' at " + std::to_string(strip) +
                  " evenly spaced times from its start to its end, " +
                  "seen from behind, the skeleton over the shaded mesh, every panel at one scale.",
              "the motion reads in order; the mesh follows its bones without folding, stretching "
              "or a candy-wrapper twist at a joint",
              in},
             film);

    // The onion skin: every frame's bones on one picture, fading from first to last.
    Picture onion = ge::lab::create_picture(panel * 2, panel, kBack);
    const View views[2] = {kBehind, kRight};
    for (int v = 0; v < 2; ++v) {
      const Frame f = fit(box, views[v], panel * v, 0, panel, panel);
      for (uint32_t k = 0; k < clip.frames; k += std::max(1u, clip.frames / 30)) {
        const Posed p = pose(m, ci, k / double{a::kClipRateHz});
        const auto alpha = static_cast<uint8_t>(40 + 200 * k / std::max(1u, clip.frames - 1));
        draw_model(onion, m, p, views[v], f, false, {kBone.r, kBone.g, kBone.b, alpha}, false);
      }
      draw_model(onion, m, pose(m, ci, d), views[v], f, false, {255, 255, 255}, true);
      ge::lab::draw_text(onion, panel * v + 4, 4, std::string("FROM ") + views[v].name, kText);
    }
    lab.shot(
        {"clip-" + clip.name + "-onion", "CLIP " + clip.name + ", ONION SKIN",
         "The skeleton of clip '" + clip.name +
             "' at up to 30 frames on one picture, faint at "
             "the start and solid at the end, the last frame in white: from behind and from the "
             "right.",
         "the joints trace smooth arcs; evenly spaced ghosts mean even speed, a gap or a "
         "bunch is a jump or a hold",
         in},
        onion);

    // The joint angles and their acceleration: the second difference of each joint's turn from
    // rest (a rotation vector), at the baked rate.
    std::vector<std::vector<double>> angle(m.joints.size()), accel(m.joints.size());
    std::vector<std::vector<double>> turn(m.joints.size());
    std::vector<a::Trs> local(m.joints.size());
    for (uint32_t k = 0; k < clip.frames; ++k) {
      a::sample_clip(m, clip, k / double{a::kClipRateHz}, false, local);
      for (size_t j = 0; j < m.joints.size(); ++j) {
        double v[3];
        angle[j].push_back(a::turn_from_rest(m.joints[j], local[j], v) * kDeg);
        turn[j].insert(turn[j].end(), v, v + 3);
      }
    }
    double max_angle = 0, max_accel = 0;
    size_t max_angle_j = 0, max_accel_j = 0, max_accel_k = 0;
    const double rate2 = double{a::kClipRateHz} * a::kClipRateHz;
    for (size_t j = 0; j < m.joints.size(); ++j) {
      for (size_t k = 0; k < angle[j].size(); ++k) {
        if (angle[j][k] > max_angle) max_angle = angle[j][k], max_angle_j = j;
        double acc = 0;
        if (k > 0 && k + 1 < angle[j].size()) {
          double d2 = 0;
          for (int e3 = 0; e3 < 3; ++e3) {
            const double e =
                turn[j][(k + 1) * 3 + e3] - 2 * turn[j][k * 3 + e3] + turn[j][(k - 1) * 3 + e3];
            d2 += e * e;
          }
          acc = std::sqrt(d2) * rate2 * kDeg;
        }
        accel[j].push_back(acc);
        if (acc > max_accel) max_accel = acc, max_accel_j = j, max_accel_k = k;
      }
    }
    // The ends have no second difference: they repeat their neighbours'.
    for (auto& s : accel) {
      if (s.size() >= 3) {
        s.front() = s[1];
        s.back() = s[s.size() - 2];
      }
    }
    lab.metric("clip-" + clip.name + ".max-angle", max_angle, "deg");
    lab.metric("clip-" + clip.name + ".peak-accel", max_accel, "deg/s2");
    clips.rows.push_back({clip.name, std::to_string(clip.frames), fixed(d, 3), fixed(max_angle, 1),
                          m.joints.empty() ? "-" : m.joints[max_angle_j].name, fixed(max_accel, 0),
                          m.joints.empty() ? "-" : m.joints[max_accel_j].name,
                          fixed(static_cast<double>(max_accel_k) / a::kClipRateHz, 3)});
    Picture graphs = ge::lab::create_picture(
        panel * 3, panel + 14 * static_cast<int>((m.joints.size() + 3) / 4) + 8, kBack);
    graph(graphs, 0, 0, panel * 3 / 2, panel, angle, d, "ANGLE FROM REST", "DEG");
    graph(graphs, panel * 3 / 2, 0, panel * 3 / 2, panel, accel, d, "ANGULAR ACCELERATION",
          "DEG/S2");
    for (size_t j = 0; j < m.joints.size(); ++j) {
      const int x = 8 + static_cast<int>(j % 4) * (panel * 3 / 4);
      const int y = panel + 4 + static_cast<int>(j / 4) * 14;
      ge::lab::fill_rect(graphs, x, y + 1, 10, 6, kSeries[j % 8]);
      ge::lab::draw_text(graphs, x + 14, y, m.joints[j].name, kText);
    }
    lab.shot(
        {"clip-" + clip.name + "-angles", "CLIP " + clip.name + ", JOINT ANGLES",
         "Each joint's angle from its rest over clip '" + clip.name +
             "' (left, degrees) and "
             "the size of its angular acceleration, the second difference of its turn at the "
             "baked rate (right, degrees a second squared), one colour a joint as the key below "
             "says.",
         "smooth curves; a spike in the acceleration is a pop or a jolt at that time; an "
         "angle past a joint's range is a joint bent the wrong way",
         in},
        graphs);
  }
  lab.table(clips);
}

const ge::lab::LabSpec kSpec{
    "models",
    ge::lab::LabKind::picture,
    "engine",
    "engine/assets (the glTF loader, the Model, posing)",
    "assets",
    "What does a model look like at rest and through each clip, and do its weights and its "
    "joints' motion hold?",
    {
        {"model", "", "a .glb or .gltf to load (default: the test model through a .glb)"},
        {"size", "256", "a panel's side, px"},
        {"frames", "8", "a filmstrip's frames"},
    },
    "--size=128 --frames=4",
    run,
};

GE_LAB(kSpec);

}  // namespace
