// THE SLIDE AND THE STEP: how the classic move carries the player's box along its velocity for
// one command and what it does where the box meets a plane.
//
// SLIDE (`slide_move`): at most 4 bumps. Each traces from the origin to origin + velocity x the
// time left; a start in solid zeroes the velocity and ends it (floor and step); any distance
// travelled moves the origin and forgets the planes collected; a full run ends it. Each plane hit
// is a floor (normal z >= 0.7), a step (z exactly 0) or other, its solid joins the touch list,
// and with 5 planes collected the velocity is zeroed. The new velocity is the slide's starting
// velocity clipped against the first collected plane whose result does not point into another;
// with none and exactly two planes, it runs along their crease; otherwise it stops. A new
// velocity against the starting one stops it (no jitter in corners). A water jump keeps its
// starting velocity through the slide.
//
// STEP-SLIDE (`step_slide_move`): slide; if a plane was hit, also try the move raised by the
// step height (18 on the ground; in the air, with pm_airstep, only after a wall, and only as
// high as keeps the climb under 18 above the ground found below), slide there and come back
// down; keep whichever went further across, the raised one taking the plain one's vertical
// speed. An air step costs 1% of the horizontal speed per unit climbed.
//
// `clip_velocity`: v - n (v . n) f, each component strictly within 0.1 of zero set to zero.
//
// ge:summary The classic move's slide along planes and its step up stairs, and the velocity clip
// ge:keywords slide slide-move step-slide stairs step-height clip clip_velocity crease bump
// ge:keywords blocked floor wall airstep slide_move step_slide_move
// ge:skill shooter
// ge:test tests/shooter_slide_test.cpp tests/shooter_pmove_test.cpp
#pragma once

#include "shooter/pmove.hpp"

namespace ge::shooter {

// The move in progress, shared by the steps of pmove.cpp and the slides here.
struct Mover {
  MoveState& s;
  const MoveSettings& set;
  const MoveWorld& world;
  TouchList& touches;
  float dt = 0;
  // The last floor a slide hit (pm_pground's landing reads it): its solid and its plane.
  int floor_ent = -1;
  Vec3f floor_normal{};
};

Vec3f clip_velocity(Vec3f v, Vec3f normal, float overbounce);

// Returns the kBlocked* bits of the planes hit.
int slide_move(Mover& m);
// Returns the first slide's kBlocked* bits.
int step_slide_move(Mover& m, bool in_air);

}  // namespace ge::shooter
