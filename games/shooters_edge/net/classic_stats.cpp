// A classic match's weapon and item stats; the rules in classic_stats.hpp.
//
// ge:summary Each player's attacks, hits, kills, damage, items and streaks off the QC server
// ge:keywords stats_frame stats_close stat_name weapon-stats midair splash spawn-frags
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_stats_test.cpp
#include "shooters_edge/net/classic_stats.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include "core/num.hpp"
#include "progs/vm.hpp"
#include "shooter/trace.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::net {

namespace {

namespace pg = ge::progs;
namespace sh = ge::shooter;

// The baseline mod's weapon and item bits (the classic ones).
constexpr int32_t kSg = 1, kSsg = 2, kNg = 4, kSng = 8, kGl = 16, kRl = 32, kLg = 64, kAxe = 4096;
constexpr int32_t kRing = 524288, kPent = 1048576, kQuad = 4194304;
// Its blows: a pellet, a pulse of the arc, a rocket's direct hit (four times each under the quad).
constexpr float kPellet = 4, kPulse = 30, kDirect = 100;
// A rocket or grenade this near a player it hurt touched them (units from their box).
constexpr float kNear = 32;
// A grenade slower than this lay still: its blast is no touch (units/s).
constexpr float kRolling = 50;
// The least blow that counts as a hit or damage while the player lives (a mega health's rot is 1).
constexpr float kLeast = 3;
// A kill this soon after the victim came in is a spawn frag, s.
constexpr double kSpawnFrag = 2;
// How far down a mid-air's height is looked for, units.
constexpr float kDrop = 2048;

constexpr const char* kWeaponLabels[kMsWeapons] = {"axe", "sg", "ssg", "ng",
                                                   "sng", "gl", "rl",  "lg"};

enum Kind : uint8_t { kNone, kNail, kHeavyNail, kGrenade, kRocket };

float field(const server::QcServer& s, int e, int f) {
  return f < 0 ? 0 : pg::as_float(*pg::entity_field(s.q.vm, e, f));
}

int entity_of(const server::QcServer& s, int e, int f) {
  return f < 0 ? -1
               : pg::entity_number(s.q.vm, static_cast<int32_t>(*pg::entity_field(s.q.vm, e, f)));
}

Kind kind_of(const server::QcServer& s, int e) {
  if (s.sv.edicts[static_cast<size_t>(e)].free) return kNone;
  const char* cn =
      pg::vm_string(s.q.vm, static_cast<int32_t>(*pg::entity_field(s.q.vm, e, pg::kFClassname)));
  if (cn == nullptr) return kNone;
  if (std::strcmp(cn, "rocket") == 0) return kRocket;
  if (std::strcmp(cn, "grenade") == 0) return kGrenade;
  if (std::strcmp(cn, "spike") == 0) {
    const char* m =
        pg::vm_string(s.q.vm, static_cast<int32_t>(*pg::entity_field(s.q.vm, e, pg::kFModel)));
    return m != nullptr && std::strstr(m, "s_spike") != nullptr ? kHeavyNail : kNail;
  }
  return kNone;
}

float gap_to_box(const sh::Edict& p, sh::Vec3f at) {
  float g = 0;
  const float lo[3] = {p.absmin.x, p.absmin.y, p.absmin.z},
              hi[3] = {p.absmax.x, p.absmax.y, p.absmax.z};
  const float v[3] = {at.x, at.y, at.z};
  for (int k = 0; k < 3; k++) {
    const float d = v[k] < lo[k] ? lo[k] - v[k] : (v[k] > hi[k] ? v[k] - hi[k] : 0);
    g = d > g ? d : g;
  }
  return g;
}

int64_t whole(double v) { return static_cast<int64_t>(ge::core::num::round(v)); }

// The weapon a player's own blow came from (the one they hold), or -1: a stomp, say.
int held_weapon(int32_t w) {
  switch (w) {
    case kAxe:
      return static_cast<int>(StatWeapon::axe);
    case kSg:
      return static_cast<int>(StatWeapon::sg);
    case kSsg:
      return static_cast<int>(StatWeapon::ssg);
    case kLg:
      return static_cast<int>(StatWeapon::lg);
    default:
      return -1;
  }
}

int missile_weapon(Kind k) {
  switch (k) {
    case kNail:
      return static_cast<int>(StatWeapon::ng);
    case kHeavyNail:
      return static_cast<int>(StatWeapon::sng);
    case kGrenade:
      return static_cast<int>(StatWeapon::gl);
    case kRocket:
      return static_cast<int>(StatWeapon::rl);
    default:
      return -1;
  }
}

int64_t& at(SeatStats& st, int weapon, int k) {
  return st.n[static_cast<size_t>(4) * static_cast<size_t>(weapon) + static_cast<size_t>(k)];
}

void close_runs(SeatStats& st) {
  st.n[kMsStreakMax] = std::max(st.n[kMsStreakMax], st.run);
  st.n[kMsQuadRunMax] = std::max(st.n[kMsQuadRunMax], st.quad_run);
  st.run = st.quad_run = 0;
}

// A missile's hit proves its shot: one the ammunition did not show (fired on the frame its
// shooter died, or took ammunition up) is counted with it.
void proven(SeatStats& st, int weapon) {
  int64_t& attacks = at(st, weapon, kMsAttacks);
  attacks = std::max(attacks, at(st, weapon, kMsHits));
}

}  // namespace

