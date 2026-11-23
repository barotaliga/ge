// Ground Zero's changes to the AI every monster shares (q2_monster_ai.hpp § Ground Zero's shared
// monster AI; the facts are porting/shooters-edge/gz-monsters.md § 1), each only under the pack's
// switch: the dodge that ducks a high shot and side-steps a low one, by skill; strafing on foot
// as well as in the air; blind fire at where the enemy was last seen, rarer with each try; jumps
// down and up ledges toward the enemy; the medic's search and its patience; and the monsters
// whose shots start no fight among them.
//
// ge:summary Ground Zero's shared monster AI: dodge, side-step, strafing, blind fire, ledge jumps
// ge:keywords q2 ground-zero rogue monsters dodge duck side-step strafe slide blind-fire
// ge:keywords ledge-jump jump-down jump-up medic patient bad-medic infighting ignore-shots
// ge:skill shooter
// ge:test tests/shooters_edge_q2_gz_ai_test.cpp
#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include "brush/map.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_lines.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_monster_ai.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server::monster_ai {

namespace {

constexpr float kWalkerSlide = 8;  // a walker's sideways step a frame at most ("less jerky")

const GzDodge* dodge_row(const Q2Monster& m) {
  // The medic commander dodges as the medic does (its body is the medic's).
  return gz_dodge_of(looks_like(kind(m)));
}

// Whether `foe` is a tesla mine (never strafed round).
bool tesla(const Q2Server& s, int foe) {
  if (foe >= -1) return false;
  const int e = foe_entity(foe);
  return e >= 0 && size_t(e) < s.entities.size() &&
         s.entities[size_t(e)].classname.find("tesla") != std::string::npos;
}

// Whether trace `t` stopped on a monster other than `m`'s enemy.
bool other_monster(const Q2Server& s, const Q2Monster& m, const ge::classic::Q2Trace& t) {
  if (t.fraction >= 1 || t.entity <= sh::kMaxClients) return false;
  return monster_index(s, t.entity) >= 0 && foe_of_entity(t.entity) != m.enemy;
}

}  // namespace

bool gz_ai(const Q2Server& s) { return s.opt.pack == Q2Pack::ground_zero; }

bool gz_dodges(const Q2Server& s, const Q2Monster& m) {
  return gz_ai(s) && dodge_row(m) != nullptr;
}

GzDodgeAct gz_dodge(Q2Server& s, Q2Monster& m, int shooter, sh::Vec3f hit, double eta) {
  const GzDodge* d = dodge_row(m);
  if (!gz_ai(s) || d == nullptr) return GzDodgeAct::none;
  if (m.enemy == -1) found(s, m, shooter, true);  // as on sight, before anything else
  if (eta < 0.1 || eta > 5) return GzDodgeAct::handled;
  if (random01(s) >= 0.25 * double(skill(s) + 1)) return GzDodgeAct::handled;
  const Q2Entity& e = body(s, m);
  const bool ducker = d->ducks;
  const bool dodger = d->side_steps && !m.hold;
  if (!ducker && !dodger) return GzDodgeAct::handled;
  // A ducker can't duck a shot at or below 33 under its spawned box's top; for a walker that
  // can't duck every shot is low.
  const float top = e.origin.z + kind(m).maxs.z;
  const bool low = hit.z <= (ducker ? top - 33 : top);
  if (ducker && !dodger && (low || m.ducked)) return GzDodgeAct::handled;
  if (dodger && s.level_time + 0.001 < m.dodge_until) return GzDodgeAct::handled;  // finishes it
  if (dodger && (low || m.ducked)) {
    // Away from the side it would be struck on; stood up first.
    const sh::Vec3f left = forward_of(e.angles.y + 90);
    m.slide_left = sh::dot(hit - e.origin, left) <= 0;
    if (m.ducked) {
      m.ducked = false;
      body(s, m).maxs.z += 32;
      m.duck_frame = m.duck_pose = -1;
    }
    m.dodge_until = s.level_time + std::max(eta, 0.3);
    return GzDodgeAct::handled;
  }
  if (s.level_time + 0.001 < m.next_duck) return GzDodgeAct::handled;
  m.dodge_until = 0;
  m.duck_hold = s.level_time + eta + (skill(s) == 0 ? 1.0 : 0.1 * double(3 - skill(s)));
  return GzDodgeAct::duck;
}

