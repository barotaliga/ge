// The co-op companion (companion.hpp states what it does and why).
//
// ge:summary The co-op companion: threats marked, the leader followed, buttons pressed, boredom
// ge:keywords escort_init escort_sense escort_think escort_command shoulder threat button
// ge:keywords bored idle companion
// ge:skill shooter bots
// ge:test tests/shooters_edge_companion_test.cpp
#include "shooters_edge/bots/companion.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/nav.hpp"
#include "shooter/trace.hpp"
#include "shooters_edge/bots/fighter.hpp"
#include "shooters_edge/profiles/items.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::bots {

namespace {

namespace ai = ge::ai;
namespace libm = ge::core::libm;

constexpr double kDeg = 180.0 / ge::core::kPi;
constexpr float kStill = 48;        // the leader within this of where he stopped stands still
constexpr float kMoving = 60;       // and slower than this, units/s
constexpr float kInHisWay = 44;     // nearer his line than this, ahead of him: give way, units
constexpr float kAhead = 170;       // within this ahead of him, units
constexpr float kRetarget = 64;     // the shoulder spot moved this far: a new node
constexpr float kTurnRate = 720;    // degrees/s the companion's own view turns at
constexpr float kLookRate = 260;    // and a bored look's
constexpr float kPressReach = 120;  // a touch button walked into from this near, units
constexpr float kShootReach = 900;  // a shot button fired at from this near, units
constexpr int32_t kWGrenades = 16,
                  kWRockets = 32;         // the blast weapons' W_ bits (games/shooters_edge/qc)
constexpr float kLookSight = 700;         // an item this near and in sight is worth a bored look
constexpr float kHopOut[2] = {180, 320};  // a bored hop's turn point from the leader, units

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

float flat_dist(sh::Vec3f a, sh::Vec3f b) {
  const float dx = a.x - b.x, dy = a.y - b.y;
  return std::sqrt(dx * dx + dy * dy);
}

float yaw_to(sh::Vec3f from, sh::Vec3f to) {
  return static_cast<float>(libm::atan2(to.y - from.y, to.x - from.x) * kDeg);
}

float pitch_to(sh::Vec3f from, sh::Vec3f to) {
  const double flat = flat_dist(from, to);
  return static_cast<float>(-libm::atan2(to.z - from.z, flat) * kDeg);  // down is positive
}

float wrap(float a) {
  while (a > 180) a -= 360;
  while (a < -180) a += 360;
  return a;
}

const server::WorldEntity& ent_of(const server::RulesCommon& s, int e) {
  return s.world.entities[static_cast<size_t>(e)];
}

// Each model's kind of monster: 0 none, 1 one any weapon kills, 2 one only a blast keeps down,
// 3 one no weapon kills.
void monster_models(const server::RulesCommon& s, std::vector<uint8_t>& out) {
  out.assign(s.models.size(), 0);
  for (size_t m = 0; m < s.models.size(); m++) {
    for (std::string_view name : profiles::kClassicMonsters)
      if (s.models[m] == name) out[m] = 1;
    for (std::string_view name : profiles::kClassicBlastOnly)
      if (s.models[m] == name) out[m] = 2;
    for (std::string_view name : profiles::kClassicNoWeapon)
      if (s.models[m] == name) out[m] = 3;
  }
}

// The kinds of monster player `ent` fights with what he holds: a blast's (the grenade or rocket
// launcher, with its ammunition) puts down the second kind too.
int fights_up_to(const server::RulesCommon& s, int ent) {
  const Stats st = read_stats(s, ent);
  return (st.weapon & (kWGrenades | kWRockets)) != 0 && st.rockets > 0 ? 2 : 1;
}

bool alive_player(const server::RulesCommon& s, int slot) {
  return slot >= 0 && slot < s.sv.set.maxclients && s.clients[slot].active &&
         ent_of(s, slot + 1).health > 0;
}

// Whether `who` faces `at`: its yaw within kEscortFacing of the line to it.
bool facing(const server::WorldEntity& who, sh::Vec3f at) {
  const float off = wrap(yaw_to(who.origin, at) - who.angles.y);
  return off < kEscortFacing && off > -kEscortFacing;
}

void set_mode(Escort& e, EscortMode next, ai::Why* why, const char* because) {
  if (e.mode == next) return;
  if (why) {
    why->decide("escort");
    why->choose(escort_mode_name(next), because);
  }
  e.mode = next;
}

void set_moves(Fighter& f, bool hop, bool strafe) {
  const bool expert_moves = sh::tier_of(f.traits[sh::Trait::moves]) != sh::AimSkill::novice;
  f.bot.hop = hop && expert_moves;
  f.bot.strafe = strafe;
  if (!f.bot.aiming) {
    f.bot.pilot.pilot.set.hop = f.bot.hop;
    f.bot.pilot.pilot.set.strafe = strafe;
  }
}

// The node for the spot at the leader's shoulder: behind his view and to the companion's side,
// among the nodes it can reach that stand kEscortNear-ish from him.
int32_t shoulder_node(const Fighter& f, sh::Vec3f leader, float leader_yaw, int side) {
  const sh::NavGraph& g = *f.bot.pilot.pilot.g;
  const double a = leader_yaw / kDeg;
  const float cx = static_cast<float>(libm::cos(a)), cy = static_cast<float>(libm::sin(a));
  const sh::Vec3f want{
      leader.x - cx * kEscortNear * 0.8f - cy * kEscortNear * 0.6f * static_cast<float>(side),
      leader.y - cy * kEscortNear * 0.8f + cx * kEscortNear * 0.6f * static_cast<float>(side),
      leader.z};
  int32_t best = -1;
  float best_d = 0;
  for (size_t i = 0; i < g.nodes.size(); i++) {
    const sh::Vec3f o = g.nodes[i].origin;
    const float dl = dist(o, leader);
    if (dl < 40 || dl > kEscortFar || std::fabs(o.z - leader.z) > 64) continue;
    if (f.reach_at >= 0 && !f.reach.reached(static_cast<int32_t>(i))) continue;
    const float d = dist(o, want);
    if (best < 0 || d < best_d) {
      best = static_cast<int32_t>(i);
      best_d = d;
    }
  }
  return best >= 0 ? best : sh::nav_nearest(g, leader);
}

// A node kHopOut from the leader in a direction drawn, reachable: a bored hop's turn point.
int32_t hop_node(Fighter& f, sh::Vec3f leader) {
  const sh::NavGraph& g = *f.bot.pilot.pilot.g;
  const double a = ge::core::range(f.rng, -ge::core::kPi, ge::core::kPi);
  const float r = static_cast<float>(ge::core::range(f.rng, kHopOut[0], kHopOut[1]));
  const sh::Vec3f want{leader.x + static_cast<float>(libm::cos(a)) * r,
                       leader.y + static_cast<float>(libm::sin(a)) * r, leader.z};
  int32_t best = -1;
  float best_d = 0;
  for (size_t i = 0; i < g.nodes.size(); i++) {
    const sh::Vec3f o = g.nodes[i].origin;
    if (std::fabs(o.z - leader.z) > 32) continue;
    if (f.reach_at >= 0 && !f.reach.reached(static_cast<int32_t>(i))) continue;
    const float dl = dist(o, leader);
    if (dl < kHopOut[0] * 0.75f || dl > kHopOut[1] * 1.2f) continue;
    const float d = dist(o, want);
    if (best < 0 || d < best_d) {
      best = static_cast<int32_t>(i);
      best_d = d;
    }
  }
  return best;
}

// A goal node off every exit: one in an exit's box is that exit's stand-by node instead (-1 when
// the exit has none: no goal, it stays where it is).
int32_t off_exits(const Fighter& f, int32_t n) {
  const Escort& e = f.escort;
  if (n < 0 || static_cast<size_t>(n) >= e.exit_of.size() || e.exit_of[static_cast<size_t>(n)] < 0)
    return n;
  return f.exits[static_cast<size_t>(e.exit_of[static_cast<size_t>(n)])].stand;
}

// The exit nearest the leader within kEscortExitShow with a stand-by node, or -1.
int exit_to_show(const Fighter& f, sh::Vec3f leader) {
  int best = -1;
  float best_d = kEscortExitShow;
  for (size_t k = 0; k < f.exits.size(); k++) {
    const ExitSpot& x = f.exits[k];
    if (x.stand < 0) continue;
    const float d = dist((x.lo + x.hi) * 0.5f, leader);
    if (d < best_d) {
      best_d = d;
      best = static_cast<int>(k);
    }
  }
  return best;
}

// A step that would carry the companion into an exit's box (where it is a moment on, its box
// grown by a margin): turned back, away from the box's middle, the jump let go.
void keep_off_exits(Fighter& f, const server::RulesCommon& s, int me, sh::MoveCmd& cmd) {
  Escort& e = f.escort;
  if (f.exits.empty()) return;
  const server::WorldEntity& self = ent_of(s, me);
  const double yaw = cmd.angles.y / kDeg;
  const float fx = static_cast<float>(libm::cos(yaw)), fy = static_cast<float>(libm::sin(yaw));
  const float wx = fx * cmd.forward + fy * cmd.side, wy = fy * cmd.forward - fx * cmd.side;
  const float wl = std::sqrt(wx * wx + wy * wy);
  const sh::Vec3f v = self.velocity;
  sh::Vec3f ahead = self.origin + v * 0.15f;
  if (wl > 1) {
    ahead.x += wx / wl * 24;
    ahead.y += wy / wl * 24;
  }
  constexpr float m = 12;  // the margin round the player's box
  for (const ExitSpot& x : f.exits) {
    const auto touches = [&](sh::Vec3f o) {
      return o.x + 16 + m >= x.lo.x && o.x - 16 - m <= x.hi.x && o.y + 16 + m >= x.lo.y &&
             o.y - 16 - m <= x.hi.y && o.z + 32 + m >= x.lo.z && o.z - 24 - m <= x.hi.z;
    };
    if (!touches(ahead) && !touches(self.origin)) continue;
    const sh::Vec3f mid = (x.lo + x.hi) * 0.5f;
    const double away = libm::atan2(self.origin.y - mid.y, self.origin.x - mid.x) - yaw;
    cmd.forward = sh::quantize_axis(static_cast<float>(libm::cos(away) * 320));
    cmd.side = sh::quantize_axis(static_cast<float>(-libm::sin(away) * 320));
    cmd.buttons = static_cast<uint8_t>(cmd.buttons & ~sh::kButtonJump);
    e.stats.exit_turned++;
    return;
  }
}

bool moved_off(sh::Vec3f now, sh::Vec3f rest) { return dist(now, rest) > 0.5f; }

// Whether the button spot `k` is still to press: at rest, not done, and a door it opens shut (a
// button with no door is pressed for whatever it does).
bool button_open(const Fighter& f, const server::RulesCommon& s, size_t k) {
  const Escort& e = f.escort;
  if (k >= e.button_done.size() || e.button_done[k]) return false;
  const ButtonSpot& b = f.button_spots[k];
  if (b.node < 0 || moved_off(ent_of(s, b.ent).origin, e.button_rest[k])) return false;
  if (f.reach_at >= 0 && !f.reach.reached(b.node)) return false;
  if (b.doors.empty()) return true;
  for (int d : b.doors)
    if (static_cast<size_t>(d) < e.rest.size() && !moved_off(ent_of(s, d).origin, e.rest[d]))
      return true;
  return false;
}

// An item in sight within kLookSight worth a bored look: the nearest, else none.
bool item_to_look_at(const Fighter& f, const server::RulesCommon& s, int me, sh::Vec3f& at) {
  const sh::Vec3f self = ent_of(s, me).origin;
  const sh::Vec3f eye{self.x, self.y, self.z + 22};
  float best = kLookSight;
  bool found = false;
  for (const ItemSpot& it : f.items) {
    if (!it.present) continue;
    const float d = dist(it.at, eye);
    if (d >= best || d < 48) continue;
    const sh::HullTrace tr = sh::trace_move(s.sv, eye, {}, {}, {it.at.x, it.at.y, it.at.z + 8},
                                            sh::kTraceNoMonsters, me);
    if (tr.fraction < 1) continue;
    best = d;
    at = {it.at.x, it.at.y, it.at.z + 8};
    found = true;
  }
  return found;
}

// In his way (ahead of him as he walks, near his line): a step aside, away from his line, the
// axes under the view `e.view_yaw`.
void give_way(Escort& e, sh::Vec3f self, sh::MoveCmd& cmd) {
  const sh::Vec3f v = e.leader_vel;
  const float speed = std::sqrt(v.x * v.x + v.y * v.y);
  if (speed < kMoving * 1.5f) return;
  const float ux = v.x / speed, uy = v.y / speed;
  const float rx = self.x - e.leader_at.x, ry = self.y - e.leader_at.y;
  const float along = rx * ux + ry * uy;
  float px = rx - ux * along, py = ry - uy * along;
  const float off = std::sqrt(px * px + py * py);
  if (along <= 0 || along > kAhead || off > kInHisWay) return;
  if (off < 1) {  // square in his way: to its own side of him
    px = -uy * static_cast<float>(e.side);
    py = ux * static_cast<float>(e.side);
  } else {
    px /= off;
    py /= off;
  }
  const double rel = libm::atan2(py, px) - e.view_yaw / kDeg;
  cmd.forward = sh::quantize_axis(static_cast<float>(libm::cos(rel) * 320));
  cmd.side = sh::quantize_axis(static_cast<float>(-libm::sin(rel) * 320));
  e.stats.gave_way++;
}

}  // namespace

