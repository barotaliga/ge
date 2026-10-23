// The camera's pose into a frame's `FrameView`: the one seam between the camera and the
// renderer. Header-only, so the camera library itself never links the renderer; only code that
// draws includes it.
//
// ge:summary The camera pose written into the renderer's FrameView
// ge:keywords camera frame-view renderer seam to_frame_view eye heading pitch roll fov
// ge:skill camera render
// ge:test tests/camera_pose_test.cpp
#pragma once

#include "camera/pose.hpp"
#include "render/view.hpp"

namespace ge::camera {

inline void to_frame_view(const Pose& p, render::FrameView& v) {
  v.eye[0] = p.eye.x;
  v.eye[1] = p.eye.y;
  v.eye[2] = p.eye.z;
  v.heading = p.yaw;
  v.pitch = p.pitch;
  v.roll = p.roll;
  v.fov_y = p.fov_y;
  v.near_m = p.near_m;
}

}  // namespace ge::camera
