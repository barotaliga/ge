// QUAKE II'S MODEL EFFECTS: the models a temporary entity leaves for a moment where it happened,
// as the classic client draws them (q2_temp_fx.hpp has their specks and sounds): an explosion's
// ball, a blaster bolt's flare turned to the face it hit, a bullet's puff of smoke and its flash,
// the BFG ball's burst. Pure: the window's effects (world_fx.cpp) keep a pool of them and place
// each one as a model a frame.
//
// - THE CLOCK: each one's model frame moves on ten times a second, drawn between that frame and
//   the next (`frame_from`, `frame`, `f`), from the frame its kind starts at (`base`).
// - THREE CLASSES by how one fades and ends (`Q2FxClass`):
//   - a PUFF of N frames: its share shown falls from 1 to 0 over (N - 1) tenths, then it is gone;
//   - a FLASH: whole for a tenth, then gone;
//   - an EXPLOSION of N frames: gone after (N - 1) tenths; with `cl_smooth_explosions` 1 its share
//     falls evenly to 0 by then, see-through all along; with 0 it is (16 - t) / 16 at t whole
//     tenths, solid until the tenth tenth. The explosion's ball wears skin t / 2 for its first ten
//     tenths, 5 for the next three, 6 from then on.
//   A light's reach is its base times the share shown, its colour steady.
// - THE KINDS (`q2_model_fx_of`, by the protocol's kind byte): a gunshot's, a shotgun pellet's and
//   a bullet's sparks' hit leave the smoke (a puff of 4, lit by the room) and the flash (full
//   bright); a blaster bolt's hit the flare (a puff of 4, full bright, see-through, turned to the
//   face) and a yellow light of 150; explosion1 and a rocket's (in the air or in water) the ball's
//   frames 0..14 or 15..29 at even odds, explosion2 and a grenade's frames 30..48, each turned to a
//   random yaw, full bright, a light of 350 (1, 0.5, 0.5); the BFG ball's burst its sprite's 4
//   frames and a green light of 350.
// - THE PLAYER'S SAY: `cl_disable_explosions` leaves a rocket's or a grenade's ball out but keeps
//   its light; `cl_dlight_hacks` 2 starts those lights at 200.
// - CABLES: a parasite's drain, a medic's line and the grapple are a segment model laid as a
//   fitted chain (world_fx.hpp's `place_chain`, the one Quake's bolts are laid by loose), each
//   link at a new roll every frame; one lives 0.2 s unless sent again.
//
// Facts: porting/shooters-edge/q2-client-te.md.
//
// ge:summary Quake II's model effects: explosions' balls, puffs, flashes, flares and cables by age
// ge:keywords q2 temp-entity model-effect explosion-model r_explode smoke-puff flash-model
// ge:keywords blaster-flare bfg-explosion s_bfg2 cable segment parasite medic grapple
// ge:keywords q2_model_fx q2_model_fx_of q2_model_fx_pose explosion-skins
// ge:keywords cl_smooth_explosions cl_disable_explosions cl_dlight_hacks
// ge:skill shooter render
// ge:test tests/shooters_edge_q2_model_fx_test.cpp
#pragma once

#include <cmath>
#include <cstdint>
#include <string_view>

#include "classic/q2_fx_switches.hpp"

