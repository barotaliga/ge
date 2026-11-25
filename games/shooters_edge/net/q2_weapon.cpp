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

// The number of the frame starting at level time `at`.
int32_t frame_of(double at) { return static_cast<int32_t>(std::floor(at / kFrameS + 0.5)); }

// One step of the rules, what it made written to `out` when it shot or played a frame sound.
int step_once(Q2HandRun& r, bool attack, double at, int32_t frame, Q2HandShot* out, int room) {
  const int weapon = r.hand.weapon, was = r.hand.frame;
  const sv::SqWeaponRow& row = sv::kSqWeapons[weapon];
  const bool ready = r.hand.state == sv::SqHandState::ready;
  const bool was_firing = r.hand.state == sv::SqHandState::firing;
  // Short of a shot: unless every weapon is known, the server's choice of the next is not
  // guessed, the hand held.
  if (ready && attack && !r.known && !sv::sq_can_fire(r.body, weapon)) return 0;
  const sv::SqStep s = sv::sq_hand_step(r.hand, r.body, attack, at);
  if (ready && attack) r.pressed = false;  // a ready hand takes the press
  const int sound = sv::sq_hand_sound(weapon, was, was_firing, r.hand);
  if ((s.shots <= 0 && sound < 0) || room <= 0) return 0;
  *out = Q2HandShot{};
  out->shot = s.shots > 0;
  out->sound = sound;
  out->weapon = weapon;
  out->at = at;
  out->frame = frame;
  if (out->shot) {
    const int flash = sv::sq_flash_value(weapon, s.shots);
    out->flash = flash >= 0 && sv::sq_flash_plays(flash, row.sound) ? flash : -1;
    // A hand grenade held to its fuse's end goes off in the hand: no throw heard.
    out->quiet = weapon == static_cast<int>(sv::SqWeapon::hand_grenade) && s.throw_speed <= 0;
    out->speed = s.throw_speed;
  }
  return 1;
}

// The index of `name` in a precache list (0 none).
int listed(const std::vector<std::string>& list, const char* name) {
  for (size_t k = 1; k < list.size(); k++)
    if (list[k] == name) return static_cast<int>(k);
  return 0;
}

// The kind (`Q2Weapon::played`'s) a server sound named `name` is of when the prediction plays its
// like: a hand sound's row, 100 a weapon's shot heard by name (no flash: the hand grenade's throw);
// -2 none.
int named_kind(const std::string& name) {
  for (int k = 0; k < sv::kSqHandSounds; k++)
    if (name == sv::kSqHandSound[k].sample) return k;
  for (int w = 0; w < sv::kSqWeaponCount; w++) {
    const int flash = sv::sq_flash_value(w, 1);
    if (name == sv::kSqWeapons[w].sound &&
        (flash < 0 || !sv::sq_flash_plays(flash, sv::kSqWeapons[w].sound)))
      return 100;
  }
  return -2;
}

// Whether `name` is a loop a hand carries (any weapon's, in any state).
bool is_hand_loop(const std::string& name) {
  sv::SqHand h;
  for (int w = 0; w < sv::kSqWeaponCount; w++) {
    h.weapon = w;
    h.frame = 11;
    h.fuse_at = 1;
    for (const auto state : {sv::SqHandState::ready, sv::SqHandState::firing}) {
      h.state = state;
      const char* l = sv::sq_hand_loop(h);
      if (l != nullptr && name == l) return true;
    }
  }
  return false;
}

// The status bar's ammunition picture of each kind (the pictures Quake II's items name).
constexpr const char* kAmmoIcon[sv::kSqAmmoKinds] = {
    "", "a_shells", "a_bullets", "a_grenades", "a_rockets", "a_cells", "a_slugs"};

// The newest frame folded into what the window knows: the weapon in hand held, its ammunition
// seen; our server's arsenal when told; all forgotten at death.
void know_frame(Q2Known& k, const dc::Demo& d, int32_t frame, int weapon, int ammo, bool dead) {
  if (frame == k.frame) return;
  k.frame = frame;
  if (dead) {
    k = Q2Known{};
    k.frame = frame;
    return;
  }
  if (weapon != k.held) {
    if (k.held >= 0) k.last = k.held;
    k.held = weapon;
  }
  if (d.arsenal_told) {
    static_assert(sv::kSqAmmoKinds <= dc::Demo::kArsenalAmmo && sv::kSqAmmoKinds <= 16,
                  "the arsenal and the seen mask hold every kind");
    k.weapons = d.arsenal_weapons;
    for (int a = 1; a < sv::kSqAmmoKinds; a++) {
      k.ammo[a] = d.arsenal_ammo[a];
      k.ammo_seen = static_cast<uint16_t>(k.ammo_seen | 1u << a);
    }
  }
  if (weapon < 0) return;
  k.weapons |= 1u << weapon;
  const int a = static_cast<int>(sv::kSqWeapons[weapon].ammo);
  if (a > 0) {  // the frame's own count, newer than any arsenal
    k.ammo[a] = static_cast<int16_t>(ammo);
    k.ammo_seen = static_cast<uint16_t>(k.ammo_seen | 1u << a);
  }
}

