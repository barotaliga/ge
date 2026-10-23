// The camera stack's frame: every rig framed warm, the transition, the moments, the shake.
//
// ge:summary The camera stack's frame: warm rigs, the edit in flight, moments and the shake
// ge:keywords camera stack update warm transition blend inertialize shake moments
// ge:skill camera
// ge:test tests/camera_camera_test.cpp
#include "camera/camera.hpp"

#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::camera {

namespace libm = core::libm;

namespace {

Pose frame_rig(Camera& cam, int i, const Subject& s, const World& w, double dt) {
  const Rig& r = cam.rigs[i];
  RigState& st = cam.states[i];
  const CameraSettings& set = cam.settings;
  switch (r.kind) {
    case RigKind::first_person: {
      FirstPersonRow row = r.first_person;
      if (set.fov_h > 0) row.fov_y = fov_y_of(set.fov_h);
      return frame_first_person(row, s, st.first_person, dt, set.motion);
    }
    case RigKind::boom:
      return frame_boom(r.boom, s, w, st.boom, dt, set.motion, set.fov_speed);
    case RigKind::mounted:
      return frame_mounted(r.mounted, s, st.mounted, dt, set.motion);
    case RigKind::orbit:
      return frame_orbit(r.orbit, s, w, st.orbit, dt);
    case RigKind::planted:
      return frame_planted(r.planted, s, st.planted, dt);
    case RigKind::free:
      return frame_free(r.free, s);
  }
  return Pose{};
}

void freshen(RigState& st) {
  st.first_person.fresh = true;
  st.boom.fresh = true;
  st.mounted.fresh = true;
  st.planted.fresh = true;
}

bool worn(RigKind k) { return k == RigKind::first_person || k == RigKind::mounted; }

}  // namespace

Rig first_person_rig(std::string_view name, const FirstPersonRow& row) {
  Rig r;
  r.name = name;
  r.kind = RigKind::first_person;
  r.first_person = row;
  return r;
}
Rig boom_rig(std::string_view name, const BoomRow& row) {
  Rig r;
  r.name = name;
  r.kind = RigKind::boom;
  r.boom = row;
  return r;
}
Rig mounted_rig(std::string_view name, const MountedRow& row) {
  Rig r;
  r.name = name;
  r.kind = RigKind::mounted;
  r.mounted = row;
  return r;
}
Rig orbit_rig(std::string_view name, const OrbitRow& row) {
  Rig r;
  r.name = name;
  r.kind = RigKind::orbit;
  r.orbit = row;
  return r;
}
Rig planted_rig(std::string_view name, const PlantedRow& row) {
  Rig r;
  r.name = name;
  r.kind = RigKind::planted;
  r.planted = row;
  return r;
}
Rig free_rig(std::string_view name, const FreeRow& row) {
  Rig r;
  r.name = name;
  r.kind = RigKind::free;
  r.free = row;
  return r;
}

Camera make_camera(const Rig* rigs, int count, uint32_t seed) {
  Camera cam;
  cam.count = count < kMaxRigs ? count : kMaxRigs;
  for (int i = 0; i < cam.count; ++i) cam.rigs[i] = rigs[i];
  cam.shake = make_shake(seed);
  return cam;
}

int find_rung(const Camera& cam, std::string_view name) {
  for (int i = 0; i < cam.count; ++i) {
    if (cam.rigs[i].name == name) return i;
  }
  return -1;
}

void set_rung(Camera& cam, int rung, Edit edit, double duration_s) {
  if (rung < 0 || rung >= cam.count || rung == cam.rung) return;
  const double d = duration_s * cam.settings.blend;
  if (edit == Edit::inertialize && (!cam.framed[rung] || !cam.has_out)) edit = Edit::blend;
  if (edit == Edit::blend && !cam.has_out) edit = Edit::cut;
  cam.from = -1;
  cam.inertial_on = false;
  cam.t = 0;
  cam.duration = d;
  cam.edit = d > 0 ? edit : Edit::cut;
  if (cam.edit == Edit::cut) {
    freshen(cam.states[rung]);
  } else if (cam.edit == Edit::blend) {
    cam.from = cam.rung;
  } else {
    cam.inertial = make_inertial(cam.out, cam.out_rate, cam.last[rung], cam.last_rate[rung], d);
    cam.inertial_on = true;
  }
  cam.rung = rung;
}

