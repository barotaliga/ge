// THE CAMERA STACK: a game's LADDER of rigs (up to `kMaxRigs`, each a kind and its row), their
// states, the rung in use, the transition in flight, the trauma shake and the player's settings,
// all fixed-size and made once. Once a display frame:
//
//   Camera cam = make_camera(ladder, count, seed);
//   set_rung(cam, find_rung(cam, "helmet"), Edit::inertialize, 0.35);
//   Pose p = update(cam, subject, world, dt);   // then to_view.hpp's to_frame_view
//
// Every rig is kept WARM (framed every frame behind the scenes), so a change to it starts from
// where it would be, and an inertialized change knows the arriving rig's own velocity. A director
// (a broadcast, a replay's film, an impact camera) is the game's own function calling `set_rung`
// with a planted rig and an edit; the stack knows nothing of a TV.
//
// THE MOMENTS the stack itself answers (`Subject::events`): an explosion adds its size as trauma;
// a landing on a rig that is not worn by the body adds trauma past `land_from_mps`; a teleport
// cancels any transition and snaps every rig. Hits and shots belong to the rigs (the first
// person's kick, the mounted head's throw); an outside lens does not feel the subject's knocks.
//
// THE SETTINGS (`CameraSettings`, the cvars' struct, docs/camera.md § Settings and cvars):
// `motion` scales every motion the player did not command (the bob, the roll, the tremor, the
// neck, the shakes), `shake` the shakes alone, `blend` every transition's length (0 cuts), and
// `fov_speed` a boom's stretch; `fov_h` (rad, horizontal at 4:3; 0 keeps the rows') overrides a
// first-person rig's lens.
//
// ge:summary The camera stack: a ladder of rigs kept warm, the rung, transitions, shake, settings
// ge:keywords camera stack ladder rig rung switch view-change camera-mode warm transition shake
// ge:keywords settings comfort motion fov make_camera set_rung find_rung update snap in_transition
// ge:keywords first_person_rig boom_rig mounted_rig orbit_rig planted_rig free_rig
// ge:skill camera
// ge:test tests/camera_camera_test.cpp
#pragma once

#include <cstdint>
#include <string_view>

#include "camera/boom.hpp"
#include "camera/first_person.hpp"
#include "camera/lenses.hpp"
#include "camera/mounted.hpp"
#include "camera/pose.hpp"
#include "camera/shake.hpp"
#include "camera/subject.hpp"
#include "camera/transition.hpp"
#include "camera/world.hpp"

namespace ge::camera {

enum class RigKind : uint8_t { first_person, boom, mounted, orbit, planted, free };

// One rung: its name, its kind and the row of that kind (the other rows are unused).
struct Rig {
  std::string_view name;
  RigKind kind = RigKind::boom;
  FirstPersonRow first_person{};
  BoomRow boom{};
  MountedRow mounted{};
  OrbitRow orbit{};
  PlantedRow planted{};
  FreeRow free{};
};
Rig first_person_rig(std::string_view name, const FirstPersonRow& row);
Rig boom_rig(std::string_view name, const BoomRow& row);
Rig mounted_rig(std::string_view name, const MountedRow& row);
Rig orbit_rig(std::string_view name, const OrbitRow& row);
Rig planted_rig(std::string_view name, const PlantedRow& row);
Rig free_rig(std::string_view name, const FreeRow& row);

struct RigState {
  FirstPersonState first_person{};
  BoomState boom{};
  MountedState mounted{};
  OrbitState orbit{};
  PlantedState planted{};
};

struct CameraSettings {
  double motion = 1.0;     // 0..1: every uncommanded motion
  double shake = 1.0;      // 0..1: the shakes alone
  double blend = 1.0;      // a share of every transition's length; 0 cuts
  double fov_speed = 1.0;  // 0..1: a boom's stretch with speed
  double fov_h = 0.0;      // rad, horizontal at 4:3, for first-person rigs; 0: the rows' own
};

inline constexpr int kMaxRigs = 8;

struct Camera {
  Rig rigs[kMaxRigs]{};
  RigState states[kMaxRigs]{};
  Local last[kMaxRigs]{};       // each rig's last frame, in the subject's frame
  Local last_rate[kMaxRigs]{};  // and its rate
  bool framed[kMaxRigs]{};      // whether it has a last frame
  int count = 0;
  int rung = 0;
  // The transition in flight.
  Edit edit = Edit::cut;
  int from = -1;  // a blend's old rung; -1 when none is in flight
  double t = 0, duration = 0;
  Inertial inertial{};
  bool inertial_on = false;
  double floor_m = 0.6;  // the blend's floor over the ground at its middle
  // The picture's last frame (after the transition), in the subject's frame, and its rate.
  Local out{}, out_rate{};
  bool has_out = false;
  ShakeRow shake_row{};
  ShakeState shake{};
  double land_from_mps = 6.0, land_full_mps = 20.0, land_trauma = 0.5;
  CameraSettings settings{};
};

Camera make_camera(const Rig* rigs, int count, uint32_t seed);
// The rung named `name`, or -1.
int find_rung(const Camera& cam, std::string_view name);
// Change rung by an edit lasting `duration_s` (scaled by the settings' blend). An inertialized
// change to a rig with no frame yet is a blend.
void set_rung(Camera& cam, int rung, Edit edit, double duration_s);
// Snap every rig onto the subject on the next frame and drop any transition.
void snap(Camera& cam);
void add_trauma(Camera& cam, double amount);
bool in_transition(const Camera& cam);

Pose update(Camera& cam, const Subject& s, const World& w, double dt);

}  // namespace ge::camera