// The body the rules run on: the weapons known held, the ammunition known (else none).
sv::SqBody body_of(const Q2Known& k) {
  sv::SqBody b;
  b.blaster = true;  // never lost
  for (int w = 0; w < sv::kSqWeaponCount; w++) {
    if ((k.weapons & (1u << w)) == 0 || sv::kSqWeapons[w].item == nullptr) continue;
    const int item = sv::sq_item(sv::kSqWeapons[w].item);
    if (item >= 0 && sv::kSqItems[item].kind != sv::SqKind::ammo) b.counts[item] = 1;
  }
  for (int a = 1; a < sv::kSqAmmoKinds; a++)
    b.ammo[a] = (k.ammo_seen >> a & 1u) != 0 ? k.ammo[a] : 0;
  return b;
}

// A weapon change asked for, run on the hand as the server's command does (its weapon held, a
// shot's ammunition known; `weapnext` and `weapprev` only when every weapon is known); `drawn`
// says the window can draw the weapon (its view model listed).
void ask_weapon(Q2HandRun& r, const Q2ClientEnd::WeaponAsk& a, const Q2Known& k, auto drawn) {
  int w = -1;
  if (a.kind == 0) {
    w = static_cast<int>(a.weapon);
  } else if (a.kind == 3) {
    w = k.last;
  } else if (r.known) {
    const int way = a.kind == 1 ? 1 : sv::kSqWeaponCount - 1;
    for (int n = 1; n < sv::kSqWeaponCount && w < 0; n++) {
      const int to = (r.hand.weapon + way * n) % sv::kSqWeaponCount;
      if (sv::sq_can_fire(r.body, to)) w = to;
    }
  }
  if (w < 0 || w >= sv::kSqWeaponCount || !drawn(w)) return;
  const bool grenade = w == static_cast<int>(sv::SqWeapon::hand_grenade);
  if (!(grenade ? sv::sq_holds(r.body, w) : sv::sq_can_fire(r.body, w))) return;
  sv::sq_choose(r.hand, r.body, w);
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
    if (!r.stepped)
      n += step_once(r, r.held || r.pressed, r.frame_at, frame_of(r.frame_at), out + n, room - n);
    r.stepped = false;
    r.pressed = false;  // a stock server drops a press its frame's step did not take
  }
  if (r.latch && attack && !r.held) r.pressed = true;
  const bool steps = r.latch ? r.pressed : attack;
  if (steps && !r.stepped) {
    r.stepped = true;
    n += step_once(r, true, std::max(at, r.frame_at), frame_of(r.frame_at) + 1, out + n, room - n);
  }
  r.held = attack;
  return n;
}

void q2_phase_frame(Q2FramePhase& p, int32_t frame, double sent, double next) {
  if (frame == p.frame) return;
  p.frame = frame;
  const double start = static_cast<double>(frame) * kFrameS;
  const double lo = sent - start;
  double hi = next - start;
  if (hi > lo + 1) {  // no next packet yet: a guess only while nothing better is kept
    if (p.count > 0) return;
    hi = lo + kFrameS * 0.5;
  }
  const int at = static_cast<int>(p.added++ % kQ2PhaseFrames);
  p.lo[at] = lo;
  p.hi[at] = hi;
  p.count = std::min(p.count + 1, kQ2PhaseFrames);
}