const char* escort_mode_name(EscortMode m) {
  switch (m) {
    case EscortMode::follow:
      return "follow";
    case EscortMode::defend:
      return "defend";
    case EscortMode::button:
      return "button";
    case EscortMode::catch_up:
      return "catch-up";
    case EscortMode::bored:
      return "bored";
    case EscortMode::signal:
      return "signal";
  }
  return "?";
}

void escort_init(Fighter& f, const server::RulesCommon& s, int slot, int leader) {
  Escort& e = f.escort;
  e = Escort{};
  e.leader = leader;
  e.side = (slot & 1) != 0 ? 1 : -1;
  e.weights = sh::idle_weights(f.traits);
  // A patient player waits longer before he fidgets: 2.5 s for the most restless, 7 s the calmest.
  e.bored_after_s = 2.0 + 0.5 * f.traits[sh::Trait::discipline];
  f.bot.senses.friends = 0xffffffffu;  // every player is on its side
  const size_t n = s.sv.edicts.size();
  f.bot.senses.foes.assign(n, 0);
  e.stirred_at.assign(n, -1e9);
  e.last_at.assign(n, {});
  e.last_yaw.assign(n, 0);
  e.rest.assign(n, {});
  for (size_t i = 0; i < n && i < static_cast<size_t>(s.sv.num_edicts); i++) {
    e.rest[i] = e.last_at[i] = ent_of(s, static_cast<int>(i)).origin;
    e.last_yaw[i] = ent_of(s, static_cast<int>(i)).angles.y;
  }
  monster_models(s, e.monster_model);
  e.button_rest.clear();
  e.button_done.clear();
  for (const ButtonSpot& b : f.button_spots) {
    e.button_rest.push_back(ent_of(s, b.ent).origin);
    e.button_done.push_back(0);
  }
  // The exits are the leader's: their nodes closed to its routes and its goals.
  const sh::NavGraph& g = *f.bot.pilot.pilot.g;
  e.exit_of.assign(g.nodes.size(), -1);
  for (size_t k = 0; k < f.exits.size(); k++)
    for (const int32_t in : f.exits[k].inside)
      e.exit_of[static_cast<size_t>(in)] = static_cast<int8_t>(k);
  std::vector<sh::NavExtraLink>& shut = f.bot.pilot.pilot.shut_links;
  shut.clear();
  for (size_t a = 0; a < g.nodes.size(); a++) {
    const sh::NavNode& node = g.nodes[a];
    for (int32_t k = 0; k < node.links; k++) {
      const sh::NavLink& l = g.links[static_cast<size_t>(node.first_link) + static_cast<size_t>(k)];
      if (e.exit_of[static_cast<size_t>(l.to)] >= 0)
        shut.push_back({static_cast<int32_t>(a), l.to, l.kind, l.time_s});
    }
  }
  const server::WorldEntity& me = ent_of(s, slot + 1);
  e.view_yaw = me.view_angles.y;
  e.leader_at = e.still_at = ent_of(s, leader + 1).origin;
  e.still_since = s.sv.time;
  f.bot.pilot.pilot.set.hop_min = 160;  // a bored hop round the leader is a short one
  set_moves(f, false, false);
}

