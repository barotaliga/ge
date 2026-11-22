// QUAKE II'S EFFECT SWITCHES: the player's own say over which of the game's effects are drawn and
// how its lights behave, as its own cvars set them while it is the game loaded (the window's
// cvar layers register them; another game keeps the defaults below, which change nothing).
//
// - `cl_disable_particles`, a bit sum of the specks left out: 1 a grenade's explosion, 2 a
//   grenade's trail, 4 a rocket's explosion, 8 a rocket's trail, 16 blood, 32 a blaster bolt's
//   trail.
// - `cl_disable_explosions`, a bit sum of the explosions' balls of fire left out (their light and
//   specks stay): 1 a grenade's, 2 a rocket's. `kQ2Grenade` and `kQ2Rocket` are those bits, and
//   name an explosion's kind where the effects keep it.
// - `cl_dlight_hacks`, a bit sum: 2 every explosion's light half as far, 4 no light from a machine
//   gun's or a chain gun's flash (1, a rocket's light in another colour, is not taken).
// - `cl_muzzlelight_time`: how long a gun's flash lights the room, milliseconds (16 as drawn; the
//   light's whole fade scales with it, 0 none).
// - `cl_muzzleflashes` 0: no fire drawn at another player's or a monster's gun (the light stays).
// - `cl_footsteps` 0: no footstep heard (1 and 2 the same: the game's own four steps).
// - `s_ambient`, the loops an entity carries that are heard: 0 none, 1 all, 2 only those of an
//   entity with a model (a speaker on its own is silent), 3 only the listener's own.
//
// ge:summary Quake II's effect switches: specks, explosions, lights and muzzle flashes left out
// ge:keywords q2 cl_disable_particles cl_disable_explosions cl_dlight_hacks cl_muzzlelight_time
// ge:keywords cl_muzzleflashes cl_footsteps s_ambient effect-switches q2_fx_switches
// ge:skill classic shooter
// ge:test tests/shooters_edge_cvar_layers_test.cpp
#pragma once

#include <cstdint>

namespace ge::classic {

inline constexpr uint8_t kQ2Grenade = 1, kQ2Rocket = 2;  // an explosion's kind (0: any other)

struct Q2FxSwitches {
  int no_particles = 0;         // cl_disable_particles
  int no_explosions = 0;        // cl_disable_explosions
  int light_hacks = 0;          // cl_dlight_hacks
  double muzzle_light_ms = 16;  // cl_muzzlelight_time
  bool muzzle_models = true;    // cl_muzzleflashes
  int footsteps = 1;            // cl_footsteps
  int ambient = 1;              // s_ambient
};

// The specks of an explosion of `kind` are left out.
inline bool q2_no_blast_specks(const Q2FxSwitches& s, uint8_t kind) {
  return (kind == kQ2Grenade && (s.no_particles & 1) != 0) ||
         (kind == kQ2Rocket && (s.no_particles & 4) != 0);
}
// Its ball of fire is left out.
inline bool q2_no_blast_ball(const Q2FxSwitches& s, uint8_t kind) {
  return (s.no_explosions & kind) != 0;
}
// A gun's flash light of `age` seconds as one of `life` seconds drawn would be at the age the
// switch makes it (past `life`: gone).
inline double q2_muzzle_age(const Q2FxSwitches& s, double age) {
  return s.muzzle_light_ms > 0 ? age * 16 / s.muzzle_light_ms : 1e9;
}

}  // namespace ge::classic
