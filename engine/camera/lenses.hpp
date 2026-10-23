// THE SIMPLE LENSES: the orbit (a menu's drone round the subject), the planted lens (a trackside
// camera, a TV gantry, an impact camera standing still in the world, panning to hold the subject
// and zooming to hold its size) and the free lens (the developer's flying camera: the subject IS
// the camera, driven by the game's free-fly input).
//
// ge:summary The orbit, planted (trackside, zooming) and free (developer) lenses
// ge:keywords camera orbit drone planted trackside tv broadcast gantry impact zoom free
// ge:keywords free-fly developer frame_orbit frame_planted frame_free
// ge:skill camera
// ge:test tests/camera_lenses_test.cpp
#pragma once

#include "camera/follow.hpp"
#include "camera/pose.hpp"
#include "camera/subject.hpp"
#include "camera/world.hpp"

namespace ge::camera {

struct OrbitRow {
  double radius_m = 16, height_m = 6;
  double spin = 0.14;  // rad/s round the subject
  double fov_y = 0.96;
  double aim_height_m = 0.6;
  double clearance_m = 2;
};
struct OrbitState {
  double angle = 0;
};
Pose frame_orbit(const OrbitRow& row, const Subject& s, const World& w, OrbitState& st, double dt);

struct PlantedRow {
  Vec3 eye{0, 0, 0};  // world metres: where it stands
  double aim_height_m = 1.0;
  Poles aim{1.4, 0.9, 1.0};
  double hold_m = 0;  // the height of the frame the subject's distance is held to (0: fixed fov)
  double fov_y = 0.8;
  double fov_min = 0.05, fov_max = 1.2;
  double zoom_halflife_s = 0.25;
};
struct PlantedState {
  bool fresh = true;
  Follower yaw{}, pitch{};
  double fov = 0;
};
Pose frame_planted(const PlantedRow& row, const Subject& s, PlantedState& st, double dt);

struct FreeRow {
  double fov_y = 1.08;
};
Pose frame_free(const FreeRow& row, const Subject& s);

}  // namespace ge::camera