void hunt_init(Fighter& f, const server::RulesCommon& s) {
  f.escort = Escort{};
  f.hunts = true;
  set_moves(f, false, false);  // a first time through is walked: no hop off a ledge
  f.bot.senses.friends = 0xffffffffu;
  f.bot.senses.foes.assign(s.sv.edicts.size(), 0);
  monster_models(s, f.escort.monster_model);
}

void hunt_sense(Fighter& f, const server::RulesCommon& s) {
  std::vector<uint8_t>& foes = f.bot.senses.foes;
  if (foes.size() < static_cast<size_t>(s.sv.num_edicts))
    foes.resize(static_cast<size_t>(s.sv.num_edicts), 0);
  const std::vector<uint8_t>& mm = f.escort.monster_model;
  const int top = fights_up_to(s, f.slot + 1);
  for (int ent = s.sv.set.maxclients + 1; ent < s.sv.num_edicts; ent++) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(ent)];
    const int kind = !d.free && d.solid != sh::kSolidNot && d.modelindex > 0 &&
                             static_cast<size_t>(d.modelindex) < mm.size()
                         ? mm[static_cast<size_t>(d.modelindex)]
                         : 0;
    foes[static_cast<size_t>(ent)] = kind != 0 && kind <= top ? 1 : 0;
  }
}