double q2_phase_offset(const Q2FramePhase& p) {
  if (p.count == 0) return 0;
  // The windows' ends in order, a start before an end at one time; the sweep's deepest span.
  struct End {
    double t;
    int step;
  };
  End ends[2 * kQ2PhaseFrames];
  int n = 0;
  for (int k = 0; k < p.count; k++) {
    ends[n++] = {p.lo[k], +1};
    ends[n++] = {p.hi[k], -1};
  }
  std::sort(ends, ends + n, [](const End& a, const End& b) {
    return a.t < b.t || (a.t == b.t && a.step > b.step);
  });
  int depth = 0, best = 0;
  double from = 0, to = 0;
  for (int k = 0; k < n; k++) {
    depth += ends[k].step;
    if (ends[k].step < 0 || k + 1 >= n) continue;
    const double width = ends[k + 1].t - ends[k].t;
    if (depth > best || (depth == best && width > to - from)) {
      best = depth;
      from = ends[k].t;
      to = ends[k + 1].t;
    }
  }
  return (from + to) * 0.5;
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
  p.fired.clear();
  const auto off = [&] {
    p.played_to = first - 1;
    return;
  };
  if (w.off || nopred || c.stage != ClientStage::active || d.last_frame < 0 || st.intermission ||
      me < 0 || static_cast<size_t>(me) >= st.players.size() || c.cfg.spectator)
    return off();
  const dc::Q2View& v = d.views[static_cast<size_t>(d.last_frame & 15)];
  const bool dead = v.stats[kStatHealthSent] <= 0;
  const int weapon = v.gun_model > 0 && static_cast<size_t>(v.gun_model) < st.models.size()
                         ? q2_weapon_of_model(st.models[static_cast<size_t>(v.gun_model)].c_str())
                         : -1;
  know_frame(p.known, d, v.frame, weapon, v.stats[kStatAmmoSent], dead);
  if (v.pm_type != 0 || dead || weapon < 0) return off();
  if (c.ch.outgoing_sequence - acked >= static_cast<uint32_t>(kSqSentRing)) return off();
  if (c.made - first >= static_cast<uint32_t>(kSqSentRing) || c.made == first) return off();
  p.on = true;
  // The server's copies of what the prediction plays, dropped before its own are added: each
  // matched to a predicted one in or beside its frame, else played as it comes (late, not lost).
  std::erase_if(st.started, [&](const dc::DemoSound& snd) {
    if (snd.entity != me + 1) return false;
    int kind = -2;
    if (snd.flash >= 0) {
      const int of = weapon_of_flash(snd.flash);
      if (of < 0) return false;
      p.audit.server[p.audit.server_n++ & 31] = v.frame;  // sent with the newest frame
      if (!weapon_sound_plays(w.sound_mask, of + 1)) return false;
      kind = -1;
    } else if (snd.name == nullptr && snd.sound > 0 &&
               static_cast<size_t>(snd.sound) < st.sounds.size()) {
      kind = named_kind(st.sounds[static_cast<size_t>(snd.sound)]);
      if (kind == 100 && !weapon_sound_plays(w.sound_mask, weapon + 1)) return false;
    }
    if (kind == -2) return false;
    if (weapon_played_match(p.played, v.frame, kind)) return true;
    if (kind == -1) p.audit.filled++;
    return false;
  });
  // The hand the newest frame shows, with the frame before it for the end of a fire run.
  const dc::Q2View& b4 = d.views[static_cast<size_t>((d.last_frame - 1) & 15)];
  const int before = b4.frame == v.frame - 1 && b4.gun_model == v.gun_model ? b4.gun_frame : -1;
  Q2HandRun r;
  r.hand = q2_hand_of_frame(weapon, v.gun_frame, before);
  r.body = body_of(p.known);
  r.known = d.arsenal_told;
  // A hand grenade cooking in the server's frame: its fuse as the prediction lit it (else from
  // that frame, a little late).
  const int grenade = static_cast<int>(sv::SqWeapon::hand_grenade);
  const bool cooking = weapon == grenade && v.gun_frame >= 11 && v.gun_frame <= 15;
  if (cooking) {
    if (p.fuse_at <= 0) p.fuse_at = static_cast<double>(v.frame) * kFrameS + sv::kSqGrenadeFuse;
    r.hand.fuse_at = p.fuse_at;
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
  // The weapon changes asked for: those the server ran lately again (its frame may not show the
  // change chosen yet), then each before the moves of its packet.
  const auto drawn = [&](int to) { return listed(st.models, sv::kSqWeapons[to].view_model) > 0; };
  const uint32_t asks = std::min(c.weapon_asked, 8u);
  constexpr uint32_t kLately = 16;  // moves: about two of the server's frames
  for (uint32_t n = asks; n > 0; n--) {
    const Q2ClientEnd::WeaponAsk& a = c.weapon_asks[(c.weapon_asked - n) & 7];
    if (static_cast<int32_t>(a.move - first) < 0 &&
        static_cast<int32_t>(a.move + kLately - first) >= 0)
      ask_weapon(r, a, p.known, drawn);
  }
  for (uint32_t k = first; static_cast<int32_t>(k - shown) <= 0; k++) {
    for (uint32_t n = asks; n > 0; n--) {
      const Q2ClientEnd::WeaponAsk& a = c.weapon_asks[(c.weapon_asked - n) & 7];
      if (a.move == k) ask_weapon(r, a, p.known, drawn);
    }
    const ge::shooter::MoveCmd& m = c.sent[k & (kSqSentRing - 1)];
    Q2HandShot made[8];
    const int n = q2_hand_move(r, move_at[(k - first) & (kSqSentRing - 1)],
                               (m.buttons & ge::shooter::kButtonAttack) != 0, made, 8);
    if (static_cast<int32_t>(k - p.played_to) <= 0) continue;  // played by an earlier pass
    for (int i = 0; i < n; i++) {
      const Q2HandShot& e = made[i];
      // One step a server frame, so one shot and one frame sound: the frame it is sent with
      // names it (a frame's own step at its start, a move's between the frame before and it).
      const auto in_frame = static_cast<float>(e.frame);
      if (e.sound >= 0 &&
          weapon_shot_fresh(p.last_sound, sv::kSqHandSounds, e.sound, in_frame, kSameShot)) {
        const sv::SqHandSound& hs = sv::kSqHandSound[e.sound];
        const int index = listed(st.sounds, hs.sample);
        if (index > 0) {
          weapon_played_add(p.played, e.frame, e.sound);
          dc::DemoSound snd;
          snd.entity = me + 1;
          snd.channel = hs.channel;
          snd.sound = index;
          snd.attenuation = static_cast<float>(hs.attenuation);
          snd.origin = origin;
          st.started.push_back(snd);
        }
      }
      if (!e.shot) continue;
      const int kind = e.flash >= 0 ? e.flash : kNoFlashKind;
      if (!weapon_shot_fresh(p.last_shot, static_cast<int>(std::size(p.last_shot)), kind, in_frame,
                             kSameShot))
        continue;
      weapon_shot_fired(st, r.hand.frame);
      const double sent = move_at[(k - first) & (kSqSentRing - 1)] + offset;
      p.fired.push_back({k, e.weapon, r.hand.frame, e.speed, std::max(0.0, c.now - sent)});
      p.audit.predicted[p.audit.predicted_n++ & 31] = e.frame;
      if (e.quiet || !weapon_sound_plays(w.sound_mask, e.weapon + 1)) continue;
      if (e.flash < 0) {  // heard by name: the hand grenade's throw
        const int index = listed(st.sounds, sv::kSqWeapons[e.weapon].sound);
        if (index <= 0) continue;
        weapon_played_add(p.played, e.frame, 100);
        dc::DemoSound snd;
        snd.entity = me + 1;
        snd.channel = 1;
        snd.sound = index;
        snd.volume = p.silenced ? 0.2f : 1.0f;
        snd.origin = origin;
        st.started.push_back(snd);
        continue;
      }
      weapon_played_add(p.played, e.frame, -1);
      const int value = e.flash | (p.silenced ? sv::kQ2FlashSilenced : 0);
      const auto pick = static_cast<uint32_t>((me + 1) * 7919 + static_cast<int>(k) * 31 + value);
      client::q2_flash_fx(value, false, me + 1, origin, st.angles, true, pick, st.started,
                          st.effects);
    }
  }
  if (static_cast<int32_t>(shown - p.played_to) > 0) p.played_to = shown;
  // The hand grenade's fuse as this pass lit it, kept for the frames that show it cooking.
  if (!cooking) p.fuse_at = r.hand.weapon == grenade && r.hand.fuse_at > 0 ? r.hand.fuse_at : 0;
  // Drawn: the hand's weapon, frame and loop, and the ammunition, at the shown move.
  p.shown = r.hand;
  const sv::SqWeaponRow& row = sv::kSqWeapons[r.hand.weapon];
  p.ammo = r.body.ammo[static_cast<int>(row.ammo)];
  st.players[static_cast<size_t>(me)].weapon_frame = r.hand.frame;
  if (r.hand.weapon != weapon) {  // a change predicted: its model and its ammunition's picture
    st.stats[dc::kStatWeapon] = listed(st.models, row.view_model);
    st.q2_stats[2] = static_cast<int16_t>(
        row.ammo == sv::SqAmmo::none ? 0
                                     : listed(st.images, kAmmoIcon[static_cast<int>(row.ammo)]));
  }
  st.stats[dc::kStatAmmo] = row.ammo != sv::SqAmmo::none ? p.ammo : 0;
  st.q2_stats[kStatAmmoSent] = static_cast<int16_t>(st.stats[dc::kStatAmmo]);
  // The loop the hand carries on the player (a hum, a spin, a fuse), unless the server's is
  // another (frying in lava).
  if (static_cast<size_t>(me) + 1 < st.entities.size()) {
    int& loop = st.entities[static_cast<size_t>(me) + 1].loop;
    const bool hand_loop = loop == 0 || (static_cast<size_t>(loop) < st.sounds.size() &&
                                         is_hand_loop(st.sounds[static_cast<size_t>(loop)]));
    const char* mine = sv::sq_hand_loop(r.hand);
    if (hand_loop) loop = mine != nullptr ? listed(st.sounds, mine) : 0;
  }
}

}  // namespace shooters_edge::net
