// The animation views' bodies: the track, the measures, the findings, the filmstrip and the onion
// skin, and the charts handed to chart.hpp.
//
// ge:summary The animation views' bodies: measures, findings, filmstrips, onion skins, charts
// ge:keywords anim pose track measure finding filmstrip onion-skin projection jolt heatmap
// ge:skill debug labs
// ge:test tests/lab_anim_test.cpp
#include "lab/anim.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "core/libm/libm.hpp"
#include "core/num.hpp"
#include "core/num_text.hpp"

namespace ge::lab {

namespace {

using core::Vec3;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kDeg = 180 / 3.14159265358979323846;
constexpr double kLog10e = 0.43429448190325182765;
constexpr size_t kWindow = 9;  // the neighbours a spike is measured against
constexpr size_t kMerge = 3;   // frames within which one fault on one joint is one finding

constexpr Rgba kPaper{250, 250, 252};
constexpr Rgba kInk{20, 20, 30};
constexpr Rgba kInk2{110, 110, 120};
constexpr Rgba kWire{150, 156, 168, 150};
constexpr Rgba kLeft{42, 120, 214};
constexpr Rgba kRight{235, 104, 52};
constexpr Rgba kCentre{40, 40, 50};
constexpr Rgba kRing{208, 40, 40};
constexpr Rgba kFloor{120, 170, 120};
constexpr Rgba kPaths[] = {{42, 120, 214},  {235, 104, 52}, {27, 175, 122}, {237, 161, 0},
                           {232, 123, 164}, {0, 131, 0},    {98, 80, 214}};

Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
double dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
double len(Vec3 a) { return std::sqrt(dot(a, a)); }
Vec3 cross(Vec3 a, Vec3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

std::string fixed(double v, int d) { return core::num::to_fixed(v, d); }

// The value less the median of its neighbours (`kWindow` of them, itself left out).
std::vector<double> excess(const std::vector<double>& v) {
  std::vector<double> out(v.size());
  std::vector<double> near;
  near.reserve(kWindow);
  const size_t half = kWindow / 2;
  for (size_t i = 0; i < v.size(); i++) {
    near.clear();
    const size_t lo = i > half ? i - half : 0;
    const size_t hi = std::min(v.size(), i + half + 1);
    for (size_t k = lo; k < hi; k++) {
      if (k != i && std::isfinite(v[k])) near.push_back(v[k]);
    }
    out[i] = near.empty() ? 0 : v[i] - median(near);
  }
  return out;
}

std::vector<double> column(const std::vector<double>& m, size_t joint, size_t joints) {
  std::vector<double> out(m.size() / joints);
  for (size_t f = 0; f < out.size(); f++) out[f] = m[f * joints + joint];
  return out;
}

int first_child(const Rig& rig, size_t joint) {
  for (size_t k = 0; k < rig.parent.size(); k++) {
    if (rig.parent[k] == static_cast<int>(joint)) return static_cast<int>(k);
  }
  return -1;
}

Rgba side_colour(const Rig& rig, size_t joint) {
  const int s = joint < rig.side.size() ? rig.side[joint] : 0;
  return s < 0 ? kLeft : s > 0 ? kRight : kCentre;
}

// The view's two screen axes: u to the right, v up.
void project(AnimView view, Vec3 p, double& u, double& v) {
  switch (view) {
    case AnimView::side:  // from the figure's right: forward is to the right of the picture
      u = p.z, v = p.y;
      return;
    case AnimView::behind:
      u = p.x, v = p.y;
      return;
    case AnimView::above:  // forward is up the picture
      u = p.x, v = p.z;
      return;
  }
}

struct Box {
  double u0 = std::numeric_limits<double>::infinity(), u1 = -u0, v0 = u0, v1 = -u0;
  void add(double u, double v) {
    u0 = std::min(u0, u), u1 = std::max(u1, u);
    v0 = std::min(v0, v), v1 = std::max(v1, v);
  }
};

// Every joint and wire point of `frames` (with the floor in a side or behind view).
Box bounds(const PoseTrack& t, AnimView view, const std::vector<size_t>& frames) {
  Box b;
  double u = 0, v = 0;
  for (const size_t f : frames) {
    for (size_t j = 0; j < t.rig.joints.size(); j++) {
      project(view, t.at(f, j), u, v);
      b.add(u, v);
    }
    for (size_t k = 0; k < t.wire_per_frame; k++) {
      const WireSeg& s = t.wire[f * t.wire_per_frame + k];
      project(view, s.a, u, v);
      b.add(u, v);
      project(view, s.b, u, v);
      b.add(u, v);
    }
  }
  if (t.has_floor && view != AnimView::above) b.add(b.u0, t.floor);
  if (!(b.u1 > b.u0)) b.u0 -= 0.5, b.u1 += 0.5;
  if (!(b.v1 > b.v0)) b.v0 -= 0.5, b.v1 += 0.5;
  return b;
}

// Picture coordinates for a panel at (x0, y0) of side `size` showing `box` at `scale`.
struct Mapper {
  AnimView view;
  double x0, y0, size, scale, cu, cv;
  void at(Vec3 p, double& x, double& y) const {
    double u = 0, v = 0;
    project(view, p, u, v);
    x = x0 + size / 2 + (u - cu) * scale;
    y = y0 + size / 2 - (v - cv) * scale;
  }
};

Mapper mapper(AnimView view, const Box& b, double x0, double y0, double size, double pad) {
  const double span = std::max(b.u1 - b.u0, b.v1 - b.v0);
  return Mapper{view, x0, y0, size, (size - 2 * pad) / span, (b.u0 + b.u1) / 2, (b.v0 + b.v1) / 2};
}

void draw_pose(Picture& p, const PoseTrack& t, size_t f, const Mapper& m, bool wire, bool bones,
               uint8_t alpha) {
  double xa = 0, ya = 0, xb = 0, yb = 0;
  if (wire) {
    Rgba c = kWire;
    c.a = static_cast<uint8_t>(c.a * alpha / 255);
    for (size_t k = 0; k < t.wire_per_frame; k++) {
      const WireSeg& s = t.wire[f * t.wire_per_frame + k];
      m.at(s.a, xa, ya);
      m.at(s.b, xb, yb);
      draw_line(p, xa, ya, xb, yb, c);
    }
  }
  if (!bones) return;
  for (size_t j = 0; j < t.rig.joints.size(); j++) {
    Rgba c = side_colour(t.rig, j);
    c.a = alpha;
    m.at(t.at(f, j), xb, yb);
    const int parent = t.rig.parent[j];
    if (parent >= 0) {
      m.at(t.at(f, static_cast<size_t>(parent)), xa, ya);
      draw_line(p, xa, ya, xb, yb, c, 2);
    }
    fill_disc(p, xb, yb, 2, c);
  }
}

void draw_floor(Picture& p, const PoseTrack& t, const Mapper& m) {
  if (!t.has_floor || m.view == AnimView::above) return;
  double x = 0, y = 0;
  m.at(Vec3{0, t.floor, 0}, x, y);
  draw_line(p, m.x0 + 2, y, m.x0 + m.size - 3, y, kFloor);
}

void add_finding(std::vector<AnimFinding>& out, const char* kind, size_t joint, size_t frame,
                 double value, double usual, const std::string& unit) {
  // One fault on one joint within kMerge frames of the last is the same finding: keep the worst.
  for (auto it = out.rbegin(); it != out.rend(); ++it) {
    if (it->joint != joint || it->kind != kind) continue;
    if (frame <= it->last + kMerge && frame + kMerge >= it->first) {
      it->first = std::min(it->first, frame);
      it->last = std::max(it->last, frame);
      if (std::fabs(value) > std::fabs(it->value)) {
        it->frame = frame;
        it->value = value;
        it->usual = usual;
      }
      return;
    }
    break;
  }
  AnimFinding a;
  a.kind = kind;
  a.joint = joint;
  a.frame = a.first = a.last = frame;
  a.value = value;
  a.usual = usual;
  a.note = unit;  // the unit until the sentence is written
  out.push_back(a);
}

std::string sentence(const PoseTrack& t, const AnimFinding& a) {
  const std::string& name = t.rig.joints[a.joint];
  const std::string when = "frame " + std::to_string(a.frame) + " (t " +
                           fixed(static_cast<double>(a.frame) * t.dt, 2) + " s)";
  const std::string span =
      a.last > a.first ? ", frames " + std::to_string(a.first) + ".." + std::to_string(a.last) : "";
  if (a.kind == "stretch") {
    return "stretch: the bone to " + name + " is " + fixed(a.value * 100, 1) +
           "% off its median length at " + when + span;
  }
  if (a.kind == "clip") {
    return "clip: " + name + " is " + fixed(a.value, 3) + " m under the floor at " + when + span;
  }
  return a.kind + ": " + name + " " + fixed(a.value, 1) + " " + a.note + " at " + when + span +
         ", its neighbours " + fixed(a.usual, 1);
}

}  // namespace

void PoseTrack::add_frame(std::span<const Vec3> pose, std::span<const WireSeg> segs) {
  joints.insert(joints.end(), pose.begin(), pose.end());
  if (joints.size() == pose.size()) wire_per_frame = segs.size();
  wire.insert(wire.end(), segs.begin(), segs.end());
}

void add_box_wire(std::vector<WireSeg>& out, Vec3 centre, core::Quat rot, const double size[3]) {
  Vec3 c[8];
  for (int i = 0; i < 8; i++) {
    const Vec3 local{(i & 1 ? 0.5 : -0.5) * size[0], (i & 2 ? 0.5 : -0.5) * size[1],
                     (i & 4 ? 0.5 : -0.5) * size[2]};
    const Vec3 r = core::rotate(rot, local);
    c[i] = Vec3{centre.x + r.x, centre.y + r.y, centre.z + r.z};
  }
  // The edges join corners one bit apart.
  for (int i = 0; i < 8; i++) {
    for (const int bit : {1, 2, 4}) {
      if (!(i & bit)) out.push_back(WireSeg{c[i], c[i | bit]});
    }
  }
}

Motion measure_motion(const PoseTrack& t) {
  const size_t nj = t.rig.joints.size();
  const size_t nf = t.frames();
  Motion m;
  m.speed.assign(nf * nj, 0);
  m.accel.assign(nf * nj, 0);
  m.jolt.assign(nf * nj, 0);
  m.stretch.assign(nf * nj, 0);
  m.angle.assign(nf * nj, kNaN);
  m.angle_rate.assign(nf * nj, 0);
  const double dt = t.dt;
  for (size_t j = 0; j < nj; j++) {
    for (size_t f = 1; f < nf; f++) m.speed[f * nj + j] = len(sub(t.at(f, j), t.at(f - 1, j))) / dt;
    if (nf > 1) m.speed[j] = m.speed[nj + j];
    for (size_t f = 1; f + 1 < nf; f++) {
      const Vec3 a = t.at(f - 1, j), b = t.at(f, j), c = t.at(f + 1, j);
      const Vec3 d{c.x - 2 * b.x + a.x, c.y - 2 * b.y + a.y, c.z - 2 * b.z + a.z};
      m.accel[f * nj + j] = len(d) / (dt * dt);
    }
    for (size_t f = 1; f + 2 < nf; f++) {
      const Vec3 a = t.at(f - 1, j), b = t.at(f, j), c = t.at(f + 1, j), d = t.at(f + 2, j);
      const Vec3 e{d.x - 3 * c.x + 3 * b.x - a.x, d.y - 3 * c.y + 3 * b.y - a.y,
                   d.z - 3 * c.z + 3 * b.z - a.z};
      m.jolt[f * nj + j] = len(e) / (dt * dt * dt);
    }
    const int parent = t.rig.parent[j];
    if (parent >= 0) {
      const auto p = static_cast<size_t>(parent);
      std::vector<double> lengths(nf);
      for (size_t f = 0; f < nf; f++) lengths[f] = len(sub(t.at(f, j), t.at(f, p)));
      const double mid = median(lengths);
      for (size_t f = 0; f < nf; f++) m.stretch[f * nj + j] = mid > 0 ? lengths[f] / mid - 1 : 0;
      const int child = first_child(t.rig, j);
      if (child >= 0) {
        const auto c = static_cast<size_t>(child);
        for (size_t f = 0; f < nf; f++) {
          const Vec3 a = sub(t.at(f, p), t.at(f, j));
          const Vec3 b = sub(t.at(f, c), t.at(f, j));
          m.angle[f * nj + j] = core::libm::atan2(len(cross(a, b)), dot(a, b)) * kDeg;
        }
        for (size_t f = 1; f < nf; f++) {
          m.angle_rate[f * nj + j] =
              std::fabs(m.angle[f * nj + j] - m.angle[(f - 1) * nj + j]) / dt;
        }
      }
    }
  }
  return m;
}

std::vector<AnimFinding> anim_findings(const PoseTrack& t, const Motion& m,
                                       const AnimLimits& limits) {
  const size_t nj = t.rig.joints.size();
  const size_t nf = t.frames();
  std::vector<AnimFinding> all;
  for (size_t j = 0; j < nj; j++) {
    std::vector<AnimFinding> found;
    // A spike past its floor, as a finding of `kind`. The pops come first so a snap or an angle
    // snap beside one is left to it.
    const auto spiky = [&](const std::vector<double>& v, double floor, const char* kind,
                           const char* unit, bool beside_pop) {
      const std::vector<double> ex = excess(v);
      std::vector<size_t> hits = spikes(v, kWindow, limits.z);
      std::sort(hits.begin(), hits.end());
      for (const size_t f : hits) {
        if (ex[f] < floor) continue;
        bool near_pop = false;
        for (const AnimFinding& a : found) {
          if (a.kind == "pop" && f + kMerge >= a.first && f <= a.last + kMerge) near_pop = true;
        }
        if (near_pop && !beside_pop) continue;
        add_finding(found, kind, j, f, v[f], v[f] - ex[f], unit);
      }
    };
    spiky(column(m.speed, j, nj), limits.pop, "pop", "m/s", true);
    spiky(column(m.accel, j, nj), limits.snap, "snap", "m/s^2", false);
    if (first_child(t.rig, j) >= 0 && t.rig.parent[j] >= 0) {
      spiky(column(m.angle_rate, j, nj), limits.angle_snap, "angle-snap", "deg/s", false);
    }
    if (t.rig.parent[j] >= 0) {
      for (size_t f = 0; f < nf; f++) {
        const double s = m.stretch[f * nj + j];
        if (std::fabs(s) > limits.stretch) add_finding(found, "stretch", j, f, s, 0, "");
      }
    }
    if (t.has_floor) {
      for (size_t f = 0; f < nf; f++) {
        const double under = t.floor - t.at(f, j).y;
        if (under > limits.clip) add_finding(found, "clip", j, f, under, 0, "m");
      }
    }
    all.insert(all.end(), found.begin(), found.end());
  }
  for (AnimFinding& a : all) {
    // A pop is the frame the joint jumped TO: a one-frame blip spikes the speed twice, going out
    // and coming back, and the first is the frame it is out of place.
    if (a.kind == "pop" && a.frame != a.first) {
      a.frame = a.first;
      a.value = m.speed[a.first * nj + a.joint];
    }
    a.note = sentence(t, a);
  }
  std::stable_sort(all.begin(), all.end(), [](const AnimFinding& a, const AnimFinding& b) {
    return a.frame != b.frame ? a.frame < b.frame : a.joint < b.joint;
  });
  return all;
}

const char* view_name(AnimView view) {
  switch (view) {
    case AnimView::side:
      return "side";
    case AnimView::behind:
      return "behind";
    case AnimView::above:
      return "above";
  }
  return "side";
}

std::vector<size_t> spread_frames(const PoseTrack& t, size_t count) {
  std::vector<size_t> out;
  const size_t nf = t.frames();
  if (nf == 0 || count == 0) return out;
  if (count == 1 || nf == 1) return {0};
  out.reserve(count);
  for (size_t i = 0; i < count; i++) {
    const size_t f = (nf - 1) * i / (count - 1);
    if (out.empty() || out.back() != f) out.push_back(f);
  }
  return out;
}

Picture filmstrip(const PoseTrack& t, const StripSpec& spec) {
  const size_t n = std::max<size_t>(1, spec.frames.size());
  const auto cols = static_cast<size_t>(std::max(1, spec.columns));
  const size_t across = std::min(n, cols);
  const size_t down = (n + cols - 1) / cols;
  const double size = spec.panel;
  constexpr double kLabel = 12;
  Picture p = create_picture(static_cast<int>(size * static_cast<double>(across)),
                             static_cast<int>((size + kLabel) * static_cast<double>(down)), kPaper);
  if (spec.frames.empty()) return p;
  const Box box = bounds(t, spec.view, spec.frames);
  for (size_t i = 0; i < spec.frames.size(); i++) {
    const size_t f = spec.frames[i];
    const size_t col = i % cols, row = (i - col) / cols;
    const double x0 = size * static_cast<double>(col);
    const double y0 = (size + kLabel) * static_cast<double>(row);
    const Mapper m = mapper(spec.view, box, x0, y0, size, 10);
    draw_line(p, x0 + size - 1, y0, x0 + size - 1, y0 + size + kLabel, Rgba{220, 220, 226});
    draw_floor(p, t, m);
    draw_pose(p, t, f, m, spec.wire, spec.bones, 255);
    if (spec.rings) {
      for (const AnimFinding& a : *spec.rings) {
        if (a.frame != f) continue;
        double x = 0, y = 0;
        m.at(t.at(f, a.joint), x, y);
        draw_ring(p, x, y, 7, kRing);
        draw_ring(p, x, y, 8, kRing);
      }
    }
    draw_text(p, x0 + 4, y0 + size,
              "F" + std::to_string(f) + " " + fixed(static_cast<double>(f) * t.dt, 2) + "S", kInk2);
  }
  draw_text(p, 4, 3, view_name(spec.view), kInk);
  return p;
}

Picture onion_skin(const PoseTrack& t, AnimView view, const std::vector<size_t>& frames, int size) {
  Picture p = create_picture(size, size + 14, kPaper);
  if (frames.empty()) return p;
  const size_t lo = *std::min_element(frames.begin(), frames.end());
  const size_t hi = *std::max_element(frames.begin(), frames.end());
  std::vector<size_t> span;
  span.reserve(hi - lo + 1);
  for (size_t f = lo; f <= hi; f++) span.push_back(f);
  const Mapper m = mapper(view, bounds(t, view, span), 0, 0, size, 14);
  draw_floor(p, t, m);
  for (size_t i = 0; i < frames.size(); i++) {
    // The earliest at a fifth of full strength, the last whole.
    const double k =
        frames.size() > 1 ? static_cast<double>(i) / static_cast<double>(frames.size() - 1) : 1.0;
    draw_pose(p, t, frames[i], m, false, true,
              static_cast<uint8_t>(core::num::round(50 + 205 * k)));
  }
  double xa = 0, ya = 0, xb = 0, yb = 0;
  for (size_t e = 0; e < t.rig.ends.size(); e++) {
    const auto j = static_cast<size_t>(t.rig.ends[e]);
    const Rgba c = kPaths[e % 7];
    for (size_t f = lo + 1; f <= hi; f++) {
      m.at(t.at(f - 1, j), xa, ya);
      m.at(t.at(f, j), xb, yb);
      draw_line(p, xa, ya, xb, yb, c);
    }
    const double room = size - 2.0 - text_width(t.rig.joints[j]);
    draw_text(p, std::min(xb + 4, room), yb - 3, t.rig.joints[j], c);
  }
  draw_text(p, 4, 3,
            std::string(view_name(view)) + " F" + std::to_string(lo) + "-" + std::to_string(hi),
            kInk);
  return p;
}

Chart angle_chart(const PoseTrack& t, const Motion& m, const std::vector<size_t>& joints,
                  const ChartFrame& frame) {
  LineChart spec;
  spec.frame = frame;
  const size_t nj = t.rig.joints.size();
  const size_t nf = t.frames();
  for (const size_t j : joints) {
    Trace tr;
    tr.label = t.rig.joints[j];
    tr.x.reserve(nf);
    tr.y.reserve(nf);
    for (size_t f = 0; f < nf; f++) {
      tr.x.push_back(static_cast<double>(f) * t.dt);
      tr.y.push_back(m.angle[f * nj + j]);
    }
    spec.traces.push_back(std::move(tr));
  }
  return line_chart(spec);
}

Chart jolt_heatmap(const PoseTrack& t, const Motion& m, const ChartFrame& frame) {
  // At most 240 columns: each the largest jolt of its frames, on a log scale (a jolt runs from
  // a few m/s^3 to millions where a joint pops).
  const size_t nj = t.rig.joints.size();
  const size_t nf = t.frames();
  const size_t cols = std::max<size_t>(1, std::min<size_t>(240, nf));
  std::vector<double> values(nj * cols, 0);
  for (size_t j = 0; j < nj; j++) {
    for (size_t f = 0; f < nf; f++) {
      const size_t c = f * cols / nf;
      double& cell = values[j * cols + c];
      cell = std::max(cell, core::libm::log(1 + m.jolt[f * nj + j]) * kLog10e);
    }
  }
  return heatmap_chart(frame, t.rig.joints, values, 0, static_cast<double>(nf) * t.dt);
}

Series motion_series(const PoseTrack& t, const Motion& m) {
  const size_t nj = t.rig.joints.size();
  std::vector<std::string> names{"t_s"};
  std::vector<bool> angled(nj);
  for (size_t j = 0; j < nj; j++) {
    names.push_back(t.rig.joints[j] + ".accel");
    names.push_back(t.rig.joints[j] + ".jolt");
    angled[j] = t.rig.parent[j] >= 0 && first_child(t.rig, j) >= 0;
    if (angled[j]) names.push_back(t.rig.joints[j] + ".angle");
  }
  Series s = make_series(names);
  s.set("x", "t_s");
  for (size_t j = 0; j < nj; j++) {
    s.set("unit." + t.rig.joints[j] + ".accel", "m/s^2");
    s.set("unit." + t.rig.joints[j] + ".jolt", "m/s^3");
    if (angled[j]) s.set("unit." + t.rig.joints[j] + ".angle", "deg");
  }
  std::vector<double> row;
  row.reserve(names.size());
  for (size_t f = 0; f < t.frames(); f++) {
    row.clear();
    row.push_back(static_cast<double>(f) * t.dt);
    for (size_t j = 0; j < nj; j++) {
      row.push_back(m.accel[f * nj + j]);
      row.push_back(m.jolt[f * nj + j]);
      if (angled[j]) row.push_back(m.angle[f * nj + j]);
    }
    add_row(s, row);
  }
  return s;
}

}  // namespace ge::lab