void escort_sense(Fighter& f, const server::RulesCommon& s, int me, double t) {
  Escort& e = f.escort;
  const server::WorldEntity& leader = ent_of(s, e.leader + 1);
  const int top = fights_up_to(s, me);
  // His velocity off his places a command apart (a client sees where he is, frame by frame); any
  // move off the spot ends his standing still.
  if (e.last_t >= 0 && t > e.last_t) {
    const float inv = static_cast<float>(1.0 / (t - e.last_t));
    e.leader_vel = (leader.origin - e.leader_at) * inv;
  }
  e.leader_at = leader.origin;
  const float speed2 = e.leader_vel.x * e.leader_vel.x + e.leader_vel.y * e.leader_vel.y;
  if (speed2 > kMoving * kMoving || dist(e.leader_at, e.still_at) > kStill) {
    e.still_at = e.leader_at;
    e.still_since = t;
  }
  const sh::Vec3f self = ent_of(s, me).origin;
  std::vector<uint8_t>& foes = f.bot.senses.foes;
  if (foes.size() < static_cast<size_t>(s.sv.num_edicts)) {
    foes.resize(static_cast<size_t>(s.sv.num_edicts), 0);
    e.stirred_at.resize(foes.size(), -1e9);
    e.last_at.resize(foes.size(), {});
    e.last_yaw.resize(foes.size(), 0);
  }
  // What stirred: a monster's sound this frame.
  for (int i = 0; i < s.sv.sound_count; i++) {
    const int ent = s.sv.sounds[i].ent;
    if (ent > 0 && static_cast<size_t>(ent) < e.stirred_at.size()) e.stirred_at[ent] = t;
  }
  e.threat = -1;
  float nearest = 1e9f;
  const bool leader_alive = alive_player(s, e.leader);
  for (int ent = s.sv.set.maxclients + 1; ent < s.sv.num_edicts; ent++) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(ent)];
    const size_t k = static_cast<size_t>(ent);
    const bool monster = !d.free && d.modelindex > 0 &&
                         static_cast<size_t>(d.modelindex) < e.monster_model.size() &&
                         e.monster_model[static_cast<size_t>(d.modelindex)] != 0 &&
                         e.monster_model[static_cast<size_t>(d.modelindex)] <= top;
    // One lying dead (its body no longer solid) is no monster any more.
    if (!monster || d.solid == sh::kSolidNot) {
      foes[k] = 0;
      continue;
    }
    // Moved or turned since the last command: awake.
    const float turned = wrap(d.angles.y - e.last_yaw[k]);
    if (moved_off(d.origin, e.last_at[k]) || turned > 0.5f || turned < -0.5f) e.stirred_at[k] = t;
    e.last_at[k] = d.origin;
    e.last_yaw[k] = d.angles.y;
    const bool awake = t - e.stirred_at[k] <= kEscortStir_s;
    const server::WorldEntity& w = ent_of(s, ent);
    const float dl = dist(d.origin, e.leader_at), ds = dist(d.origin, self);
    const bool threat = awake && ((leader_alive && dl < kEscortGuard) || ds < kEscortSelf ||
                                  (leader_alive && dl < kEscortSight && facing(w, e.leader_at)) ||
                                  (ds < kEscortSight && facing(w, self)));
    if (threat && foes[k] == 0) e.stats.threats++;
    foes[k] = threat ? 1 : 0;
    if (threat && dl < nearest) {
      nearest = dl;
      e.threat = ent;
    }
  }
}

