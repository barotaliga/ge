// WHAT A FRAME LOOKS AT, AND FROM WHERE: the `FrameView` a game extracts from its state once a
// frame (plain data; the renderer never reads the simulation any other way), and the camera's
// matrices built from it.
//
// CONVENTIONS (the world's, stated in engine/core/math.hpp): y is up; a heading is 0 along +z
// and turns clockwise seen from above, toward +x; a pitch is positive looking up. So the world
// is left-handed: the camera's right is up x forward.
//
// THE CAMERA IS AT THE ORIGIN of every matrix here: the GPU works in camera-relative
// coordinates (world minus the eye, in float), so a point 3 km away keeps its millimetres.
// View space looks down -z with x right and y up; the projection is REVERSED-Z with an
// INFINITE far plane (depth 1 at the near plane, 0 at infinity), the depth buffer is cleared to
// 0 and tested `greater`, which spreads float depth's precision evenly over kilometres.
// Matrices are column-major float, as WGSL reads a mat4x4f from a buffer.
//
// The sun is a world direction (toward it), with its elevation and azimuth (a world heading),
// as engine/core/solar.hpp gives them.
//
// ge:summary The FrameView a game extracts each frame, and its camera matrices and frustum
// ge:keywords render frame-view camera view projection reverse-z infinite-far camera-relative
// ge:keywords frustum planes heading pitch fov mat4 inverse sun-direction camera_matrices
// ge:keywords look_dir sun_direction
// ge:skill render
// ge:test tests/render_view_test.cpp
#pragma once

#include <cstdint>

namespace ge::render {

// A column-major 4x4 matrix: element (row r, column c) is m[c * 4 + r].
struct Mat4 {
  float m[16];
};

Mat4 mul(const Mat4& a, const Mat4& b);
// The general inverse; the identity's when `a` is singular.
Mat4 inverse(const Mat4& a);
// a * (x, y, z, w)
void transform(const Mat4& a, const float in[4], float out[4]);

struct FrameView {
  // The eye, world metres, and where it looks.
  double eye[3] = {0, 0, 0};
  double heading = 0;   // rad, the world's convention
  double pitch = 0;     // rad, positive up
  double fov_y = 1.0;   // vertical field of view, rad
  double near_m = 0.1;  // the near plane, m
  // The sun: elevation over the horizon (rad) and azimuth (a world heading).
  double sun_elevation = 0.5;
  double sun_azimuth = 3.14159265358979;
  double sun_illuminance = 1.0;  // relative; the sky and every light scale with it
  // The world's y = 0 over the sea, m: where the atmosphere starts.
  double altitude_m = 0;
  double time_s = 0;   // the frame's time (the renderer reads no clock)
  uint64_t frame = 0;  // the frame's number (TAA's jitter is indexed by it)
};

// The unit direction a heading and pitch look along.
void look_dir(double heading, double pitch, double out[3]);
// The unit direction toward the sun.
void sun_direction(const FrameView& view, double out[3]);

struct CameraMatrices {
  Mat4 view;           // camera-relative world -> view (a rotation)
  Mat4 proj;           // view -> clip, reversed-z, infinite far
  Mat4 view_proj;      // camera-relative world -> clip
  Mat4 inv_view_proj;  // clip -> camera-relative world
  // The frustum's left, right, bottom, top and near planes in camera-relative world space:
  // (a, b, c, d) with a*x + b*y + c*z + d >= 0 inside, (a, b, c) of unit length.
  float planes[5][4];
};

CameraMatrices camera_matrices(const FrameView& view, double aspect);

}  // namespace ge::render
