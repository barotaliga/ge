// A PLAYER WITH NOTHING TO DO: what a person at the keys does while he waits on someone (a co-op
// partner reading a sign, a teammate deciding where to go), so a bot that waits looks like one
// and not like a statue. Each ACT lasts a few seconds and the next comes after a short pause:
// - SPIN: the view turned all the way round once or twice, standing (a mouse flicked to see
//   what is behind);
// - LOOK: the view turned onto something worth a look (an item in sight, a doorway) and held;
// - GLANCE: the view onto the one he waits for, with a nod (the pitch dipped and back);
// - HOP: a short bunny hop out and back round where he stands (the caller sends the pilot);
// - JUMP: a few jumps on the spot;
// - SWAY: stepping side to side under a steady view.
// Which act comes is drawn by weights off the player's traits (`idle_weights`): good movers hop
// and jump, a calm player looks and glances more, a restless one spins. The view moves at a
// person's turn rate, eased in and out (`idle_turn`), never in one frame.
//
// Deterministic: draws from the caller's stream only, no allocation.
//
// ge:summary A player with nothing to do: spins, looks, glances, hops, jumps and sways
// ge:keywords idle bored boredom spin look glance hop jump sway waiting humanlike fidget
// ge:keywords idle_pick idle_weights idle_turn idle-act companion
// ge:skill shooter bots
// ge:test tests/shooters_edge_companion_test.cpp
#pragma once

#include <cstdint>

#include "core/prng.hpp"
#include "shooter/traits.hpp"

namespace ge::shooter {

enum class IdleAct : uint8_t { none, spin, look, glance, hop, jump, sway };
inline constexpr int kIdleActs = 7;
const char* idle_act_name(IdleAct a);

// How likely each act is (any scale; `none` is never drawn).
struct IdleWeights {
  float w[kIdleActs] = {};
};
IdleWeights idle_weights(const Traits& t);

struct IdleState {
  IdleAct act = IdleAct::none;
  double until = 0;    // the act's end, s
  double next_at = 0;  // the next act's start (a pause between), s
  float turn = 0;      // spin: degrees still to turn (signed); sway: the side held (-1 or 1)
  double flip_at = 0;  // sway and jump: the next change of key, s
  int64_t acts[kIdleActs] = {};  // acts begun, by kind
};

// The act begun at `t`: drawn by `w` (with `look_ok` false a LOOK is never drawn: nothing worth a
// look), its length set and its own numbers (the spin's turn, the sway's side). Sets `s.act`.
IdleAct idle_begin(IdleState& s, const IdleWeights& w, core::Rng& rng, double t, bool look_ok);
// The act ended at `t` (its time up, or the wait over), the pause before the next drawn.
void idle_end(IdleState& s, core::Rng& rng, double t);

// The view's yaw (or pitch) `now` turned toward `want` at most `rate` degrees a second over `dt`,
// eased: the turn slows within 30 degrees of the goal. Degrees, unwrapped about `now`.
float idle_turn(float now, float want, float rate, double dt);

}  // namespace ge::shooter
