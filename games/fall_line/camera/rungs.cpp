// Fall Line's ladder rows, the camera key and the hand-overs.
//
// ge:summary Fall Line's ladder rows, the camera key and the hand-overs
// ge:keywords fall-line camera ladder rows camera-key hand-over
// ge:skill camera
// ge:test tests/fall_line_camera_test.cpp
#include "fall_line/camera/rungs.hpp"

#include <array>

#include "camera/ladders.hpp"

namespace fall_line::camera {

namespace cam = ge::camera;

namespace {

constexpr std::array<std::string_view, kRungs> kNames = {"tips", "helmet", "chase",
                                                         "far",  "high",   "orbit"};

// A hand-over between two rungs, s: long enough to read as a move, short enough to answer a key.
constexpr double kHandOverS = 0.6;
// The orbit is a drone flying in from its own circle: slower.
constexpr double kOrbitS = 1.0;

cam::Rig reference(std::string_view name) {
  for (const cam::Rig& r : cam::reference_ladder()) {
    if (r.name == name) return r;
  }
  return cam::Rig{};
}

std::array<cam::Rig, kRungs> build() {
  std::array<cam::Rig, kRungs> l{};
  // The worn rows' eyes are placeholders: `fit_worn` sets them off the figure every frame.
  cam::MountedRow tips = reference("tips").mounted;
  tips.eye_body = {0, 0.3, 0.55};
  tips.down = 0.12;
  l[0] = cam::mounted_rig(kNames[0], tips);
  l[1] = cam::mounted_rig(kNames[1], reference("helmet").mounted);
  for (int i = 2; i < kRungs; ++i) {
    l[static_cast<size_t>(i)] = reference(kNames[static_cast<size_t>(i)]);
  }
  return l;
}

// Built on first use: the reference rows are another file's statics.
const std::array<cam::Rig, kRungs>& ladder() {
  static const std::array<cam::Rig, kRungs> l = build();
  return l;
}

bool worn(Rung r) { return r == Rung::tips || r == Rung::helmet; }

}  // namespace

std::span<const cam::Rig> fall_line_ladder() { return ladder(); }

std::string_view rung_name(Rung r) { return kNames[static_cast<size_t>(r)]; }

bool rung_named(std::string_view name, Rung& out) {
  for (size_t i = 0; i < kNames.size(); ++i) {
    if (kNames[i] == name) {
      out = static_cast<Rung>(i);
      return true;
    }
  }
  return false;
}

Rung next_rung(Rung r) {
  switch (r) {
    case Rung::tips:
      return Rung::helmet;
    case Rung::helmet:
      return Rung::chase;
    case Rung::chase:
      return Rung::far;
    case Rung::far:
      return Rung::high;
    case Rung::high:
      return Rung::tips;
    case Rung::orbit:
      return Rung::chase;
  }
  return Rung::chase;
}

HandOver hand_over(Rung from, Rung to) {
  if (from == Rung::orbit || to == Rung::orbit) return {cam::Edit::blend, kOrbitS};
  if (worn(from) || worn(to)) return {cam::Edit::blend, kHandOverS};
  return {cam::Edit::inertialize, kHandOverS};
}

cam::Camera make_run_camera(Rung start, uint32_t seed) {
  cam::Camera c = cam::make_camera(ladder().data(), kRungs, seed);
  c.rung = static_cast<int>(start);
  return c;
}

Rung rung_of(const cam::Camera& c) { return static_cast<Rung>(c.rung); }

void change_rung(cam::Camera& c, Rung to) {
  const HandOver h = hand_over(rung_of(c), to);
  cam::set_rung(c, static_cast<int>(to), h.edit, h.seconds);
}

}  // namespace fall_line::camera
