// The window joined to a server of Quake II, its weapon predicted (q2_weapon.hpp).
//
// ge:summary The joined window's Quake II weapon predicted: the hand's runs, the frames, the shots
// ge:keywords weapon-prediction q2 q2_hand_move q2_hand_of_frame frame-boundary press-latch
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_weapon_test.cpp
#include "shooters_edge/net/q2_weapon.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string_view>
#include <vector>

#include "shooters_edge/client/q2_flash_fx.hpp"
#include "shooters_edge/net/q2_client.hpp"
#include "shooters_edge/net/q2_predict.hpp"
#include "shooters_edge/net/qw_weapon.hpp"
#include "shooters_edge/net/weapon_shown.hpp"
#include "shooters_edge/server/q2_flash.hpp"
#include "shooters_edge/server/q2_items.hpp"

namespace shooters_edge::net {

namespace {

namespace sv = shooters_edge::server;
namespace dc = shooters_edge::client;

constexpr double kFrameS = 0.1;     // a server frame of Quake II, s
constexpr double kEarly = 1e-6;     // s: a move this close to a frame's start is in the new frame
constexpr float kSameShot = 0.5f;   // server frames: a flash of the same value in one is one shot
constexpr int kStatAmmoSent = 3;    // the player state's ammunition stat
constexpr int kStatHealthSent = 1;  // and its health
constexpr int kNoFlashKind = 15;    // `Q2Weapon::last_shot`'s slot of the shots with no flash

// The weapon (kSqWeapons' row) a player flash value of a shot is of; -1 none (a login's, a
// respawn's, a mission pack's).
int weapon_of_flash(int value) {
  value &= ~sv::kQ2FlashSilenced;
  for (int w = 0; w < sv::kSqWeaponCount; w++)
    for (int shots = 1; shots <= 3; shots++)
      if (sv::sq_flash_value(w, shots) == value) return w;
  return -1;
}

// One step of the rules, its shot written to `out` when it fires one.
int step_once(Q2HandRun& r, bool attack, double at, Q2HandShot* out, int room) {
  const sv::SqWeaponRow& row = sv::kSqWeapons[r.hand.weapon];
  const bool ready = r.hand.state == sv::SqHandState::ready;
  // Short of a shot: the server's choice of the next weapon is not guessed, the hand held.
  if (ready && attack && !sv::sq_can_fire(r.body, r.hand.weapon)) return 0;
  const sv::SqStep s = sv::sq_hand_step(r.hand, r.body, attack, at);
  if (ready && attack) r.pressed = false;  // a ready hand takes the press
  if (s.shots <= 0 || room <= 0) return 0;
  const int flash = sv::sq_flash_value(r.hand.weapon, s.shots);
  out->flash = flash >= 0 && sv::sq_flash_plays(flash, row.sound) ? flash : -1;
  out->at = at;
  return 1;
}

}  // namespace

int q2_weapon_of_model(const char* model) {
  if (model == nullptr) return -1;
  for (int w = 0; w < sv::kSqWeaponCount; w++)
    if (std::strcmp(model, sv::kSqWeapons[w].view_model) == 0) return w;
  return -1;
}

sv::SqHand q2_hand_of_frame(int weapon, int frame, int before) {
  sv::SqHand h;
  h.weapon = std::clamp(weapon, 0, sv::kSqWeaponCount - 1);
  h.frame = frame;
  const sv::SqWeaponRow& r = sv::kSqWeapons[h.weapon];
  if (frame <= r.last_activate) {
    h.state = sv::SqHandState::activating;
  } else if (frame <= r.last_fire) {
    h.state = sv::SqHandState::firing;
  } else if (frame <= r.last_idle) {
    // The frame after the fire run is still firing when the frame before was a fire frame.
    const bool shot = frame == r.last_fire + 1 && before > r.last_activate && before <= r.last_fire;
    h.state = shot ? sv::SqHandState::firing : sv::SqHandState::ready;
  } else {
    h.state = sv::SqHandState::dropping;
  }
  return h;
}

int q2_hand_move(Q2HandRun& r, double at, bool attack, Q2HandShot* out, int room) {
  int n = 0;
  // The frames that start by this move: the hand steps once each, unless a move stepped it.
  while (at + kEarly >= r.frame_at + kFrameS) {
    r.frame_at += kFrameS;
    if (!r.stepped) n += step_once(r, r.held || r.pressed, r.frame_at, out + n, room - n);
    r.stepped = false;
    r.pressed = false;  // a stock server drops a press its frame's step did not take
  }
  if (r.latch && attack && !r.held) r.pressed = true;
  const bool steps = r.latch ? r.pressed : attack;
  if (steps && !r.stepped) {
    r.stepped = true;
    n += step_once(r, true, std::max(at, r.frame_at), out + n, room - n);
  }
  r.held = attack;
  return n;
}

void q2_phase_frame(Q2FramePhase& p, int32_t frame, double sent, double next) {
  if (frame == p.frame) return;
  p.frame = frame;
  const double start = static_cast<double>(frame) * kFrameS;
  const double lo = sent - start, hi = next - start;
  if (p.have && lo <= p.hi && hi >= p.lo) {  // agrees: the window narrowed
    p.lo = std::max(p.lo, lo);
    p.hi = std::min(p.hi, hi);
    return;
  }
  // The first frame, or one that disagrees (the latency changed): its own window. One with no next
  // packet yet is a packet's interval wide.
  p.lo = lo;
  p.hi = hi > lo + 1 ? lo + kFrameS * 0.5 : hi;
  p.have = true;
}

void q2_weapon_predict(Q2Weapon& p, const RemoteWeapon& w, bool nopred, const Q2ClientEnd& c,
                       dc::Demo& d, const ge::shooter::Vec3f& origin) {
  dc::DemoState& st = d.state;
  const int me = st.player;
  // The server's flashes of the player's shots this window frame: silenced or not.
  for (const dc::DemoSound& snd : st.started)
    if (snd.entity == me + 1 && snd.flash >= 0 && weapon_of_flash(snd.flash) >= 0)
      p.silenced = (snd.flash & sv::kQ2FlashSilenced) != 0;
  const uint32_t acked = c.ch.incoming_acknowledged;
  const uint32_t first = c.made_at[acked & (kSqSentRing - 1)];
  p.on = false;
  const auto off = [&] {
    p.played_to = first - 1;
    return;
  };
  if (w.off || nopred || c.stage != ClientStage::active || d.last_frame < 0 || st.intermission ||
      me < 0 || static_cast<size_t>(me) >= st.players.size() || c.cfg.spectator)
    return off();
  const dc::Q2View& v = d.views[static_cast<size_t>(d.last_frame & 15)];
  if (v.pm_type != 0 || v.stats[kStatHealthSent] <= 0) return off();
  if (v.gun_model <= 0 || static_cast<size_t>(v.gun_model) >= st.models.size()) return off();
  const int weapon = q2_weapon_of_model(st.models[static_cast<size_t>(v.gun_model)].c_str());
  if (weapon < 0 || weapon == static_cast<int>(sv::SqWeapon::hand_grenade)) return off();
  if (c.ch.outgoing_sequence - acked >= static_cast<uint32_t>(kSqSentRing)) return off();
  if (c.made - first >= static_cast<uint32_t>(kSqSentRing) || c.made == first) return off();
  p.on = true;
  // The server's copies of what the prediction plays, dropped before its own are added.
  std::erase_if(st.started, [&](const dc::DemoSound& snd) {
    if (snd.entity != me + 1 || snd.flash < 0) return false;
    const int of = weapon_of_flash(snd.flash);
    return of >= 0 && weapon_sound_plays(w.sound_mask, of + 1);
  });
  // The hand the newest frame shows, with the frame before it for the end of a fire run.
  const dc::Q2View& b4 = d.views[static_cast<size_t>((d.last_frame - 1) & 15)];
  const int before = b4.frame == v.frame - 1 && b4.gun_model == v.gun_model ? b4.gun_frame : -1;
  Q2HandRun r;
  r.hand = q2_hand_of_frame(weapon, v.gun_frame, before);
  const sv::SqWeaponRow& row = sv::kSqWeapons[weapon];
  r.body.ammo[static_cast<int>(row.ammo)] = v.stats[kStatAmmoSent];
  if (row.item != nullptr) {
    const int item = sv::sq_item(row.item);
    if (item >= 0) r.body.counts[item] = 1;
  }
  r.latch = !d.feel_told || p.stock_rule;  // a stock server: only a new press fires at once
  r.frame_at = static_cast<double>(v.frame) * kFrameS;
  // When the server runs each move: its packet's send time less the frames' offset.
  const uint32_t sent_to = c.ch.outgoing_sequence;  // the next packet's: not yet sent
  const double far = 1e9;
  q2_phase_frame(p.phase, v.frame, c.sent_at[acked & (kSqSentRing - 1)],
                 acked + 1 != sent_to ? c.sent_at[(acked + 1) & (kSqSentRing - 1)] : far);
  const double offset = q2_phase_offset(p.phase);
  double move_at[kSqSentRing];
  for (uint32_t k = first; k != c.made; k++)  // not yet sent: the next packet's, a tick on
    move_at[(k - first) & (kSqSentRing - 1)] = c.now + 0.016 - offset;
  for (uint32_t q = sent_to - 1; static_cast<int32_t>(q - acked) > 0; q--) {
    const uint32_t from = c.made_at[(q - 1) & (kSqSentRing - 1)];
    const uint32_t to = c.made_at[q & (kSqSentRing - 1)];
    for (uint32_t k = from; k != to; k++)
      if (static_cast<int32_t>(k - first) >= 0)
        move_at[(k - first) & (kSqSentRing - 1)] = c.sent_at[q & (kSqSentRing - 1)] - offset;
  }
  r.held = (c.sent[(first - 1) & (kSqSentRing - 1)].buttons & ge::shooter::kButtonAttack) != 0;
  const uint32_t shown = weapon_shown_command(first - 1, c.made, w.buffer);
  if (static_cast<int32_t>(p.played_to - (first - 1)) < 0) p.played_to = first - 1;
  for (uint32_t k = first; static_cast<int32_t>(k - shown) <= 0; k++) {
    const ge::shooter::MoveCmd& m = c.sent[k & (kSqSentRing - 1)];
    Q2HandShot shots[8];
    const int n = q2_hand_move(r, move_at[(k - first) & (kSqSentRing - 1)],
                               (m.buttons & ge::shooter::kButtonAttack) != 0, shots, 8);
    if (static_cast<int32_t>(k - p.played_to) <= 0) continue;  // played by an earlier pass
    for (int i = 0; i < n; i++) {
      const int kind = shots[i].flash >= 0 ? shots[i].flash : kNoFlashKind;
      // One step a server frame, so one shot: the frame it falls in names it.
      const auto in_frame = static_cast<float>(std::floor(shots[i].at / kFrameS + kEarly));
      if (!weapon_shot_fresh(p.last_shot, static_cast<int>(std::size(p.last_shot)), kind, in_frame,
                             kSameShot))
        continue;
      weapon_shot_fired(st, r.hand.frame);
      if (shots[i].flash < 0 || !weapon_sound_plays(w.sound_mask, weapon + 1)) continue;
      const int value = shots[i].flash | (p.silenced ? sv::kQ2FlashSilenced : 0);
      const auto pick = static_cast<uint32_t>((me + 1) * 7919 + static_cast<int>(k) * 31 + value);
      client::q2_flash_fx(value, false, me + 1, origin, st.angles, true, pick, st.started,
                          st.effects);
    }
  }
  if (static_cast<int32_t>(shown - p.played_to) > 0) p.played_to = shown;
  // Drawn: the hand's frame and the ammunition at the shown move.
  p.shown = r.hand;
  p.ammo = r.body.ammo[static_cast<int>(row.ammo)];
  st.players[static_cast<size_t>(me)].weapon_frame = r.hand.frame;
  if (row.ammo != sv::SqAmmo::none) {
    st.stats[dc::kStatAmmo] = p.ammo;
    st.q2_stats[kStatAmmoSent] = static_cast<int16_t>(p.ammo);
  }
}

}  // namespace shooters_edge::net