const char* stat_name(int stat) {
  const char* const* kWeapons = kWeaponLabels;
  static const char* kParts[4] = {"attacks", "hits", "kills", "deaths"};
  static char names[4 * kMsWeapons][16] = {};
  if (stat < 4 * kMsWeapons) {
    char* n = names[stat];
    if (n[0] == 0) {
      const char* w = kWeapons[stat / 4];
      const char* p = kParts[stat % 4];
      const size_t wl = std::strlen(w), pl = std::strlen(p);
      std::memcpy(n, w, wl);
      n[wl] = '_';
      std::memcpy(n + wl + 1, p, pl + 1);
    }
    return n;
  }
  static const char* kRest[kMatchStats - 4 * kMsWeapons] = {
      "rl_splash", "gl_splash",   "rl_damage",  "midair",      "midair_height", "midair_height_max",
      "dmg_given", "dmg_taken",   "dmg_team",   "dmg_self",    "dmg_ewep",      "ga",
      "ya",        "ra",          "mh",         "quad",        "pent",          "ring",
      "quad_s",    "spawn_frags", "streak_max", "quad_run_max"};
  return stat >= 0 && stat < kMatchStats ? kRest[stat - 4 * kMsWeapons] : "";
}

void stats_close(SeatStats& seat) { close_runs(seat); }

namespace {

// A share as a percentage with one decimal ("45.5%").
std::string percent(int64_t part, int64_t whole_n) {
  const int64_t tenths = whole_n > 0 ? (part * 1000 + whole_n / 2) / whole_n : 0;
  return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + "%";
}

std::string counted(int64_t n, const char* what) {
  return std::to_string(n) + " " + what + (n == 1 || n == -1 ? "" : "s");
}

}  // namespace