namespace shooters_edge::client {

// The models these effects draw: models/objects/r_explode, models/objects/explode (the flare),
// models/objects/smoke, models/objects/flash, the parasite's and the grapple's segments and
// sprites/s_bfg2.sp2 (world_models.hpp's kEffectModels lists them in this order from `q2_fx`).
enum class Q2FxModel : uint8_t { explosion, flare, smoke, flash, segment, ctf_segment, bfg };

enum class Q2FxClass : uint8_t { puff, flash, explosion };

struct Q2ModelFx {
  Q2FxModel model = Q2FxModel::explosion;
  Q2FxClass cls = Q2FxClass::puff;
  int frames = 1;  // its frame count (N)
  int base = 0;    // the frame it starts at
  double born_s = 0;
  float at[3] = {};      // map units
  float angles[3] = {};  // pitch, yaw, roll, degrees
  bool full_bright = false;
  bool see_through = false;  // drawn see-through from the start
  bool drawn = true;         // false: its light alone (cl_disable_explosions)
  float light = 0;           // its light's base reach, units (0: none)
  float rgb[3] = {};
};

// One effect at `now_s`: whether it still shows, the frames drawn between (`frame_from` toward
// `frame` by `f`), its skin, the share shown, see-through or not, and its light's reach.
struct Q2FxPose {
  bool alive = false;
  int frame_from = 0, frame = 0;
  float f = 0;
  int skin = 0;
  float alpha = 1;
  bool see_through = false;
  float light = 0;
};

inline Q2FxPose q2_model_fx_pose(const Q2ModelFx& m, double now_s, bool smooth) {
  Q2FxPose p;
  const double age = now_s - m.born_s;
  const double life = m.cls == Q2FxClass::flash ? 0.1 : (m.frames - 1) * 0.1;
  if (age < 0 || age >= life) return p;
  p.alive = true;
  const double tenths = age * 10;
  const int t = static_cast<int>(tenths);
  p.frame_from = m.base + t;
  p.frame = m.base + t + 1;
  p.f = static_cast<float>(tenths - t);
  if (m.model == Q2FxModel::bfg) {  // a sprite: its frames one after another, none between
    p.frame_from = p.frame = m.base + t;
    p.f = 1;
  }
  p.see_through = m.see_through;
  switch (m.cls) {
    case Q2FxClass::puff:
      p.alpha = static_cast<float>(1 - age / life);
      break;
    case Q2FxClass::flash:
      p.alpha = 1;
      break;
    case Q2FxClass::explosion:
      if (smooth) {
        p.alpha = static_cast<float>(1 - age / life);
        p.see_through = true;
      } else {
        p.alpha = t >= 16 ? 0.0f : static_cast<float>(16 - t) / 16;
        p.see_through = p.see_through || t >= 10;
      }
      if (m.model == Q2FxModel::explosion) {
        p.skin = t < 10 ? t / 2 : t < 13 ? 5 : 6;
        p.see_through = p.see_through || t >= 10;
      }
      break;
  }
  p.light = m.light * p.alpha;
  return p;
}

// The model effects of temporary entity `code` at `at` (map units) off a face along `way` (a unit
// normal; the blaster's flare is turned to it), born at `t`, into `out` (up to two); `pick` any
// number (the ball's start and its yaw). Returns how many.
inline int q2_model_fx_of(int code, uint32_t pick, const float at[3], const float way[3], double t,
                          const ge::classic::Q2FxSwitches& q, Q2ModelFx out[2]) {
  const auto at_point = [&](Q2ModelFx& m) {
    for (int k = 0; k < 3; ++k) m.at[k] = at[k];
    m.born_s = t;
  };
  switch (code) {
    case 0:     // a gunshot's hit
    case 4:     // a shotgun pellet's
    case 14: {  // a bullet's sparks
      Q2ModelFx& smoke = out[0];
      smoke = {};
      at_point(smoke);
      smoke.model = Q2FxModel::smoke;
      smoke.cls = Q2FxClass::puff;
      smoke.frames = 4;
      smoke.see_through = true;
      Q2ModelFx& flash = out[1];
      flash = {};
      at_point(flash);
      flash.model = Q2FxModel::flash;
      flash.cls = Q2FxClass::flash;
      flash.frames = 2;
      flash.full_bright = true;
      return 2;
    }
    case 2: {  // a blaster bolt's hit: the flare turned to the face
      Q2ModelFx& m = out[0];
      m = {};
      at_point(m);
      m.model = Q2FxModel::flare;
      m.cls = Q2FxClass::puff;
      m.frames = 4;
      m.full_bright = m.see_through = true;
      constexpr double kDeg = 180 / 3.141592653589793;
      const double z = way[2] < -1 ? -1 : way[2] > 1 ? 1 : way[2];
      m.angles[0] = static_cast<float>(std::acos(z) * kDeg);
      if (way[0] != 0 || way[1] != 0) {
        double yaw = std::atan2(way[1], way[0]) * kDeg;
        if (yaw < 0) yaw += 360;
        m.angles[1] = static_cast<float>(yaw);
      }
      m.light = 150;
      m.rgb[0] = m.rgb[1] = 1;
      return 1;
    }
    case 5:     // explosion1
    case 7:     // a rocket's
    case 17:    // a rocket's in water
    case 28:    // The Reckoning's plasma burst, drawn as a rocket's
    case 6:     // explosion2
    case 8:     // a grenade's
    case 18: {  // a grenade's in water
      Q2ModelFx& m = out[0];
      m = {};
      at_point(m);
      const bool grenade = code == 6 || code == 8 || code == 18;
      m.model = Q2FxModel::explosion;
      m.cls = Q2FxClass::explosion;
      m.frames = grenade ? 19 : 15;
      m.base = grenade ? 30 : (pick & 1) != 0 ? 15 : 0;
      m.angles[1] = static_cast<float>((pick >> 1) % 360);
      m.full_bright = true;
      // The player's say reaches the rocket's and the grenade's, never explosion1's or 2's.
      const bool own = code != 5 && code != 6;
      const uint8_t kind = grenade ? ge::classic::kQ2Grenade : ge::classic::kQ2Rocket;
      m.drawn = !own || (q.no_explosions & kind) == 0;
      m.light = own && (q.light_hacks & 2) != 0 ? 200 : 350;
      m.rgb[0] = 1;
      m.rgb[1] = m.rgb[2] = 0.5f;
      return 1;
    }
    case 20: {  // the BFG ball's burst: its sprite and a green light
      Q2ModelFx& m = out[0];
      m = {};
      at_point(m);
      m.model = Q2FxModel::bfg;
      m.cls = Q2FxClass::explosion;
      m.frames = 4;
      m.full_bright = m.see_through = true;
      m.light = 350;
      m.rgb[1] = 1;
      return 1;
    }
    default:
      return 0;
  }
}

// The kind byte of our rules' blast `name` (0: none of these): explosion1 5, explosion2 6, a
// rocket's (and our rules' plain blast) 7, The Reckoning's plasma burst 28, a grenade's 8, the BFG
// ball's burst 20 and its big one 21.
constexpr int q2_blast_code(std::string_view name) {
  return name == "te_explosion1"                                   ? 5
         : name == "te_explosion2"                                 ? 6
         : name == "te_rocket_explosion" || name == "te_explosion" ? 7
         : name == "te_plasma_explosion"                           ? 28
         : name == "te_grenade_explosion"                          ? 8
         : name == "te_bfg_explosion"                              ? 20
         : name == "te_bfg_bigexplosion"                           ? 21
                                                                   : 0;
}
// Which of cl_disable_explosions' and cl_disable_particles' kinds blast `code` is: a rocket's (in
// the air or in water), a grenade's, or neither (0).
constexpr uint8_t q2_blast_kind(int code) {
  return code == 7 || code == 17 || code == 28 ? ge::classic::kQ2Rocket
         : code == 8 || code == 18             ? ge::classic::kQ2Grenade
                                               : 0;
}

// A cable lives this long unless sent again.
inline constexpr double kQ2CableS = 0.2;
// The BFG's laser lives this long; its colour one of palette 0xd0..0xd3.
inline constexpr double kQ2BfgLaserS = 0.1;

}  // namespace shooters_edge::client