void escort_think(Fighter& f, const server::RulesCommon& s, int me, const Stats& st, double t,
                  ai::Why* why) {
  (void)st;
  Escort& e = f.escort;
  const sh::Vec3f self = ent_of(s, me).origin;
  const sh::Vec3f eye{self.x, self.y, self.z + f.bot.senses.set.view_height};
  const sh::Vec3f L = e.leader_at;
  const float leader_yaw = ent_of(s, e.leader + 1).view_angles.y;
  const bool leader_still = t - e.still_since >= 0.5;
  const float d = dist(self, L);

  const sh::Contact* c = sh::freshest(f.bot.senses, eye);
  const bool fight = c && (c->visible || (ai::known(c->seen) && ai::age(c->seen, t) < 1.0));

  // The button the leader needs: one he has stood near a while, still to press.
  int candidate = -1;
  float cand_d = kEscortButtonNear;
  for (size_t k = 0; k < f.button_spots.size(); k++) {
    const float bd = dist(f.button_spots[k].at, L);
    if (bd < cand_d && button_open(f, s, k)) {
      cand_d = bd;
      candidate = static_cast<int>(k);
    }
  }
  if (e.mode == EscortMode::button && e.button >= 0 &&
      !button_open(f, s, static_cast<size_t>(e.button))) {
    if (moved_off(ent_of(s, f.button_spots[static_cast<size_t>(e.button)].ent).origin,
                  e.button_rest[static_cast<size_t>(e.button)]))
      e.stats.buttons++;
    e.button_done[static_cast<size_t>(e.button)] = 1;
    e.button = -1;
  }
  if (e.mode == EscortMode::button && e.button >= 0 && t - e.button_since > kEscortButtonGive_s) {
    e.button_done[static_cast<size_t>(e.button)] = 1;
    e.stats.buttons_given_up++;
    e.button = -1;
  }
  if (candidate < 0 || !leader_still) {  // he walks past it: not yet his
    e.near_button_since = -1;
  } else if (e.near_button_since < 0) {
    e.near_button_since = t;
  }
  const bool press = e.button >= 0 || (candidate >= 0 && e.near_button_since >= 0 &&
                                       t - e.near_button_since >= kEscortButtonWait_s);
  // The exit to show, once the leader has stood a moment (or it is shown already).
  const int32_t show = e.mode == EscortMode::signal || t - e.still_since >= kEscortSignalAfter_s
                           ? exit_to_show(f, L)
                           : -1;

  EscortMode next;
  const char* because;
  if (fight) {
    next = EscortMode::defend;
    because = "a monster that came for us in sight";
  } else if (press) {
    next = EscortMode::button;
    because = "a button by the leader still to press";
  } else if (d > kEscortCatchUp) {
    next = EscortMode::catch_up;
    because = "the leader far ahead";
  } else if (show >= 0) {
    e.exit = show;
    next = EscortMode::signal;
    because = "the leader near the exit: showing it";
  } else if (leader_still && t - e.still_since >= e.bored_after_s && d < kEscortFar * 1.5f) {
    next = EscortMode::bored;
    because = "the leader standing still with nothing about";
  } else {
    next = EscortMode::follow;
    because = "at the leader's shoulder";
  }
  // Boredom ends a human moment after its reason does (sooner for a fight), at once when it is out
  // on a hop past its shoulder distance and the leader walks off: the moment would cost the hop's
  // length again and leave it past its catch-up.
  if (e.mode == EscortMode::bored && next != EscortMode::bored) {
    if (e.break_at < 0)
      e.break_at =
          t + (fight ? ge::core::range(f.rng, 0.1, 0.3) : ge::core::range(f.rng, 0.3, 0.8));
    const bool left = !leader_still && d > kEscortFar;
    if (t < e.break_at && !left) {
      next = EscortMode::bored;
      because = "taking it in";
    } else {
      e.break_at = -1;
      e.stats.idle_breaks++;
      sh::idle_end(e.idle, f.rng, t);
      e.hop_node = -1;
    }
  } else if (next == EscortMode::bored) {
    e.break_at = -1;
  }
  set_mode(e, next, why, because);
  f.mode = fight ? FightMode::fight : FightMode::roam;
  // The doors only something else opens, while they rest shut: no route of its own goes through
  // one (it would wait at it for ever); the leader opens them, or the button it presses does.
  std::vector<int32_t>& shut = f.bot.pilot.pilot.shut;
  shut.clear();
  for (size_t k = 0; k < f.locked.size(); k++)
    if (dist(ent_of(s, f.locked[k]).origin, f.locked_rest[k]) < 1) shut.push_back(f.locked[k]);

  switch (e.mode) {
    case EscortMode::defend:
      set_moves(f, false, false);
      if (d > kEscortLeash) {
        const int32_t n = off_exits(f, shoulder_node(f, L, leader_yaw, e.side));
        if (n >= 0) e.follow_node = n;
        if (e.follow_node >= 0) fighter_go(f, e.follow_node);
      }
      break;
    case EscortMode::button: {
      set_moves(f, false, false);
      if (e.button < 0) {
        e.button = candidate;
        e.button_since = t;
      }
      fighter_go(f, f.button_spots[static_cast<size_t>(e.button)].node);
      break;
    }
    case EscortMode::catch_up: {
      // Hopped: walking is his own speed, so a gap once opened never closes (the pilot's hop
      // keeps to links it can land, `plugins/shooter/pilot.hpp`).
      set_moves(f, true, false);
      // The spot at his shoulder among the nodes it can reach (the node he stands on may be one
      // it cannot: a ledge, the water), kept until he has gone kRetarget beyond it: a route
      // planned again at every think starts its run-ups and its turns over.
      const sh::NavGraph& g = *f.bot.pilot.pilot.g;
      const int32_t n = off_exits(f, shoulder_node(f, L, leader_yaw, e.side));
      if (n >= 0 && (e.follow_node < 0 || f.goal_node != e.follow_node ||
                     dist(g.nodes[static_cast<size_t>(n)].origin,
                          g.nodes[static_cast<size_t>(e.follow_node)].origin) > kRetarget)) {
        e.follow_node = n;
        fighter_go(f, e.follow_node);
      }
      break;
    }
    case EscortMode::signal: {
      set_moves(f, false, false);
      const int32_t n = f.exits[static_cast<size_t>(e.exit)].stand;
      if (f.goal_node != n) fighter_go(f, n);
      break;
    }
    case EscortMode::follow: {
      set_moves(f, d > kEscortFar * 2, false);
      const sh::NavGraph& g = *f.bot.pilot.pilot.g;
      const bool far = d > kEscortFar;
      const bool stale = e.follow_node < 0 ||
                         dist(g.nodes[static_cast<size_t>(e.follow_node)].origin, L) > kEscortFar;
      if (far || stale || (!leader_still && d > kEscortNear * 1.4f)) {
        const int32_t n = off_exits(f, shoulder_node(f, L, leader_yaw, e.side));
        if (n >= 0 && (e.follow_node < 0 ||
                       dist(g.nodes[static_cast<size_t>(n)].origin,
                            g.nodes[static_cast<size_t>(e.follow_node)].origin) > kRetarget ||
                       far))
          e.follow_node = n;
        fighter_go(f, e.follow_node);
      }
      break;
    }
    case EscortMode::bored: {
      sh::IdleState& is = e.idle;
      if (is.act != sh::IdleAct::none && t >= is.until) {
        sh::idle_end(is, f.rng, t);
        e.hop_node = -1;
        if (e.follow_node >= 0) fighter_go(f, e.follow_node);  // back to his side
      }
      if (is.act == sh::IdleAct::none && t >= is.next_at && e.break_at < 0) {
        sh::Vec3f at;
        const bool look = item_to_look_at(f, s, me, at);
        const sh::IdleAct act = sh::idle_begin(is, e.weights, f.rng, t, look);
        if (look) e.look_at = at;
        if (why) {
          why->decide("idle");
          why->choose(sh::idle_act_name(act), "bored, waiting on the leader");
        }
        if (act == sh::IdleAct::hop) {
          e.hop_node = hop_node(f, L);
          if (e.hop_node >= 0 && e.exit_of[static_cast<size_t>(e.hop_node)] >= 0) e.hop_node = -1;
          if (e.hop_node < 0) {
            is.until = t;  // nowhere to hop: over at once
          } else {
            set_moves(f, true, false);
            fighter_go(f, e.hop_node);
          }
        }
      }
      if (is.act == sh::IdleAct::hop && e.hop_node >= 0 &&
          f.bot.pilot.pilot.mode == sh::PilotMode::arrived && f.goal_node == e.hop_node) {
        e.hop_node = -1;  // out there: hop back to his side
        if (e.follow_node >= 0) fighter_go(f, e.follow_node);
      }
      if (is.act != sh::IdleAct::hop) set_moves(f, false, false);
      break;
    }
  }
}

