// Quake II's held weapons' detail sounds (q2_fight.hpp's `fight_hand_step`): the hand's step and
// the one-shot sounds its frames make: the hand grenade's pin, the chaingun's spin up and down,
// the hyperblaster's wind-down (porting/shooters-edge/q2-monsters-round4.md § 13). The loops a
// weapon carries are the player's row's (q2_fight.cpp's `player_loop`).
//
// ge:summary Quake II's held weapons' frame sounds: grenade pin, chaingun spin up and down,
// ge:summary hyperblaster wind-down
// ge:keywords q2 weapons hand-step weapon-sounds chaingun spin-up spin-down
// ge:keywords hyperblaster grenade-pin
// ge:skill shooter
// ge:test tests/shooters_edge_q2_fight_test.cpp
#include <utility>

#include "shooters_edge/server/q2_combat.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace {

// A one-shot from player `slot` on `channel` (0 any, 1 the weapon) at `attenuation`.
void hand_sound(Q2Server& s, int slot, const char* sample, int channel, int attenuation) {
  const Q2Player& p = s.players[slot];
  NetEvent ev;
  ev.kind = EventKind::sound;
  ev.to = 0xffffffffu;
  ev.ent = slot + 1;
  ev.level = channel;
  ev.origin = {float(p.move.origin[0]) / 8.0f, float(p.move.origin[1]) / 8.0f,
               float(p.move.origin[2]) / 8.0f};
  ev.values[0] = 255;
  ev.values[1] = float(attenuation);
  ev.text = sample;
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

}  // namespace

const char* const kHandSounds[4] = {"weapons/hgrena1b.wav", "weapons/chngnu1a.wav",
                                    "weapons/chngnd1a.wav", "weapons/hyprbd1a.wav"};

SqStep fight_hand_step(Q2Server& s, int slot, bool attack, bool infinite, int swap) {
  Q2Player& p = s.players[slot];
  const int weapon = p.hand.weapon, was = p.hand.frame;
  const bool was_firing = p.hand.state == SqHandState::firing;
  const SqStep step = sq_hand_step(p.hand, p.body, attack, s.level_time, infinite, swap);
  const int now = p.hand.frame;
  if (p.hand.weapon != weapon || (now == was && was_firing)) return step;
  switch (static_cast<SqWeapon>(weapon)) {
    case SqWeapon::hand_grenade:  // the pin pulled
      if (now == 5) hand_sound(s, slot, kHandSounds[0], 1, 1);
      break;
    case SqWeapon::chaingun:  // spun up from its first fire frame (5, shown with the first shots);
                              // spun down at the burst's end (not when let go early)
      if (!was_firing && p.hand.state == SqHandState::firing)
        hand_sound(s, slot, kHandSounds[1], 0, 2);
      if (now == 22) hand_sound(s, slot, kHandSounds[2], 0, 2);
      break;
    case SqWeapon::hyperblaster:  // wound down
      if (now == 12) hand_sound(s, slot, kHandSounds[3], 0, 1);
      break;
    default:
      break;
  }
  return step;
}

}  // namespace shooters_edge::server
