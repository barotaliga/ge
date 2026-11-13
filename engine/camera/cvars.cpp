// The camera's cvars: registered with a game's defaults, applied over its ladder's rows.
//
// ge:summary The camera's cvars registered with a game's defaults and applied over its ladder
// ge:keywords camera cvars add_camera_cvars apply_camera_cvars world-unit degrees
// ge:skill camera console
// ge:test tests/camera_cvars_test.cpp
#include "camera/cvars.hpp"

#include <algorithm>
#include <string>

#include "core/math.hpp"
#include "core/num_text.hpp"

namespace ge::camera {

namespace con = ge::console;

namespace {

constexpr double kDeg = core::kPi / 180;

std::string text(double v) { return core::num::to_string(v); }

}  // namespace

void add_camera_cvars(con::Cvars& c, CameraCvars& out, const CameraDefaults& d) {
  out.unit_m = d.unit_m;
  constexpr uint32_t kPlayer = con::kArchive;
  struct Row {
    std::string_view name;
    double fallback, min, max;
    std::string_view help;
    std::string_view values;  // console/words.hpp
    double* field;
  };
  const Row rows[] = {
      {"cl_fov", d.fov, 0, 170, "The first-person lens's horizontal angle at 4:3, in degrees.",
       "0: the rig's own; 0..170: that many degrees", &out.fov},
      {"cl_viewheight", d.viewheight, 0, 1e4,
       "How high the first-person eyes sit over the body, in world units.",
       "0: the rig's own; 0..10000: that many units", &out.viewheight},
      {"cl_bob", d.bob, 0, 0.2,
       "How much the view bobs with the stride, for each unit of ground speed.",
       "0: no bob; 0..0.2: that much", &out.bob},
      {"cl_bobcycle", d.bobcycle, 0.05, 5, "How long a stride's bob takes, in seconds.",
       "0.05..5: that many seconds", &out.bobcycle},
      {"cl_bobup", d.bobup, 0.01, 0.99, "The share of a stride the view spends rising.",
       "0.01..0.99: that share", &out.bobup},
      {"cl_rollangle", d.rollangle, 0, 30, "How far the view rolls at full side speed, in degrees.",
       "0: no roll; 0..30: that many degrees", &out.rollangle},
      {"cl_rollspeed", d.rollspeed, 0.01, 1e5,
       "The side speed that rolls the view all of cl_rollangle, in world units a second.",
       "0.01..100000: that speed", &out.rollspeed},
      {"v_kicktime", d.kicktime, 0, 5, "How long a hit's view kick lasts, in seconds.",
       "0: no kick; 0..5: that many seconds", &out.kicktime},
      {"v_kickpitch", d.kickpitch, 0, 30,
       "How far a hit kicks the view in pitch, in degrees for each unit of its size.",
       "0: none; 0..30: that many degrees", &out.kickpitch},
      {"v_kickroll", d.kickroll, 0, 30,
       "How far a hit kicks the view in roll, in degrees for each unit of its size.",
       "0: none; 0..30: that many degrees", &out.kickroll},
      {"v_idlescale", d.idlescale, 0, 10, "How far the view sways at most when idle, in degrees.",
       "0: no sway; 0..10: that many degrees", &out.idlescale},
      {"cl_landdip", d.landdip, 0, 4,
       "How far the view dips on landing, as a share of the rig's own.",
       "0: no dip; 1: the rig's own; 0..4: that share", &out.landdip},
      {"cl_chasedist", d.chasedist, 0, 1e5,
       "How far behind the chase camera's arm reaches, in world units.",
       "0: the rig's own; 0..100000: that many units", &out.chasedist},
      {"cl_chaseheight", d.chaseheight, 0, 1e5,
       "How far up the chase camera's arm reaches, in world units.",
       "0: the rig's own; 0..100000: that many units", &out.chaseheight},
      {"cl_fovspeed", d.fovspeed, 0, 1, "How much a following lens widens with speed.",
       "0: not at all; 0..1: that much", &out.fovspeed},
      {"cl_camblend", d.camblend, 0, 4,
       "How long every camera change takes, as a share of its own length.",
       "0: a cut; 0..4: that share", &out.camblend},
      {"cl_motion", d.motion, 0, 1,
       "How much of the camera's motion you did not command remains: bob, roll, sway, tremor and "
       "shakes.",
       "0: none, for comfort; 1: all of it; 0..1: that share", &out.motion},
      {"cl_shake", d.shake, 0, 1, "How much of the camera's shakes remain.",
       "0: none; 1: all of them; 0..1: that share", &out.shake},
  };
  for (const Row& r : rows) {
    // The registry copies the fallback and the help (console/cvar.hpp's Cvar).
    const std::string fallback = text(r.fallback);
    c.bind({.name = r.name,
            .fallback = fallback,
            .flags = kPlayer,
            .help = r.help,
            .kind = con::CvarKind::number,
            .min = r.min,
            .max = r.max,
            .values = r.values},
           r.field);
  }
  if (!d.camera.empty()) {
    // One entry a rung, each its own name (console/words.hpp).
    std::string values;
    for (size_t at = 0; at < d.ladder_names.size();) {
      const size_t end = std::min(d.ladder_names.find(' ', at), d.ladder_names.size());
      if (end > at) {
        if (!values.empty()) values += "; ";
        values.append(d.ladder_names.substr(at, end - at)).append(": that rung");
      }
      at = end + 1;
    }
    out.camera_cvar = &c.bind({.name = "cl_camera",
                               .fallback = d.camera,
                               .flags = kPlayer,
                               .help = "The camera's rung on the game's ladder.",
                               .kind = con::CvarKind::text,
                               .values = values},
                              &out.camera);
    out.camera_seen = out.camera_cvar->revision;
  }
}

CameraSettings camera_settings(const CameraCvars& v) {
  CameraSettings s;
  s.motion = v.motion;
  s.shake = v.shake;
  s.blend = v.camblend;
  s.fov_speed = v.fovspeed;
  s.fov_h = v.fov * kDeg;
  return s;
}

bool apply_camera_cvars(CameraCvars& v, std::span<const Rig> ladder, Camera& cam, Edit edit,
                        double seconds) {
  cam.settings = camera_settings(v);
  const double u = v.unit_m;
  const int n =
      cam.count < static_cast<int>(ladder.size()) ? cam.count : static_cast<int>(ladder.size());
  for (int i = 0; i < n; ++i) {
    const Rig& base = ladder[static_cast<size_t>(i)];
    Rig& rig = cam.rigs[i];
    if (rig.kind == RigKind::first_person) {
      FirstPersonRow& r = rig.first_person;
      const FirstPersonRow& b = base.first_person;
      if (v.viewheight > 0) {
        r.eye_stand_m = v.viewheight * u;
        r.eye_crouch_m = b.eye_crouch_m * r.eye_stand_m / b.eye_stand_m;
      } else {
        r.eye_stand_m = b.eye_stand_m;
        r.eye_crouch_m = b.eye_crouch_m;
      }
      r.bob = v.bob;  // a length over a speed: the same in any unit
      r.bob_cycle_s = v.bobcycle;
      r.bob_up = v.bobup;
      r.roll_angle = v.rollangle * kDeg;
      r.roll_speed_mps = v.rollspeed * u;
      r.kick_time_s = v.kicktime;
      r.kick_pitch = v.kickpitch * kDeg;
      r.kick_roll = v.kickroll * kDeg;
      r.idle = v.idlescale * kDeg;
      r.dip_per_mps = b.dip_per_mps * v.landdip;
      r.dip_max_m = b.dip_max_m * v.landdip;
    } else if (rig.kind == RigKind::boom && rig.name == "chase") {
      rig.boom.dist_m = v.chasedist > 0 ? v.chasedist * u : base.boom.dist_m;
      rig.boom.height_m = v.chaseheight > 0 ? v.chaseheight * u : base.boom.height_m;
    }
  }
  if (!v.camera_cvar || v.camera_cvar->revision == v.camera_seen) return true;
  v.camera_seen = v.camera_cvar->revision;
  const int rung = find_rung(cam, v.camera);
  if (rung < 0) return false;
  if (rung != cam.rung) set_rung(cam, rung, edit, seconds);
  return true;
}

}  // namespace ge::camera