std::vector<std::string> stats_lines(const ge::net::MatchReport& r) {
  std::vector<std::string> out;
  if (r.columns.size() != static_cast<size_t>(kMatchStats)) return out;
  for (int k = 0; k < kMatchStats; k++)
    if (r.columns[static_cast<size_t>(k)] != stat_name(k)) return out;
  out.emplace_back("Player statistics:");
  for (const ge::net::ReportPlayer& p : r.players) {
    if (p.columns.size() != static_cast<size_t>(kMatchStats)) continue;
    const auto v = [&](int k) { return p.columns[static_cast<size_t>(k)]; };
    const auto w = [&](StatWeapon sw, int k) { return v(stat_of(sw, k)); };
    const int64_t effi_base = p.score + p.deaths;
    out.push_back(" " + p.name + (p.team.empty() ? "" : " [" + p.team + "]") + ": " +
                  counted(p.score, "frag") + ", " + counted(p.deaths, "death") + ", " +
                  percent(p.score > 0 ? p.score : 0, effi_base > 0 ? effi_base : 1) +
                  " efficiency");
    std::string wp;
    const auto add = [&](const std::string& part) { wp += (wp.empty() ? "" : ", ") + part; };
    const int64_t rockets = w(StatWeapon::rl, kMsAttacks), grenades = w(StatWeapon::gl, kMsAttacks);
    if (w(StatWeapon::lg, kMsAttacks) > 0)
      add("lg " + percent(w(StatWeapon::lg, kMsHits), w(StatWeapon::lg, kMsAttacks)));
    if (rockets > 0)
      add("rl " + percent(v(kMsRlSplash), rockets) + " (" + std::to_string(v(kMsRlSplash)) + "/" +
          std::to_string(rockets) + ")");
    if (grenades > 0) add("gl " + percent(v(kMsGlSplash), grenades));
    for (StatWeapon sw : {StatWeapon::sng, StatWeapon::ng, StatWeapon::ssg, StatWeapon::sg})
      if (w(sw, kMsAttacks) > 0)
        add(std::string(kWeaponLabels[static_cast<int>(sw)]) + " " +
            percent(w(sw, kMsHits), w(sw, kMsAttacks)));
    if (!wp.empty()) out.push_back("  Weapons: " + wp);
    if (rockets > 0) {
      const int64_t splash = v(kMsRlSplash);
      const int64_t tenths = splash > 0 ? (v(kMsRlDamage) * 10 + splash / 2) / splash : 0;
      std::string line = "  Rockets: " + std::to_string(w(StatWeapon::rl, kMsHits)) + " direct, " +
                         std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) +
                         " average damage";
      if (v(kMsMidair) > 0)
        line += ", " + std::to_string(v(kMsMidair)) + " mid-air (highest " +
                std::to_string(v(kMsMidairHeightMax)) + ")";
      out.push_back(line);
    }
    out.push_back("  Armour: green " + std::to_string(v(kMsGa)) + ", yellow " +
                  std::to_string(v(kMsYa)) + ", red " + std::to_string(v(kMsRa)) + ", mega " +
                  std::to_string(v(kMsMh)) + "; quad " + std::to_string(v(kMsQuad)) + ", pent " +
                  std::to_string(v(kMsPent)) + ", ring " + std::to_string(v(kMsRing)));
    out.push_back("  Damage: taken " + std::to_string(v(kMsDmgTaken)) + ", given " +
                  std::to_string(v(kMsDmgGiven)) + ", team " + std::to_string(v(kMsDmgTeam)) +
                  ", self " + std::to_string(v(kMsDmgSelf)));
    out.push_back("  Streak " + std::to_string(v(kMsStreakMax)) + ", quad run " +
                  std::to_string(v(kMsQuadRunMax)) + ", quad held " + std::to_string(v(kMsQuadS)) +
                  "s, spawn frags " + std::to_string(v(kMsSpawnFrags)));
  }
  return out;
}

