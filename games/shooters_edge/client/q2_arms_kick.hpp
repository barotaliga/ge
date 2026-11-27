// QUAKE II'S KICK ON ANOTHER GAME'S RULES, as the window draws it: rules that hold Quake II's
// weapons whole (classic remix's `cr_items q2` on Quake's rules) send the hand's frame and the view
// model and nothing of Quake II's view kick, so the window reads each shot off the frames
// (`ge::classic::q2_frame_fired`) and gives the view that weapon's kick
// (`ge::classic::q2_shot_kick`), eased in over the rules' tenth and out over the next
// (`ge::classic::Q2KickBlend`). The listen window and a joined server's both step one.
//
// - **The weapon** is the view model's (`profiles::held_frames`); any other model kicks nothing,
//   so Quake's own weapons keep Quake's kick.
// - **The machinegun's climb** counts its shots in a row and is none in deathmatch; its shake,
//   like the chaingun's, is drawn off the tenth and the player.
// - **What moves**: the kick's angles turn the view (and the gun, placed at the eye) unless
//   `cl_kickangles` is 0; its move shifts the eye always.
//
// ge:summary Quake II's view kick drawn for its weapons on other rules: shots read off the frames
// ge:keywords q2-kick view-kick kick-angles kick-origin recoil remix cr_items q2-arms
// ge:keywords arms_kick_step machinegun-climb weapon-frames
// ge:skill shooter camera
// ge:test tests/shooters_edge_remix_q2_arms_test.cpp
#pragma once

#include <cstdint>
#include <string_view>

#include "classic/q2_arms.hpp"
#include "core/num.hpp"
#include "shooters_edge/profiles/remix_arsenal.hpp"

namespace shooters_edge::client {

// WHOSE FRAMES A WHOLE VIEW MODEL OF QUAKE II'S IS SENT. Quake II's hand (its own rules, or the
// classic rules stepping it) sends the model's own frames: up from 0, firing, idling past frame 8,
// and frame 0 only for the tenth a raise starts. Rules that fire another game's weapon under Quake
// II's model (a recording made by Quake's own rules, a program that keeps Quake's frames) send
// Quake's: 0 at rest and 1 on from each shot. Read off the frames: an idle frame past 8 is Quake
// II's hand; frame 0 held a quarter of a second is Quake's. Quake's frames are drawn on Quake II's
// model as its rest and its fire run (`whole_frame`), never its raise, and kick nothing of Quake
// II's.
inline constexpr int kQuakeFramesMost = 8;      // Quake's view models' frames run 0..8
inline constexpr double kQuakeRestHeld = 0.25;  // s on frame 0 that only Quake's frames hold
struct WholeFrames {
  bool quake = false;  // the frames are Quake's
  int last = -1;       // the frame last read
  double since = 0;    // when it was first read, s
};
inline void read_whole_frames(WholeFrames& w, int frame, double t) {
  if (frame != w.last) {
    w.last = frame;
    w.since = t;
  }
  if (frame > kQuakeFramesMost) w.quake = false;
  if (frame == 0 && t - w.since >= kQuakeRestHeld) w.quake = true;
}
// The frame of a whole model resting on `rest` and firing `fire_first`..`fire_last` to draw for
// frame `frame` read as `w` says.
inline int whole_frame(const WholeFrames& w, int rest, int fire_first, int fire_last, int frame) {
  if (!w.quake) return frame;
  if (frame <= 0) return rest;
  const int f = fire_first + frame - 1;
  return f < fire_last ? f : fire_last;
}

struct ArmsKick {
  WholeFrames read;
  ge::classic::Q2KickBlend blend;
  std::string_view model;  // the view model last looked at
  int frame = -1;          // its frame then; -1 none yet
  int in_row = 0;          // the machinegun's shots in a row
};

// One look at the hand at time `t` (s): view model `model` on `frame`, the view along `forward`
// (a unit vector in the map's axes), `deathmatch` the rules' (no climb), `who` the player's number.
// The kick to draw now.
inline ge::classic::Q2Kick arms_kick_step(ArmsKick& k, std::string_view model, int frame, double t,
                                          const float forward[3], bool deathmatch, uint32_t who) {
  namespace gc = ge::classic;
  const profiles::HeldFrames* held = profiles::held_frames(model);
  if (held == nullptr || model != k.model) {  // another weapon: no shot read across the change
    k.model = model;
    k.frame = held != nullptr ? frame : -1;
    k.in_row = 0;
    gc::q2_kick_advance(k.blend, t);
    return gc::q2_kick_at(k.blend, t);
  }
  const gc::Q2Arm& arm = gc::q2_arm(held->arm);
  read_whole_frames(k.read, frame, t);
  if (k.frame >= 0 && frame != k.frame && !k.read.quake) {
    if (gc::q2_frame_fired(arm.frames, arm.held, k.frame, frame)) {
      const float climb =
          held->arm == gc::Q2ArmId::machinegun ? gc::q2_machine_climb(k.in_row, deathmatch) : 0.0f;
      k.in_row++;
      const auto tenth =
          static_cast<uint32_t>(static_cast<int64_t>(ge::core::num::round(t * 10.0)));
      gc::q2_kick_shot(k.blend, t,
                       gc::q2_shot_kick(held->arm, forward, climb, gc::q2_pause_draw(tenth, who)));
    } else {
      k.in_row = 0;
    }
  }
  k.frame = frame;
  gc::q2_kick_advance(k.blend, t);
  return gc::q2_kick_at(k.blend, t);
}

}  // namespace shooters_edge::client