void escort_command(Fighter& f, const server::RulesCommon& s, int me, double t, int msec,
                    sh::MoveCmd& cmd) {
  Escort& e = f.escort;
  const double dt = msec * 0.001;
  e.stats.commands[static_cast<int>(e.mode)]++;
  e.last_t = t;
  const sh::Vec3f self = ent_of(s, me).origin;
  const sh::Vec3f eye{self.x, self.y, self.z + 22};
  const sh::Vec3f leader_eye{e.leader_at.x, e.leader_at.y, e.leader_at.z + 22};
  if (f.bot.aiming) {  // the aim's own hands have the view
    e.view_yaw = cmd.angles.y;
    e.view_pitch = cmd.angles.x;
    give_way(e, self, cmd);
    keep_off_exits(f, s, me, cmd);
    return;
  }

  bool own_view = false;  // the view below is the companion's, not the pilot's
  float want_yaw = cmd.angles.y, want_pitch = cmd.angles.x;
  float rate = kTurnRate;

  if (e.mode == EscortMode::button && e.button >= 0) {
    const ButtonSpot& b = f.button_spots[static_cast<size_t>(e.button)];
    const float reach = b.shoot ? kShootReach : kPressReach * 2;
    const float bd = flat_dist(b.at, eye);
    const sh::HullTrace tr = sh::trace_move(s.sv, eye, {}, {}, b.at, sh::kTraceNoMonsters, me);
    const bool sees = tr.fraction >= 1 || tr.ent == b.ent;
    if (sees && bd < reach &&
        (b.shoot || f.bot.pilot.pilot.mode == sh::PilotMode::arrived || bd < kPressReach)) {
      own_view = true;
      want_yaw = yaw_to(eye, b.at);
      want_pitch = pitch_to(eye, b.at);
      const float off = wrap(want_yaw - e.view_yaw);
      if (b.shoot) {
        cmd.forward = cmd.side = 0;
        if (off < 4 && off > -4 && f.stats.commands % 8 == 0)
          cmd.buttons = static_cast<uint8_t>(cmd.buttons | sh::kButtonAttack);
      } else {
        cmd.forward = sh::quantize_axis(off < 30 && off > -30 ? 320.f : 0.f);  // into it
        cmd.side = 0;
      }
    }
  } else if (e.mode == EscortMode::bored && e.idle.act != sh::IdleAct::none &&
             e.idle.act != sh::IdleAct::hop) {
    own_view = true;
    cmd.forward = cmd.side = 0;
    cmd.buttons = static_cast<uint8_t>(cmd.buttons & ~sh::kButtonJump);
    switch (e.idle.act) {
      case sh::IdleAct::spin: {
        const float step = static_cast<float>(540 * dt);
        const float turn =
            e.idle.turn > 0 ? std::fmin(step, e.idle.turn) : std::fmax(-step, e.idle.turn);
        e.idle.turn -= turn;
        want_yaw = e.view_yaw + turn;
        want_pitch = 0;
        rate = 1e6f;  // the spin is its own rate
        if (e.idle.turn == 0 && e.idle.until > t + 0.2) e.idle.until = t + 0.2;
        break;
      }
      case sh::IdleAct::look:
        want_yaw = yaw_to(eye, e.look_at);
        want_pitch = pitch_to(eye, e.look_at);
        rate = kLookRate;
        break;
      case sh::IdleAct::glance: {
        want_yaw = yaw_to(eye, leader_eye);
        // A nod: the head dipped through the act's middle third.
        const double left = e.idle.until - t;
        want_pitch = left > 0.4 && left < 0.9 ? 18.f : pitch_to(eye, leader_eye);
        rate = kLookRate;
        break;
      }
      case sh::IdleAct::jump:
        want_yaw = e.view_yaw;
        want_pitch = 0;
        if (t >= e.idle.flip_at) {
          cmd.buttons = static_cast<uint8_t>(cmd.buttons | sh::kButtonJump);
          e.idle.flip_at = t + ge::core::range(f.rng, 0.55, 0.95);
        }
        break;
      case sh::IdleAct::sway:
        want_yaw = yaw_to(eye, leader_eye);
        want_pitch = 0;
        rate = kLookRate;
        if (t >= e.idle.flip_at) {
          e.idle.turn = -e.idle.turn;
          e.idle.flip_at = t + ge::core::range(f.rng, 0.3, 0.6);
        }
        cmd.side = sh::quantize_axis(220.f * e.idle.turn);
        break;
      case sh::IdleAct::none:
      case sh::IdleAct::hop:
        break;
    }
  } else if (e.mode == EscortMode::signal && e.exit >= 0 &&
             (f.bot.pilot.pilot.mode == sh::PilotMode::arrived ||
              flat_dist(self,
                        f.bot.pilot.pilot.g
                            ->nodes[static_cast<size_t>(f.exits[static_cast<size_t>(e.exit)].stand)]
                            .origin) < 40)) {
    // Beside the exit: facing him, and a jump now and then, "here".
    own_view = true;
    cmd.forward = cmd.side = 0;
    want_yaw = yaw_to(eye, leader_eye);
    want_pitch = pitch_to(eye, leader_eye);
    rate = kLookRate;
    cmd.buttons = static_cast<uint8_t>(cmd.buttons & ~sh::kButtonJump);
    if (t >= e.signal_at) {
      cmd.buttons = static_cast<uint8_t>(cmd.buttons | sh::kButtonJump);
      e.signal_at = t + ge::core::range(f.rng, kEscortSignalEvery_s[0], kEscortSignalEvery_s[1]);
      e.stats.signal_jumps++;
    }
  } else if (e.mode == EscortMode::follow && f.bot.pilot.pilot.mode == sh::PilotMode::arrived) {
    // Standing at his shoulder: the view drifts the way he looks, as a partner keeps watch.
    own_view = true;
    want_yaw = ent_of(s, e.leader + 1).view_angles.y;
    want_pitch = 0;
    rate = kLookRate * 0.6f;
  }

  const bool hopping = f.bot.pilot.pilot.mode == sh::PilotMode::hop ||
                       f.bot.pilot.pilot.mode == sh::PilotMode::strafe;
  if (!own_view && hopping) {  // the hop's hands keep the view on the velocity
    e.view_yaw = cmd.angles.y;
    e.view_pitch = cmd.angles.x;
    keep_off_exits(f, s, me, cmd);
    return;
  }
  const float pilot_yaw = cmd.angles.y;
  const float yaw = sh::idle_turn(e.view_yaw, e.view_yaw + wrap(want_yaw - e.view_yaw), rate, dt);
  const float pitch = sh::idle_turn(e.view_pitch, want_pitch, rate * 0.5f, dt);
  if (!own_view) sh::turn_axes(cmd.forward, cmd.side, pilot_yaw, yaw);  // the pilot's wish kept
  e.view_yaw = wrap(yaw);
  e.view_pitch = pitch;
  give_way(e, self, cmd);
  cmd.angles = {sh::quantize_angle(e.view_pitch), sh::quantize_angle(e.view_yaw), 0};
  keep_off_exits(f, s, me, cmd);
  f.bot.yaw = e.view_yaw;  // an aim taken up starts from where the view is
  f.bot.pitch = e.view_pitch;
}