void snap(Camera& cam) {
  for (int i = 0; i < cam.count; ++i) freshen(cam.states[i]);
  cam.from = -1;
  cam.inertial_on = false;
  cam.has_out = false;
}

void add_trauma(Camera& cam, double amount) { add_trauma(cam.shake, amount); }

bool in_transition(const Camera& cam) {
  return (cam.from >= 0 || cam.inertial_on) && cam.t < cam.duration;
}

Pose update(Camera& cam, const Subject& s, const World& w, double dt) {
  if (cam.count == 0) return Pose{};
  for (int i = 0; i < s.event_count; ++i) {
    const Event& e = s.events[i];
    if (e.kind == Moment::teleport) snap(cam);
    if (e.kind == Moment::explosion) add_trauma(cam.shake, e.size);
    if (e.kind == Moment::landing && !worn(cam.rigs[cam.rung].kind)) {
      add_trauma(cam.shake,
                 cam.land_trauma * core::smoothstep(cam.land_from_mps, cam.land_full_mps, e.size));
    }
  }

  // Every rig, warm; each one's frame kept in the subject's frame.
  Local now[kMaxRigs];
  for (int i = 0; i < cam.count; ++i) {
    now[i] = to_local(frame_rig(cam, i, s, w, dt), s);
    cam.last_rate[i] =
        cam.framed[i] ? local_rate(cam.last[i], now[i], dt) : Local{{0, 0, 0}, 0, 0, 0, 0, 0};
    cam.last[i] = now[i];
    cam.framed[i] = true;
  }

  // The transition.
  Local out = now[cam.rung];
  if (cam.from >= 0 || cam.inertial_on) {
    cam.t += dt;
    if (cam.t >= cam.duration) {
      cam.from = -1;
      cam.inertial_on = false;
    } else if (cam.inertial_on) {
      out = apply_inertial(cam.inertial, now[cam.rung], cam.t);
    } else {
      const double k = smootherstep(cam.t / cam.duration);
      const Pose a = from_local(now[cam.from], s), b = from_local(now[cam.rung], s);
      Pose p = blend_pose(a, b, k);
      // A floor under the path, faded in and out, so a flight between two clear ends does not
      // pass through a brow between them.
      const double floor = ground_at(w, p.eye.x, p.eye.z) + cam.floor_m * 4.0 * k * (1.0 - k);
      if (p.eye.y < floor) p.eye.y = floor;
      out = to_local(p, s);
    }
  }
  cam.out_rate = cam.has_out ? local_rate(cam.out, out, dt) : Local{{0, 0, 0}, 0, 0, 0, 0, 0};
  cam.out = out;
  cam.has_out = true;

  // The shake, on top of everything, in the lens's own axes.
  Pose p = from_local(out, s);
  const ShakeOut sh =
      shake_at(cam.shake_row, cam.shake, dt, cam.settings.shake * cam.settings.motion);
  const double cy = libm::cos(p.yaw), sy = libm::sin(p.yaw);
  const Vec3 f = look_forward(p.yaw, p.pitch);
  p.eye.x += cy * sh.right + f.x * sh.forward;
  p.eye.y += sh.up + f.y * sh.forward;
  p.eye.z += -sy * sh.right + f.z * sh.forward;
  p.yaw += sh.yaw;
  p.pitch = core::clamp(p.pitch + sh.pitch, -1.55, 1.55);
  p.roll += sh.roll;
  return p;
}

}  // namespace ge::camera