void stats_frame(StatsEyes& eyes, const std::array<SeatStats*, sh::kMaxClients>& seats,
                 const server::QcServer& s) {
  const pg::LayoutFields& f = s.q.layout->f;
  const int clients = std::min(s.sv.set.maxclients, sh::kMaxClients);
  const size_t n = s.sv.edicts.size();
  const double now = s.sv.time;
  const bool fresh = eyes.life.size() != n || now < eyes.time || now - eyes.time > 1;
  if (fresh) {
    eyes = {};
    eyes.life.assign(n, 0);
    eyes.health.assign(n, 0);
    eyes.armour.assign(n, 0);
    eyes.attack_finished.assign(n, 0);
    eyes.items.assign(n, 0);
    eyes.weapon.assign(n, 0);
    eyes.ammo.assign(n, {});
    eyes.aloft.assign(n, 0);
    eyes.at.assign(n, {});
    eyes.kind.assign(n, kNone);
    eyes.speed.assign(n, 0);
    eyes.owner.assign(n, -1);
  }
  const double dt = fresh ? 0 : now - eyes.time;
  const auto seat = [&](int e) -> SeatStats* {
    return e >= 1 && e <= clients ? seats[static_cast<size_t>(e - 1)] : nullptr;
  };
  const auto items_of = [&](int e) { return static_cast<int32_t>(field(s, e, f.items)); };
  const ge::console::Cvar* teamplay = s.cvars.find("teamplay");
  const bool teams = teamplay != nullptr && teamplay->number != 0;
  const auto enemies = [&](int a, int b) {
    const float ta = field(s, a, f.team);
    return !teams || ta == 0 || ta != field(s, b, f.team);
  };
  std::vector<uint8_t> struck(n, 0);  // a missile's direct hit counted this frame
  for (int e = 1; e <= clients && !fresh; e++) {
    const size_t i = static_cast<size_t>(e);
    if (!s.clients[i - 1].active) continue;
    SeatStats* me = seat(e);
    const float health = field(s, e, f.health), armour = field(s, e, f.armorvalue);
    const float life = (health > 0 ? health : 0) + armour;
    const int32_t items = items_of(e);
    const bool was_alive = eyes.health[i] > 0, alive = health > 0;
    if (me != nullptr && alive && (!was_alive || me->spawned < 0)) me->spawned = now;
    // Attacks: ammunition spent on the weapon held the frame before, alive in both frames.
    if (me != nullptr && alive && was_alive) {
      const int32_t w = eyes.weapon[i];
      const auto spent = [&](int k) {
        const float d = eyes.ammo[i][static_cast<size_t>(k)] - field(s, e,
                                                                     k == 0   ? f.ammo_shells
                                                                     : k == 1 ? f.ammo_nails
                                                                     : k == 2 ? f.ammo_rockets
                                                                              : f.ammo_cells);
        return d > 0 ? whole(d) : 0;
      };
      const auto add = [&](StatWeapon sw, int64_t v) {
        at(*me, static_cast<int>(sw), kMsAttacks) += v;
      };
      if (w == kAxe && field(s, e, s.world_fields.attack_finished) > eyes.attack_finished[i])
        add(StatWeapon::axe, 1);
      if (w == kSg) add(StatWeapon::sg, 6 * spent(0));
      if (w == kSsg) {
        add(StatWeapon::ssg, 14 * (spent(0) / 2));
        add(StatWeapon::sg, 6 * (spent(0) % 2));  // one shell left fires as the scattergun
      }
      if (w == kNg) add(StatWeapon::ng, spent(1));
      if (w == kSng) {
        add(StatWeapon::sng, spent(1) / 2);
        add(StatWeapon::ng, spent(1) % 2);
      }
      if (w == kGl) add(StatWeapon::gl, spent(2));
      if (w == kRl) add(StatWeapon::rl, spent(2));
      if (w == kLg) add(StatWeapon::lg, spent(3));
    }
    // Items: armour's points rising (by its kind), a mega health, a power-up's bit set.
    if (me != nullptr && alive && was_alive) {
      if (armour > eyes.armour[i] + 0.5f) {
        const float kind = field(s, e, f.armortype);
        me->n[kind >= 0.7f ? kMsRa : kind >= 0.5f ? kMsYa : kMsGa]++;
      }
      if (health - eyes.health[i] >= 50) me->n[kMsMh]++;
      const int32_t rose = items & ~eyes.items[i];
      if ((rose & kQuad) != 0) {
        me->n[kMsQuad]++;
        me->n[kMsQuadRunMax] = std::max(me->n[kMsQuadRunMax], me->quad_run);
        me->quad_run = 0;
      }
      if ((rose & kPent) != 0) me->n[kMsPent]++;
      if ((rose & kRing) != 0) me->n[kMsRing]++;
    }
    if (me != nullptr && alive && (items & kQuad) != 0) {
      me->quad_carry += dt;
      const int64_t whole_s = static_cast<int64_t>(me->quad_carry);
      me->n[kMsQuadS] += whole_s;
      me->quad_carry -= static_cast<double>(whole_s);
    }
    // A blow: who dealt it and with what.
    const float blow = eyes.life[i] - life;
    const bool died = was_alive && !alive;
    if (!(blow >= kLeast || (died && blow > 0))) {
      if (died && me != nullptr) close_runs(*me);
      continue;
    }
    const int from = entity_of(s, e, f.dmg_inflictor);
    if (from <= 0 || static_cast<size_t>(from) >= n) {
      if (died && me != nullptr) close_runs(*me);
      continue;
    }
    const bool missile = from > clients;
    const Kind k = missile ? static_cast<Kind>(eyes.kind[static_cast<size_t>(from)]) : kNone;
    const int by = missile ? eyes.owner[static_cast<size_t>(from)] : from;
    if (by < 1 || by > clients || (missile && k == kNone)) {
      if (died && me != nullptr) close_runs(*me);
      continue;
    }
    SeatStats* them = seat(by);
    const bool quad = (items_of(by) & kQuad) != 0;
    const float scale = quad ? 4.f : 1.f;
    const int weapon =
        missile ? missile_weapon(k) : held_weapon(static_cast<int32_t>(field(s, by, f.weapon)));
    const bool self = by == e, enemy = !self && enemies(by, e);
    const int64_t dmg = whole(blow);
    if (self) {
      if (me != nullptr) me->n[kMsDmgSelf] += dmg;
    } else if (!enemy) {
      if (them != nullptr) them->n[kMsDmgTeam] += dmg;
    } else {
      if (them != nullptr) them->n[kMsDmgGiven] += dmg;
      if (me != nullptr) me->n[kMsDmgTaken] += dmg;
      if (them != nullptr && (items & (kRl | kLg)) != 0) them->n[kMsDmgEwep] += dmg;
    }
    // Hits (team mates count, oneself never).
    if (!self && them != nullptr && weapon >= 0) {
      const auto many = [&](float each) {
        const int64_t m = whole(blow / (each * scale));
        return m > 0 ? m : 1;
      };
      const sh::Edict& body = s.sv.edicts[i];
      const size_t mi = static_cast<size_t>(from);
      if (!missile) {
        const int64_t hits = weapon == static_cast<int>(StatWeapon::sg) ||
                                     weapon == static_cast<int>(StatWeapon::ssg)
                                 ? many(kPellet)
                             : weapon == static_cast<int>(StatWeapon::lg) ? many(kPulse)
                                                                          : 1;
        at(*them, weapon, kMsHits) += hits;
      } else if (struck[mi] == 0) {
        bool touched = k == kNail || k == kHeavyNail;
        if (k == kRocket)
          touched = blow == kDirect * scale || (died && gap_to_box(body, eyes.at[mi]) <= kNear);
        if (k == kGrenade)
          touched = eyes.speed[mi] >= kRolling && gap_to_box(body, eyes.at[mi]) <= kNear;
        if (touched) {
          at(*them, weapon, kMsHits)++;
          proven(*them, weapon);
          struck[mi] = 1;
        }
      }
      if (enemy && (k == kRocket || k == kGrenade)) {
        them->n[k == kRocket ? kMsRlSplash : kMsGlSplash]++;
        proven(*them, weapon);
        if (k == kRocket) them->n[kMsRlDamage] += dmg;
      }
      if (enemy && k == kRocket && eyes.aloft[i] != 0) {
        const sh::Vec3f was = eyes.at[i];
        const sh::Vec3f down{was.x, was.y, was.z - kDrop};
        const sh::HullTrace t = sh::trace_move(s.sv, was, {}, {}, down, sh::kTraceNoMonsters, e);
        const int64_t height = whole(was.z + body.mins.z - t.endpos.z);
        them->n[kMsMidair]++;
        them->n[kMsMidairHeight] += height > 0 ? height : 0;
        them->n[kMsMidairHeightMax] = std::max(them->n[kMsMidairHeightMax], height);
      }
    }
    if (!died) continue;
    if (enemy && them != nullptr) {
      if (weapon >= 0) at(*them, weapon, kMsKills)++;
      if (me != nullptr && now - me->spawned <= kSpawnFrag) them->n[kMsSpawnFrags]++;
      them->run++;
      if (quad) them->quad_run++;
    }
    if (enemy && weapon >= 0 && me != nullptr) at(*me, weapon, kMsDeaths)++;
    if (me != nullptr) close_runs(*me);
  }
  // What this frame read, for the next.
  for (int e = 1; e <= clients; e++) {
    const size_t i = static_cast<size_t>(e);
    const float health = field(s, e, f.health);
    eyes.health[i] = health;
    eyes.armour[i] = field(s, e, f.armorvalue);
    eyes.life[i] = (health > 0 ? health : 0) + eyes.armour[i];
    eyes.attack_finished[i] = field(s, e, s.world_fields.attack_finished);
    eyes.items[i] = items_of(e);
    eyes.weapon[i] = static_cast<int32_t>(field(s, e, f.weapon));
    eyes.ammo[i] = {field(s, e, f.ammo_shells), field(s, e, f.ammo_nails),
                    field(s, e, f.ammo_rockets), field(s, e, f.ammo_cells)};
    const sh::Edict& body = s.sv.edicts[i];
    eyes.aloft[i] = health > 0 && (body.flags & sh::kFlagOnGround) == 0 && body.waterlevel < 2;
    eyes.at[i] = body.origin;
  }
  for (size_t e = static_cast<size_t>(clients) + 1; e < n; e++) {
    const Kind k = kind_of(s, static_cast<int>(e));
    eyes.kind[e] = k;
    if (k == kNone) continue;
    const sh::Edict& m = s.sv.edicts[e];
    eyes.at[e] = m.origin;
    eyes.speed[e] = std::sqrt(m.velocity.x * m.velocity.x + m.velocity.y * m.velocity.y +
                              m.velocity.z * m.velocity.z);
    eyes.owner[e] = entity_of(s, static_cast<int>(e), f.owner);
  }
  eyes.time = now;
}

}  // namespace shooters_edge::net
