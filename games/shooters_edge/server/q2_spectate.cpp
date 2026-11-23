// Quake II's spectators in deathmatch (q2_spectate.hpp).
//
// ge:summary Quake II's spectators: the spectator wish, the switch mid-match, the chase camera
// ge:keywords q2 spectator spectating chase chase-camera sidelines joined-the-game password
// ge:skill shooter
// ge:test tests/shooters_edge_q2_spectate_test.cpp
#include "shooters_edge/server/q2_spectate.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

#include "classic/q2_move.hpp"
#include "core/num.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {
namespace {

namespace num = ge::core::num;
namespace cl = ge::classic;

constexpr double kSwitchAfter = 5;    // seconds from the respawn mark
constexpr float kChaseBack = 30;      // the camera's distance behind the eye
constexpr float kChaseLift = 20;      // its least height over the target's origin
constexpr float kChaseAir = 16;       // more while he is off the ground
constexpr float kChasePitch = 56;     // the most it looks down for its place
constexpr int16_t kJumpUp = 10;       // the up move that counts as a jump's press
constexpr int kQ2PlayerSkins = 1312;  // the players' name-and-skin configstrings, by slot

bool deathmatch(const Q2Server& s) { return s.cvars.find("deathmatch")->on(); }

const std::string& info(const Q2Server& s, int slot, const char* key) {
  static const std::string kNone;
  for (const auto& [k, v] : s.clients[slot].userinfo)
    if (k == key) return v;
  return kNone;
}

// "" and `none` (exactly) mean no password.
bool is_set(const std::string& password) { return !password.empty() && password != "none"; }

void stuff(Q2Server& s, int slot, std::string line) {
  NetEvent ev;
  ev.kind = EventKind::stuffcmd;
  ev.to = 1u << slot;
  ev.reliable = true;
  ev.text = std::move(line);
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

void centre(Q2Server& s, int slot, std::string text) {
  NetEvent ev;
  ev.kind = EventKind::centerprint;
  ev.to = 1u << slot;
  ev.reliable = true;
  ev.text = std::move(text);
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

bool chaseable(const Q2Server& s, int slot) {
  return s.clients[slot].active && !s.clients[slot].spectator;
}

// The next (dir 1) or previous (-1) player from `from`, wrapping; `from` itself when no other.
int next_player(const Q2Server& s, int from, int dir) {
  for (int k = 1; k <= sh::kMaxClients; k++) {
    const int i = ((from + dir * k) % sh::kMaxClients + sh::kMaxClients) % sh::kMaxClients;
    if (chaseable(s, i)) return i;
  }
  return from;
}

int16_t short_angle(float degrees) {
  return static_cast<int16_t>(num::to_int32(double(degrees) * (65536.0 / 360.0)));
}

sh::Vec3f origin_of(const Q2Player& p) {
  return {float(p.move.origin[0]) * 0.125f, float(p.move.origin[1]) * 0.125f,
          float(p.move.origin[2]) * 0.125f};
}

void stop_chase(Q2Player& p) {
  p.chase = -1;
  p.move.type = cl::Q2Type::spectator;
  p.move.flags = static_cast<uint8_t>(p.move.flags & ~cl::kSqNoPrediction);
}

// The camera behind target `t`'s view: 30 back from his eye (pitched down at most 56 for its
// place), at least 20 over his origin (16 more while he is in the air), kept off the walls, then
// off the ceiling and the floor by 6. His view angles are the chaser's; a dead target's view lies
// on its side looking up toward his killer.
void place(Q2Server& s, int chaser, int target) {
  Q2Player& c = s.players[chaser];
  const Q2Player& t = s.players[target];
  const sh::Vec3f origin = origin_of(t);
  const sh::Vec3f eye = origin + sh::Vec3f{0, 0, t.view_height};
  sh::Vec3f look = t.view_angles;
  look.x = std::min(look.x, kChasePitch);
  const sh::Vec3f forward = sh::angle_vectors(look).forward;
  sh::Vec3f goal = eye - forward * kChaseBack;
  goal.z = std::max(goal.z, origin.z + kChaseLift);
  if (!t.on_ground) goal.z += kChaseAir;
  const int mask = sh::kSqSolid | sh::kSqWindow;
  sh::Vec3f at = map_trace(s, eye, {}, {}, goal, mask).end + forward * 2.0f;
  if (const auto up = map_trace(s, at, {}, {}, at + sh::Vec3f{0, 0, 6}, mask); up.fraction < 1)
    at = up.end - sh::Vec3f{0, 0, 6};
  if (const auto down = map_trace(s, at, {}, {}, at - sh::Vec3f{0, 0, 6}, mask); down.fraction < 1)
    at = down.end + sh::Vec3f{0, 0, 6};
  const float pos[3] = {at.x, at.y, at.z};
  for (int i = 0; i < 3; i++) {
    c.move.origin[i] = static_cast<int16_t>(num::to_int32(pos[i] * 8));
    c.move.velocity[i] = 0;
  }
  const sh::Vec3f view = t.alive ? t.view_angles : sh::Vec3f{-15, t.dead_angles.y, 40};
  const int16_t want[3] = {short_angle(view.x), short_angle(view.y), short_angle(view.z)};
  for (int i = 0; i < 3; i++) c.move.delta_angles[i] = int16_t(want[i] - c.cmd_angles[i]);
  c.view_angles = view;
  c.view_height = 0;
  c.move.type = t.alive ? cl::Q2Type::freeze : cl::Q2Type::dead;
  c.move.flags = static_cast<uint8_t>(c.move.flags | cl::kSqNoPrediction);
}

// A chase starts on the first player there is, or says there is none.
void start_chase(Q2Server& s, int slot) {
  for (int i = 0; i < sh::kMaxClients; i++)
    if (chaseable(s, i)) {
      s.players[slot].chase = i;
      place(s, slot, i);
      return;
    }
  centre(s, slot, "No other players to chase.");
}

// The switch to watching (`on`) or back to playing: false, with his word and his user info put
// back, when refused.
bool try_switch(Q2Server& s, int slot, bool on) {
  Q2Player& p = s.players[slot];
  if (on) {
    if (is_set(s.gate.spectator_password) &&
        info(s, slot, "spectator") != s.gate.spectator_password) {
      fight_print(s, slot, "Spectator password incorrect.\n", 2);
      p.want_spectator = false;
      stuff(s, slot, "spectator 0\n");
      return false;
    }
    int watching = 0;  // every wish counts, his own too
    for (int i = 0; i < sh::kMaxClients; i++)
      if (s.clients[i].active && s.players[i].want_spectator) watching++;
    if (watching >= s.gate.max_spectators) {
      fight_print(s, slot, "Server spectator limit is full.", 2);
      p.want_spectator = false;
      stuff(s, slot, "spectator 0\n");
      return false;
    }
  } else if (is_set(s.gate.password) && info(s, slot, "password") != s.gate.password) {
    fight_print(s, slot, "Password incorrect.\n", 2);
    p.want_spectator = true;
    stuff(s, slot, "spectator 1\n");
    return false;
  }
  return true;
}

}  // namespace

bool spectate_wish(const Q2Server& s, int slot) {
  const std::string& v = info(s, slot, "spectator");
  const bool held = slot >= 0 && slot < 32 && ((s.held_out >> slot) & 1u) != 0;
  return deathmatch(s) && (held || (!v.empty() && v != "0"));
}

void spectate_hold(Q2Server& s, int slot, bool held) {
  if (slot < 0 || slot >= sh::kMaxClients || slot >= 32) return;
  const uint32_t bit = 1u << slot;
  s.held_out = held ? s.held_out | bit : s.held_out & ~bit;
  s.players[slot].want_spectator = spectate_wish(s, slot);
}

void spectate_frame(Q2Server& s, bool placed[]) {
  if (!deathmatch(s) || s.intermission_at > 0) return;
  for (int i = 0; i < sh::kMaxClients; i++) {
    placed[i] = false;
    Q2Player& p = s.players[i];
    if (!s.clients[i].active || p.want_spectator == s.clients[i].spectator) continue;
    if (s.level_time + 0.001 < p.respawn_time + kSwitchAfter) continue;
    const bool on = p.want_spectator;
    if (!try_switch(s, i, on)) continue;
    s.clients[i].spectator = on;
    p.frags = 0;
    p.chase = -1;
    placed[i] = true;
    fight_print(s, -1,
                info(s, i, "name") + (on ? " has moved to the sidelines\n" : " joined the game\n"),
                2);
  }
}

bool spectate_command(Q2Server& s, int slot, const sh::MoveCmd& c) {
  Q2Player& p = s.players[slot];
  const bool attack = (c.buttons & sh::kButtonAttack) != 0 && (p.buttons & sh::kButtonAttack) == 0;
  const bool jump = c.up >= kJumpUp && !p.jump_held;
  p.jump_held = c.up >= kJumpUp;
  if (attack) {
    if (p.chase >= 0)
      stop_chase(p);
    else
      start_chase(s, slot);
  } else if (jump) {
    if (p.chase >= 0)
      spectate_step(s, slot, 1);
    else
      start_chase(s, slot);
  }
  if (p.chase < 0) return false;
  p.buttons = c.buttons;
  const float a[3] = {c.angles.x, c.angles.y, c.angles.z};
  for (int i = 0; i < 3; i++) p.cmd_angles[i] = short_angle(a[i]);
  return true;
}

bool spectate_step(Q2Server& s, int slot, int dir) {
  Q2Player& p = s.players[slot];
  if (!s.clients[slot].spectator || p.chase < 0) return false;
  p.chase = next_player(s, p.chase, dir);  // placed when he next moves
  return true;
}

void spectate_follow(Q2Server& s, int slot) {
  for (int i = 0; i < sh::kMaxClients; i++) {
    Q2Player& c = s.players[i];
    if (!s.clients[i].active || !s.clients[i].spectator || c.chase != slot) continue;
    if (s.clients[slot].spectator) {  // he went to watch too: on to the next, or the chase ends
      const int next = next_player(s, slot, 1);
      if (next == slot)
        stop_chase(c);
      else
        c.chase = next;
      continue;
    }
    place(s, i, slot);
  }
}

void spectate_stats(const Q2Server& s, int slot, int16_t* stats) {
  if (!s.clients[slot].spectator) return;
  const Q2Player& p = s.players[slot];
  const int16_t layouts = stats[13];
  if (p.chase >= 0 && s.clients[p.chase].active) {
    const int16_t* theirs = s.world.clients[p.chase].numbers.stats;
    std::copy(theirs, theirs + 32, stats);
    stats[13] = layouts;
    stats[16] = static_cast<int16_t>(kQ2PlayerSkins + p.chase);
  }
  stats[17] = 1;
}

}  // namespace shooters_edge::server