bool gz_dodge_step(Q2Server& s, Q2Monster& m) {
  if (!gz_ai(s) || m.dodge_until <= 0) return false;
  if (s.level_time + 0.001 >= m.dodge_until) {
    m.dodge_until = 0;
    return false;
  }
  Q2Entity& e = body(s, m);
  const Kind& k = kind(m);
  if (m.enemy != -1) turn(e, k, yaw_to(foe_at(s, m.enemy) - e.origin));
  const float len = gz_slide_step(s, m, k.run * kStep);
  for (int tries = 0; tries < 2; tries++) {
    if (step(s, m, forward_of(e.angles.y + (m.slide_left ? 90.0f : -90.0f)) * len)) return true;
    m.slide_left = !m.slide_left;
  }
  m.dodge_until = 0;  // blocked both ways: back to the run
  return false;
}

double gz_slide_chance(Q2Server& s, const Q2Monster& m, Range r) {
  if (!gz_ai(s)) return -1;
  if (r == Range::melee || tesla(s, m.enemy)) return 0;
  if (named(m, "monster_daedalus")) return 0.8;
  return kind(m).flies ? 0.6 : 0.4;
}

float gz_slide_step(const Q2Server& s, const Q2Monster& m, float run_step) {
  return gz_ai(s) && !kind(m).flies ? std::min(run_step, kWalkerSlide) : run_step;
}

void gz_seen(Q2Server& s, Q2Monster& m) {
  if (!gz_ai(s) || m.enemy == -1) return;
  m.blind_at = foe_at(s, m.enemy);
  m.blind_delay = 0;
  m.hint_node = m.hint_goal = -1;  // seen: off any hint path, at him
}

bool gz_blind_fire(Q2Server& s, Q2Monster& m) {
  if (!gz_ai(s) || m.enemy == -1) return false;
  const GzBlind* b = gz_blind_of(looks_like(kind(m)));
  if (b == nullptr || m.blind_delay > 20 || sees(s, m, m.enemy)) return false;
  if (s.level_time + 0.001 < m.attack_finished) return false;
  if (s.level_time + 0.001 < m.trail_time + m.blind_delay) return false;  // waits that long
  const Q2Entity& e = body(s, m);
  const sh::Vec3f from = eye(e, kind(m));
  if (other_monster(s, m, map_trace(s, from, {}, {}, foe_eye(s, m.enemy), kShotMask, m.entity)) ||
      other_monster(s, m, map_trace(s, from, {}, {}, m.blind_at, kShotMask, m.entity)))
    return false;
  // The roll by the delay so far, before it grows: a failed one still spends the attack's turn.
  const double chance = m.blind_delay < 1 ? 1.0 : m.blind_delay < 7.5 ? 0.4 : 0.1;
  m.attack_finished = s.level_time + 2 * random01(s);
  if (random01(s) >= chance) return false;
  m.blind_delay += double(b->delay_lo) + random01(s) * double(b->delay_hi - b->delay_lo);
  m.attack_finished =
      s.level_time + double(b->wait_lo) + random01(s) * double(b->wait_hi - b->wait_lo);
  return true;
}

bool gz_ledge_jump(Q2Server& s, Q2Monster& m) {
  if (!gz_ai(s) || m.enemy == -1 || !m.on_ground || m.jump_at > 0) return false;
  const GzLedge* l = gz_ledge(looks_like(kind(m)));
  if (l == nullptr) return false;
  Q2Entity& e = body(s, m);
  sh::Vec3f lo, hi;
  foe_box(s, m.enemy, lo, hi);
  const float foe_bottom = foe_at(s, m.enemy).z + lo.z;
  const float bottom = e.origin.z + e.mins.z;
  const bool above = foe_bottom - bottom > 16;
  const bool below = bottom - foe_bottom > 16;
  if (!above && !below) return false;
  const sh::Vec3f fwd = forward_of(e.angles.y);
  const sh::Vec3f ahead = e.origin + fwd * 48.0f;
  constexpr int32_t kLand = sh::kSqSolid | sh::kSqMaskWater;
  sh::Vec3f launch;
  if (below) {
    if (sweep(s, m, e.origin, ahead).fraction < 1) return false;  // its box can't go 48 ahead
    const ge::classic::Q2Trace t =
        map_trace(s, ahead, {}, {}, {ahead.x, ahead.y, bottom - l->drop - 1}, kLand, m.entity);
    if (t.fraction >= 1 || (t.contents & sh::kSqMaskWater) != 0) return false;
    if (bottom - t.end.z < 24 || t.end.z < foe_bottom - 32 || t.normal.z < 0.9f) return false;
    launch = fwd * l->down_forward + sh::Vec3f{0, 0, l->down_up};
  } else {
    const float top = e.origin.z + e.maxs.z;
    const ge::classic::Q2Trace t = map_trace(s, {ahead.x, ahead.y, top + l->climb}, {}, {},
                                             {ahead.x, ahead.y, e.origin.z}, kLand, m.entity);
    if (t.fraction >= 1 || t.start_solid || t.end.z - bottom > l->climb) return false;
    // Faces the wall it jumps up (a probe 64 ahead).
    const ge::classic::Q2Trace wall =
        map_trace(s, e.origin, {}, {}, e.origin + fwd * 64.0f, sh::kSqSolid, m.entity);
    if (wall.fraction < 1) e.angles.y = yaw_to(sh::Vec3f{-wall.normal.x, -wall.normal.y, 0});
    launch = forward_of(e.angles.y) * l->up_forward + sh::Vec3f{0, 0, l->up_up};
  }
  m.velocity = launch;
  m.on_ground = false;
  m.jump_at = s.level_time;
  return true;
}

