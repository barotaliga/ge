// Quake II's held weapons' detail sounds (q2_fight.hpp's `fight_hand_step`): the hand's step and
// the one-shot sounds its frames make (q2_combat.hpp's `sq_hand_sound`, shared with the joined
// window's prediction): the hand grenade's pin, the chaingun's spin up and down, the
// hyperblaster's wind-down (porting/shooters-edge/q2-monsters-round4.md § 13). The loops a
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

SqStep fight_hand_step(Q2Server& s, int slot, bool attack, bool infinite, int swap) {
  Q2Player& p = s.players[slot];
  const int weapon = p.hand.weapon, was = p.hand.frame;
  const bool was_firing = p.hand.state == SqHandState::firing;
  // A rest on an idle pause frame, drawn off the frame and the slot (not the rules' stream).
  const uint32_t draw = ge::classic::q2_pause_draw(static_cast<uint32_t>(s.frame_number),
                                                   static_cast<uint32_t>(slot));
  const SqStep step = sq_hand_step(p.hand, p.body, attack, s.level_time, infinite, swap, draw);
  const int k = sq_hand_sound(weapon, was, was_firing, p.hand);
  if (k >= 0)
    hand_sound(s, slot, kSqHandSound[k].sample, kSqHandSound[k].channel,
               kSqHandSound[k].attenuation);
  return step;
}

}  // namespace shooters_edge::server
