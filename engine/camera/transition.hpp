// TRANSITIONS: a change of view is an EDIT, and the three edits say different things.
//
//   CUT          the next frame is the new view (a broadcast's cut to a corner camera, a respawn,
//                a replay's angle). The arriving rig starts fresh. Wrong between two views of the
//                same subject a few metres apart: all it tells a player is that the picture was
//                replaced.
//   BLEND        for `duration` both rigs are framed and the pose is blended from the old to the
//                new on a smootherstep. The blend is held in the SUBJECT's frame (`Local`: how far
//                to its right, up and ahead, and where it looks against its heading), rebuilt
//                round the subject every frame, so at speed a lens is not left standing in a
//                field while the subject drives out of the frame; it lands ON the new rig's pose;
//                and a floor under the path (faded in and out) keeps it out of the ground.
//   INERTIALIZE  only the new rig is framed. At the switch the offset between the old pose and
//                the new, and its velocity, are recorded and decay to nothing on a quintic over
//                `duration` that starts with the offset's own velocity and ends at rest
//                (`Decay`): continuous in position and velocity, half a blend's cost, and a second
//                change mid-way re-bases from wherever the picture is. The default between rungs.
//
// ge:summary Camera transitions: cut, blend in the subject's frame, inertialize (quintic decay)
// ge:keywords camera transition edit cut blend crossfade inertialize inertialization quintic
// ge:keywords decay handover hand-over view-change subject-frame local to_local from_local
// ge:keywords smootherstep floor make_decay decay_at
// ge:skill camera
// ge:test tests/camera_transition_test.cpp
#pragma once

#include <cstdint>

#include "camera/pose.hpp"
#include "camera/subject.hpp"

namespace ge::camera {

enum class Edit : uint8_t { cut, blend, inertialize };

// A pose in the subject's frame: the eye's offset to its right, up and ahead (its heading's
// axes, level), the yaw against its heading, and the rest as they are.
struct Local {
  Vec3 off{0, 0, 0};
  double yaw = 0, pitch = 0, roll = 0, fov = 1, near = 0.1;
};
Local to_local(const Pose& p, const Subject& s);
Pose from_local(const Local& l, const Subject& s);
// The per-second rate from `a` to `b` over `dt` (angles the short way).
Local local_rate(const Local& a, const Local& b, double dt);

// 0..1 eased with zero velocity and acceleration at both ends.
double smootherstep(double t);

// One offset decaying to nothing on a quintic: x0 and v0 at the switch (a velocity either way,
// kept), gone at `t_end` (sooner when the velocity closes the offset fast).
struct Decay {
  double sign = 1, x0 = 0, v0 = 0, a0 = 0, a = 0, b = 0, c = 0, t_end = 0;
};
Decay make_decay(double x0, double v0, double duration_s);
double decay_at(const Decay& d, double t);

// The inertialized offset of a whole local pose: the eye's on each axis of the subject's frame,
// the angles and the fov, each on its own and each keeping its velocity.
struct Inertial {
  Decay x{}, y{}, z{}, yaw{}, pitch{}, roll{}, fov{};
};
// From the old pose (and its rate) onto the new one (and its rate).
Inertial make_inertial(const Local& from, const Local& from_rate, const Local& to,
                       const Local& to_rate, double duration_s);
// The new rig's local pose with what is left of the offset at time `t` added.
Local apply_inertial(const Inertial& in, const Local& to, double t);

}  // namespace ge::camera