bool gz_jumping(Q2Server& s, Q2Monster& m) {
  if (m.jump_at <= 0) return false;
  if ((m.on_ground && s.level_time > m.jump_at + 0.001) || s.level_time > m.jump_at + 3) {
    m.jump_at = 0;
    return false;
  }
  return true;
}

bool gz_ignores_shots(const Q2Server& s, const Q2Monster& m) {
  if (!gz_ai(s)) return false;
  if (m.revived) return true;
  const std::string_view c = kind(m).classname;
  return c == "monster_tank" || c == "monster_tank_commander" || c == "monster_supertank" ||
         c == "monster_medic" || c == "monster_medic_commander" || c == "monster_turret";
}

bool gz_patient_ok(const Q2Server& s, const Q2Monster& medic, const Q2Monster& c) {
  if (!gz_ai(s)) return true;
  if (kind(c).good || c.bad_medic[0] == medic.entity || c.bad_medic[1] == medic.entity)
    return false;
  const sh::Vec3f d = s.entities[size_t(c.entity)].origin - s.entities[size_t(medic.entity)].origin;
  return sh::length(d) >= 32;
}

float gz_patient_reach(const Q2Server& s, const Q2Monster& medic) {
  return gz_ai(s) && medic.hold ? 400.0f : 1024.0f;
}

bool gz_patient_keep(Q2Server& s, Q2Monster& medic, Q2Monster& c) {
  if (!gz_ai(s)) return true;
  const auto give_up = [&] {
    c.bad_medic[1] = c.bad_medic[0];
    c.bad_medic[0] = medic.entity;
  };
  if (s.level_time > medic.patient_since + 10) {  // ten seconds to raise it
    give_up();
    return false;
  }
  if (medic.revive_at > 0) {  // the cable out: a corpse too near is torn apart
    const Q2Entity& e = s.entities[size_t(medic.entity)];
    const sh::Vec3f d = s.entities[size_t(c.entity)].origin - eye(e, kind(medic));
    if (sh::length(d) < 32) {
      gib_monster(s, c, kind(c).gib < 0 ? -kind(c).gib : 500);
      return false;
    }
  }
  return true;
}

// ---- Hint paths (gz-monsters.md § 1.5) ----

