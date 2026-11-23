// The joined window's weapon predicted (qw_weapon.hpp).
//
// ge:summary The joined window's weapon predicted: the shown command, shots, sounds, ammo drawn
// ge:keywords weapon-prediction shown-command predicted-sound own_shots weapon_restore
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_weapon_test.cpp
#include "shooters_edge/net/qw_weapon.hpp"
#include "shooters_edge/net/weapon_shown.hpp"

#include <algorithm>
#include <string_view>

namespace shooters_edge::net {

namespace {

namespace cl = ge::classic;
namespace dc = shooters_edge::client;

constexpr float kSameShot = 0.05f;  // s of weapon clock: one sound twice this close is one shot
constexpr float kHitEvery = 0.6f;   // s: the lightning gun's hit heard at most this often
constexpr int kMostCommands = 62;

// The stats the prediction draws over.
constexpr int kDrawn[] = {dc::kStatWeapon,  dc::kStatAmmo,  dc::kStatShells,      dc::kStatNails,
                          dc::kStatRockets, dc::kStatCells, dc::kStatActiveWeapon};

int sound_index(const dc::DemoState& d, std::string_view sample) {
  for (size_t i = 1; i < d.sounds.size(); i++)
    if (d.sounds[i] == sample) return static_cast<int>(i);
  return 0;
}

int model_index(const dc::DemoState& d, const char* name) {
  if (name == nullptr) return 0;
  for (size_t i = 1; i < d.models.size(); i++)
    if (d.models[i] == name) return static_cast<int>(i);
  return 0;
}

// The server's copy of a predicted sound, on the player's own entity: dropped.
bool predicted_copy(const RemoteWeapon& w, const dc::DemoState& d, const dc::DemoSound& s, int me) {
  if (s.entity != me + 1 || s.name != nullptr || w.sound_mask == 0) return false;
  if (s.sound <= 0 || static_cast<size_t>(s.sound) >= d.sounds.size()) return false;
  const std::string_view sample = d.sounds[static_cast<size_t>(s.sound)];
  const int bit = cl::weapon_sound_bit_of_sample(sample);
  if (s.channel == 0) return sample == "weapons/lstart.wav" && (w.sound_mask & 256) == 0;
  if (s.channel != 1) return false;
  if (w.sound_mask == 1) return sample != "player/axhit2.wav";
  return bit == 0 || (w.sound_mask & bit) == 0;
}

// Played by the prediction: on, and its weapon's bit not left to the server (weapon_shown.hpp).
bool plays(const RemoteWeapon& w, cl::WeaponSound s) {
  const int bit = cl::weapon_sound_bit(s);
  int key = 0;
  while (key < 30 && (1 << key) != bit) key++;
  return weapon_sound_plays(w.sound_mask, key);
}

}  // namespace

void weapon_restore(RemoteWeapon& w, dc::DemoState& d) {
  if (!w.applied) return;
  for (const int k : kDrawn) d.stats[k] = w.stats[k];
  if (d.player >= 0 && static_cast<size_t>(d.player) < d.players.size())
    d.players[static_cast<size_t>(d.player)].weapon_frame = w.frame;
  w.applied = false;
}

void weapon_predict(RemoteWeapon& w, const QwClientEnd& c, dc::DemoState& d, int waterlevel,
                    const ge::shooter::Vec3f& origin, const std::string& w_rank) {
  const int me = c.playernum;
  const PlayerState& own = c.players[me >= 0 && me < 32 ? me : 0];
  const uint32_t acked = c.ch.incoming_acknowledged;
  const uint32_t next = c.ch.outgoing_sequence;
  w.on = !w.off && me >= 0 && me == d.player && c.player_seen[me] && !c.cfg.spectator &&
         c.ext.has(ExtFamily::mvd, kMvdWeaponPred) && own.weapon.known &&
         own.weapon.flags != cl::kWeaponFlagsOff && !d.intermission && !d.paused && acked != 0 &&
         next - 1 - acked < 63;
  w.fired.clear();
  if (!w.on) {
    w.played_to = acked;
    return;
  }
  // The server's copies of what the prediction plays, dropped before its own are added.
  std::erase_if(d.started, [&](const dc::DemoSound& snd) { return predicted_copy(w, d, snd, me); });
  // The shown command: `buffer` + 1 before the next to send, within the commands in flight.
  const uint32_t shown = weapon_shown_command(acked, next, w.buffer);
  if (static_cast<int32_t>(w.played_to - acked) < 0) w.played_to = acked;
  cl::WeaponState s = own.weapon;
  s.frame = own.weaponframe;
  const cl::WeaponWorld world{static_cast<int>(d.stats[dc::kStatItems]), w_rank};
  const bool frozen = own.dead || own.pm == PmType::none || own.pm == PmType::lock;
  int run = 0;
  for (uint32_t q = acked + 1; static_cast<int32_t>(q - shown) <= 0 && run < kMostCommands; q++) {
    if (c.sent_seq[q & 63] != q) continue;
    run++;
    const ge::shooter::MoveCmd& m = c.sent[q & 63];
    cl::WeaponCmd cmd;
    cmd.msec = m.msec;
    cmd.attack = (m.buttons & ge::shooter::kButtonAttack) != 0;
    cmd.impulse = m.impulse;
    cmd.frozen = frozen;
    cmd.waterlevel = waterlevel;
    cl::WeaponEvent ev[8];
    const int n = cl::weapon_command(s, cmd, world, ev, 8);
    if (static_cast<int32_t>(q - w.played_to) <= 0) continue;  // played by an earlier pass
    for (int i = 0; i < n; i++) {
      const float gap = ev[i].sound == cl::WeaponSound::lightning_hit ? kHitEvery : kSameShot;
      if (!weapon_shot_fresh(w.last_shot, static_cast<int>(std::size(w.last_shot)),
                             static_cast<int>(ev[i].sound), ev[i].clock, gap))
        continue;
      w.fired.push_back({ev[i], q, (s.flags & 1) != 0});
      if (ev[i].sound != cl::WeaponSound::lightning_hit) weapon_shot_fired(d, s.frame);
      if (!plays(w, ev[i].sound)) continue;
      dc::DemoSound snd;
      snd.entity = me + 1;
      snd.channel = cl::weapon_sound_channel(ev[i].sound);
      const char* sample = cl::weapon_sound_sample(ev[i].sound);
      snd.sound = sound_index(d, sample);
      if (snd.sound == 0) snd.name = sample;
      snd.attenuation = 0;
      snd.origin = origin;
      d.started.push_back(snd);
    }
  }
  if (static_cast<int32_t>(shown - w.played_to) > 0) w.played_to = shown;
  // Drawn: the predicted weapon, its frame and the ammunition, the server's kept to put back.
  w.shown = s;
  for (const int k : kDrawn) w.stats[k] = d.stats[k];
  w.frame = d.players[static_cast<size_t>(me)].weapon_frame;
  w.applied = true;
  const int model = model_index(d, cl::weapon_view_model(s.weapon));
  if (model != 0 && d.stats[dc::kStatWeapon] != 0) d.stats[dc::kStatWeapon] = model;
  d.stats[dc::kStatActiveWeapon] = s.weapon;
  d.stats[dc::kStatAmmo] = cl::weapon_current_ammo(s);
  for (int i = 0; i < 4; i++) d.stats[dc::kStatShells + i] = s.ammo[i];
  // The frame only from a server that animates it (rules whose shots run frame by frame): one whose
  // frames stay 0 leaves the hand to play its own shot (client/held.cpp).
  if (own.weaponframe != 0) w.frames_live = true;
  if (w.frames_live) d.players[static_cast<size_t>(me)].weapon_frame = s.frame;
}

}  // namespace shooters_edge::net