bool escort_blast_clear(const Fighter& f, const server::RulesCommon& s, sh::Vec3f at) {
  for (int k = 0; k < s.sv.set.maxclients && k < sh::kMaxClients; k++) {
    if (!alive_player(s, k)) continue;
    if (dist(ent_of(s, k + 1).origin, at) < kEscortBlastClear) return false;
  }
  (void)f;
  return true;
}

bool escort_player_in_line(const Fighter& f, const server::RulesCommon& s, sh::Vec3f eye,
                           sh::Vec3f target, float clear) {
  const sh::Vec3f line = target - eye;
  const float len2 = line.x * line.x + line.y * line.y + line.z * line.z;
  if (len2 < 1) return false;
  for (int k = 0; k < s.sv.set.maxclients && k < sh::kMaxClients; k++) {
    if (k == f.slot || !alive_player(s, k)) continue;
    const sh::Vec3f p = ent_of(s, k + 1).origin;
    const sh::Vec3f to = p - eye;
    const float u = (to.x * line.x + to.y * line.y + to.z * line.z) / len2;
    if (u <= 0 || u >= 1) continue;
    const sh::Vec3f off = to - line * u;
    if (off.x * off.x + off.y * off.y + off.z * off.z < clear * clear) return true;
  }
  return false;
}

}  // namespace shooters_edge::bots