namespace {

constexpr std::string_view kHint = "hint_path";
constexpr int kHintEnd = 1;          // spawnflag: either end of a chain
constexpr size_t kHintChains = 100;  // chains at most
constexpr float kHintNear = 512;     // a node's reach from the monster, and from its enemy

bool hint(const Q2Server& s, int e) {
  return e >= 0 && size_t(e) < s.entities.size() && !s.entities[size_t(e)].gone &&
         s.entities[size_t(e)].classname == kHint;
}

// The hint nodes `name` names (two or more: a fork).
int named_hints(const Q2Server& s, const std::string& name, int& first) {
  int n = 0;
  first = -1;
  if (name.empty()) return 0;
  for (size_t i = 0; i < s.entities.size(); i++)
    if (hint(s, int(i)) && s.entities[i].targetname == name && n++ == 0) first = int(i);
  return n;
}

// Every chain, in order from its starting end: each END node with a target and no name starts one,
// followed by targets; a fork or a node already on a chain ends it (a fork's chain is cut after
// its start).
std::vector<std::vector<int>> hint_chains(const Q2Server& s) {
  std::vector<std::vector<int>> chains;
  std::vector<bool> taken(s.entities.size(), false);
  for (size_t i = 0; i < s.entities.size() && chains.size() < kHintChains; i++) {
    const Q2Entity& e = s.entities[i];
    if (!hint(s, int(i)) || (e.spawnflags & kHintEnd) == 0 || e.target.empty() ||
        !e.targetname.empty())
      continue;
    std::vector<int> chain{int(i)};
    taken[i] = true;
    for (int at = int(i);;) {
      int next = -1;
      if (named_hints(s, s.entities[size_t(at)].target, next) != 1 || taken[size_t(next)]) break;
      chain.push_back(next);
      taken[size_t(next)] = true;
      at = next;
    }
    chains.push_back(std::move(chain));
  }
  return chains;
}

// Whether the line from `from` to node `n` is clear of the world.
bool clear_to_node(const Q2Server& s, sh::Vec3f from, int n, int skip) {
  return map_trace(s, from, {}, {}, s.entities[size_t(n)].origin, kOpaque, skip).fraction >= 1;
}

// The chain node `n` is on (empty none).
std::vector<int> chain_of(const Q2Server& s, int n) {
  for (std::vector<int>& c : hint_chains(s))
    if (std::find(c.begin(), c.end(), n) != c.end()) return c;
  return {};
}

// The path chosen toward `m`'s enemy: a node it sees within 512 starting a chain that holds one
// its enemy sees within 512; false when there is none.
bool choose_hint(const Q2Server& s, Q2Monster& m) {
  const Q2Entity& e = s.entities[size_t(m.entity)];
  const sh::Vec3f me = eye(e, kind(m)), him = foe_eye(s, m.enemy);
  int start = -1, goal = -1;
  float best = 0;
  for (const std::vector<int>& c : hint_chains(s)) {
    int end = -1;
    for (const int n : c)  // the last of the chain in its order that qualifies
      if (sh::length(s.entities[size_t(n)].origin - him) <= kHintNear &&
          clear_to_node(s, him, n, -1))
        end = n;
    if (end < 0) continue;
    for (const int n : c) {
      const float d = sh::length(s.entities[size_t(n)].origin - e.origin);
      if (d > kHintNear || !clear_to_node(s, me, n, m.entity)) continue;
      if (start < 0 || d < best) {
        start = n;
        goal = end;
        best = d;
      }
    }
  }
  if (start < 0) return false;
  m.hint_node = start;
  m.hint_goal = goal;
  return true;
}

}  // namespace

bool gz_hints(Q2Server& s, Q2Monster& m) {
  if (!gz_ai(s) || m.enemy == -1 || m.hold || kind(m).turret) return false;
  if (m.hint_node < 0) {
    if (s.level_time < m.seen_at + 5 || s.level_time < m.hint_check + 10) return false;
    m.hint_check = s.level_time;
    if (!choose_hint(s, m)) return false;
  }
  if (!hint(s, m.hint_node) || !hint(s, m.hint_goal)) {
    m.hint_node = m.hint_goal = -1;
    return false;
  }
  if (s.level_time + 0.001 < m.hint_wait) return true;  // held at a node (a plat's ride)
  Q2Entity& e = body(s, m);
  const Q2Entity& node = s.entities[size_t(m.hint_node)];
  sh::Vec3f d = node.origin - e.origin;
  d.z = 0;
  if (sh::length(d) > e.maxs.x + 8 || std::abs(node.origin.z - e.origin.z) > 64) {
    go_to(s, m, node.origin, kind(m).run);
    return true;
  }
  // Touched: a node's wait held; at the goal the path ends, else one node on toward it.
  if (node.wait > 0) m.hint_wait = s.level_time + double(node.wait);
  if (m.hint_node == m.hint_goal) {
    m.hint_node = m.hint_goal = -1;
    return false;
  }
  const std::vector<int> c = chain_of(s, m.hint_node);
  const auto at = std::find(c.begin(), c.end(), m.hint_node);
  const auto to = std::find(c.begin(), c.end(), m.hint_goal);
  if (at == c.end() || to == c.end()) {
    m.hint_node = m.hint_goal = -1;
    return false;
  }
  m.hint_node = to > at ? *(at + 1) : *(at - 1);
  return true;
}

}  // namespace shooters_edge::server::monster_ai

namespace shooters_edge::server {

bool gz_hint_spawn(Q2Server& s, int i) {
  Q2Entity& e = s.entities[size_t(i)];
  if (e.classname != "hint_path" || s.opt.pack != Q2Pack::ground_zero) return false;
  if (monster_ai::deathmatch(s)) {
    e.gone = true;
    return true;
  }
  if (e.target.empty() && e.targetname.empty()) {
    s.console.push_back("hint_path with no target and no targetname at " + q2_pos(e.origin));
    e.gone = true;
    return true;
  }
  e.hidden = true;
  e.box = false;
  e.mins = {-8, -8, -8};
  e.maxs = {8, 8, 8};
  if (e.block >= 0 && size_t(e.block) < s.map->entities.size()) {
    const std::string& wait = s.map->entities[size_t(e.block)].get_any_case("wait");
    e.wait = wait.empty() ? 0 : std::strtof(wait.c_str(), nullptr);
  }
  return true;
}

}  // namespace shooters_edge::server
