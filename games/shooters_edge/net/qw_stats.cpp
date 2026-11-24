// A QuakeWorld match's weapon and item stats; the rules in qw_stats.hpp.
//
// ge:summary Each player's attacks, hits, kills, damage, items and streaks off the QC server
// ge:keywords stats_frame stats_close stat_name weapon-stats midair splash spawn-frags
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_stats_test.cpp
#include "shooters_edge/net/qw_stats.hpp"

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

}  // namespace

const server::StatsGame& qw_stats_game() {
  static const server::StatsGame g = [] {
    server::StatsGame out;
    out.weapons = kMsWeapons;
    for (int w = 0; w < kMsWeapons; w++) out.label[static_cast<size_t>(w)] = kWeaponLabels[w];
    out.rl = static_cast<int>(StatWeapon::rl);
    out.gl = static_cast<int>(StatWeapon::gl);
    int k = 0;
    for (StatWeapon w : {StatWeapon::lg, StatWeapon::rl, StatWeapon::gl, StatWeapon::sng,
                         StatWeapon::ng, StatWeapon::ssg, StatWeapon::sg})
      out.shown[static_cast<size_t>(k++)] = static_cast<uint8_t>(w);
    out.shown_count = k;
    out.item = {"ga", "ya", "ra", "mh", "quad", "pent", "ring"};
    out.words = {"green", "yellow", "red", "mega", "quad", "pent", "ring"};
    return out;
  }();
  return g;
}

const char* stat_name(int stat) {
  static const std::array<std::string, kMatchStats> names = [] {
    std::array<std::string, kMatchStats> out;
    for (int k = 0; k < kMatchStats; k++)
      out[static_cast<size_t>(k)] = server::stats_column_name(qw_stats_game(), k);
    return out;
  }();
  return stat >= 0 && stat < kMatchStats ? names[static_cast<size_t>(stat)].c_str() : "";
}

void stats_close(SeatStats& seat) { server::stats_close(qw_stats_game(), seat); }

std::vector<std::string> stats_lines(const ge::net::MatchReport& r) {
  return server::stats_lines(qw_stats_game(), r);
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
  const server::StatsGame& g = qw_stats_game();
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
    if (me != nullptr && alive && (!was_alive || me->spawned < 0)) server::stats_spawned(me, now);
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
        server::stats_attack(me, static_cast<int>(sw), v);
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
        server::stats_took(g, me,
                           kind >= 0.7f   ? server::StatItem::armour_heavy
                           : kind >= 0.5f ? server::StatItem::armour_middle
                                          : server::StatItem::armour_light);
      }
      if (health - eyes.health[i] >= 50) server::stats_took(g, me, server::StatItem::mega);
      const int32_t rose = items & ~eyes.items[i];
      if ((rose & kQuad) != 0) server::stats_took(g, me, server::StatItem::quad);
      if ((rose & kPent) != 0) server::stats_took(g, me, server::StatItem::power_b);
      if ((rose & kRing) != 0) server::stats_took(g, me, server::StatItem::power_c);
    }
    if (me != nullptr && alive && (items & kQuad) != 0) server::stats_quad_held(g, me, dt);
    // A blow: who dealt it and with what.
    const float blow = eyes.life[i] - life;
    const bool died = was_alive && !alive;
    if (!(blow >= kLeast || (died && blow > 0))) {
      if (died && me != nullptr) server::stats_close(g, *me);
      continue;
    }
    const int from = entity_of(s, e, f.dmg_inflictor);
    if (from <= 0 || static_cast<size_t>(from) >= n) {
      if (died && me != nullptr) server::stats_close(g, *me);
      continue;
    }
    const bool missile = from > clients;
    const Kind k = missile ? static_cast<Kind>(eyes.kind[static_cast<size_t>(from)]) : kNone;
    const int by = missile ? eyes.owner[static_cast<size_t>(from)] : from;
    if (by < 1 || by > clients || (missile && k == kNone)) {
      if (died && me != nullptr) server::stats_close(g, *me);
      continue;
    }
    SeatStats* them = seat(by);
    const bool quad = (items_of(by) & kQuad) != 0;
    const float scale = quad ? 4.f : 1.f;
    const int weapon =
        missile ? missile_weapon(k) : held_weapon(static_cast<int32_t>(field(s, by, f.weapon)));
    const bool self = by == e, enemy = !self && enemies(by, e);
    const int64_t dmg = whole(blow);
    server::stats_blow(g, them, me, self, enemy, dmg, (items & (kRl | kLg)) != 0);
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
        server::stats_hit(them, weapon, hits);
      } else if (struck[mi] == 0) {
        bool touched = k == kNail || k == kHeavyNail;
        if (k == kRocket)
          touched = blow == kDirect * scale || (died && gap_to_box(body, eyes.at[mi]) <= kNear);
        if (k == kGrenade)
          touched = eyes.speed[mi] >= kRolling && gap_to_box(body, eyes.at[mi]) <= kNear;
        if (touched) {
          server::stats_hit(them, weapon, 1);
          struck[mi] = 1;
        }
      }
      if (enemy && (k == kRocket || k == kGrenade)) server::stats_splash(g, them, weapon, dmg);
      if (enemy && k == kRocket && eyes.aloft[i] != 0) {
        const sh::Vec3f was = eyes.at[i];
        const sh::Vec3f down{was.x, was.y, was.z - kDrop};
        const sh::HullTrace t = sh::trace_move(s.sv, was, {}, {}, down, sh::kTraceNoMonsters, e);
        const int64_t height = whole(was.z + body.mins.z - t.endpos.z);
        server::stats_midair(g, them, height);
      }
    }
    if (died) server::stats_died(g, me, them, enemy, weapon, quad, now);
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
