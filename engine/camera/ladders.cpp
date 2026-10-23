// The reference ladder's rows.
//
// ge:summary The reference ladder's rows
// ge:keywords camera ladder reference rows
// ge:skill camera
// ge:test tests/camera_camera_test.cpp
#include "camera/ladders.hpp"

namespace ge::camera {

namespace {

Rig make_far() {
  BoomRow r;
  r.dist_m = 10;
  r.dist_per_mps = 0.02;
  r.height_m = 3.8;
  r.fov_y = 0.98;
  r.fov_per_mps = 0.0087;
  r.fov_max = 1.33;
  r.surge_m_per_mps2 = 0.22;
  r.tremor_rad = 0.0007;
  r.yaw = Poles{0.8, 0.9, 0};
  r.slip = 0.4;
  r.lift = Poles{1.2, 0.75, 2};
  r.lift_air = Poles{0.7, 0.85, 2};
  r.lag_max_m = 3.2;
  r.incline = 0.4;
  r.incline_steep = 0.6;
  r.ahead_s = 0.8;
  r.lean = Poles{0.55, 1, 0};
  r.place = 0.22;
  r.look = Poles{1.2, 0.9, 0};
  r.clearance_m = 1.4;
  r.bank = 0.07;
  return boom_rig("far", r);
}

Rig make_high() {
  BoomRow r;
  r.dist_m = 13;
  r.dist_per_mps = 0.05;
  r.height_m = 10;
  r.fov_y = 0.98;
  r.fov_per_mps = 0.0035;
  r.fov_max = 1.15;
  r.hold = 0;
  r.tremor_rad = 0.0004;
  r.yaw = Poles{0.65, 0.9, 0};
  r.slip = 0.5;
  r.lift = Poles{0.8, 0.8, 2};
  r.lift_air = Poles{0.5, 0.9, 2};
  r.lag_max_m = 4.5;
  r.incline = 0.35;
  r.incline_steep = 0.35;
  r.ahead_s = 1;
  r.lean = Poles{0.45, 1, 0};
  r.place = 0.12;
  r.look = Poles{1, 0.9, 0};
  r.clearance_m = 3;
  r.bank = 0;
  return boom_rig("high", r);
}

Rig make_tips() {
  MountedRow r;
  r.eye_body = Vec3{0, 0.35, 0.55};
  r.down = 0.12;
  r.fov_y = 1.29;
  r.fov_per_mps = 0.007;
  r.roll_share = 0.85;
  r.grain_m = 0.004;
  return mounted_rig("tips", r);
}

Rig make_trackside() {
  PlantedRow r;
  r.eye = Vec3{12, 3, 30};
  r.hold_m = 4;
  return planted_rig("trackside", r);
}

const Rig kLadder[] = {
    first_person_rig("eyes", FirstPersonRow{}),
    boom_rig("chase", BoomRow{}),
    make_far(),
    make_high(),
    mounted_rig("helmet", MountedRow{}),
    make_tips(),
    orbit_rig("orbit", OrbitRow{}),
    make_trackside(),
};

}  // namespace

std::span<const Rig> reference_ladder() { return kLadder; }

}  // namespace ge::camera
