// The floor trap: a floor over lava a touch button slides away (floor_trap.hpp).
//
// ge:summary The floor trap: a floor over lava a touch button slides away, never brushed by accident,
// ge:summary pressed only when an enemy stands on it, the presser staying put till it returns
// ge:keywords floor-trap floortrap trap-button dm2 big-room lava floor-door touch-button press
// ge:skill bots shooter
// ge:test tests/shooters_edge_footwork_test.cpp
#include "shooters_edge/bots/floor_trap.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/brink.hpp"
#include "shooter/features.hpp"
#include "shooter/trace.hpp"
#include "shooters_edge/bots/mind.hpp"
#include "shooters_edge/bots/talk.hpp"

namespace shooters_edge::bots {

namespace sh = ge::shooter;

namespace {

bool boxes_meet(sh::Vec3f alo, sh::Vec3f ahi, sh::Vec3f blo, sh::Vec3f bhi) {
  return alo.x <= bhi.x && ahi.x >= blo.x && alo.y <= bhi.y && ahi.y >= blo.y && alo.z <= bhi.z &&
         ahi.z >= blo.z;
}

// Whether a player standing at `o` (an origin) is over the trap's floor.
bool over_floor(const FloorTrap& tr, sh::Vec3f o) {
  const float feet = o.z + sh::kPlayerMins.z;
  return o.x >= tr.lo.x && o.x <= tr.hi.x && o.y >= tr.lo.y && o.y <= tr.hi.y &&
         feet >= tr.hi.z - 8 && feet <= tr.hi.z + kTrapAbove;
}

// Whether the trap's floor is away (any of its doors off its rest: `Bridge::in`).
bool floor_away(const Fighter& f, const FloorTrap& tr) {
  for (const int32_t b : tr.floors)
    if (f.bridges[static_cast<size_t>(b)].in) return true;
  return false;
}

void end_press(Fighter& f) {
  f.trap_press = -1;
  f.goal_node = -1;
}

float flat(sh::Vec3f a, sh::Vec3f b) {
  return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

// (`trapflee`) Whether a player at `self` runs straight to node origin `o` off the floor: his box
// swept there a jump's height up meets no wall, and no lava or slime lies under the line now (the
// gap the floor leaves).
bool flee_clear(const server::RulesCommon& s, int me, sh::Vec3f self, sh::Vec3f o) {
  const float up = (o.z > self.z ? o.z : self.z) + kFleeUp;
  const sh::HullTrace go =
      sh::trace_move(s.sv, {self.x, self.y, self.z + kFleeUp}, sh::kPlayerMins, sh::kPlayerMaxs,
                     {o.x, o.y, up}, sh::kTraceNoMonsters, me);
  if (go.startsolid || go.fraction < 1) return false;
  const float d = flat(self, o);
  for (float u = 16; u < d; u += 16) {
    const float k = u / d;
    const sh::Vec3f p{self.x + (o.x - self.x) * k, self.y + (o.y - self.y) * k, up};
    if (sh::over_harm(s.sv, p, p, me)) return false;
  }
  return true;
}

// (`trapflee`) The node off the trap's floor a player at `self` standing on it as it goes runs to:
// off the floor's box, on his level or up to a jump higher (dm2's floor lies 20 under the strips
// beside it and 52 under the ledge south), the nearest by its straight distance, a way against
// the slide of the door under him costing double (the floor carries him the other way), and run
// to straight (`flee_clear`). The nav's floor nodes are no way off: they stay where the floor was
// while it slides from under him. The way chosen is kept while it stays clear.
int32_t floor_exit(const Fighter& f, const server::RulesCommon& s, const FloorTrap& tr, int me,
                   sh::Vec3f self) {
  const sh::NavGraph& g = *f.bot.pilot.pilot.g;
  auto off_floor = [&](sh::Vec3f o) {
    return o.x < tr.lo.x - 16 || o.x > tr.hi.x + 16 || o.y < tr.lo.y - 16 || o.y > tr.hi.y + 16;
  };
  if (f.trap_exit >= 0 && static_cast<size_t>(f.trap_exit) < g.nodes.size()) {
    const sh::Vec3f o = g.nodes[static_cast<size_t>(f.trap_exit)].origin;
    if (flat(self, o) <= kFleeReach && flee_clear(s, me, self, o)) return f.trap_exit;
  }
  // The slide under him: the door whose box (where it is now) holds him, off its rest.
  float mx = 0, my = 0;
  for (const int32_t b : tr.floors) {
    const Fighter::Bridge& br = f.bridges[static_cast<size_t>(b)];
    const server::WorldEntity& d = s.world.entities[static_cast<size_t>(br.ent)];
    if (self.x < d.absmin.x || self.x > d.absmax.x || self.y < d.absmin.y || self.y > d.absmax.y)
      continue;
    const float dx = d.origin.x - br.spawn.x, dy = d.origin.y - br.spawn.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len >= 1) mx = dx / len, my = dy / len;
  }
  struct Way {
    float cost;
    int32_t node;
  };
  std::vector<Way> ways;
  for (size_t n = 0; n < g.nodes.size(); n++) {
    const sh::Vec3f o = g.nodes[n].origin;
    if (g.nodes[n].swim || g.nodes[n].mover >= 0 || (!g.harm.empty() && g.harm[n] != 0)) continue;
    if (o.z > self.z + kFleeJump || o.z < self.z - kFleeStep || !off_floor(o)) continue;
    const float d = flat(self, o);
    if (d > kFleeReach) continue;
    const float against = -((o.x - self.x) * mx + (o.y - self.y) * my);
    ways.push_back({d + (against > 0 ? against : 0), static_cast<int32_t>(n)});
  }
  std::sort(ways.begin(), ways.end(), [](const Way& a, const Way& b) {
    return a.cost < b.cost || (a.cost == b.cost && a.node < b.node);
  });
  int tried = 0;
  for (const Way& w : ways) {
    if (tried++ >= kFleeTries) break;
    if (flee_clear(s, me, self, g.nodes[static_cast<size_t>(w.node)].origin)) return w.node;
  }
  return -1;
}

}  // namespace

void floor_traps_init(Fighter& f, const sh::NavGraph& g, const server::RulesCommon& s) {
  f.floor_traps.clear();
  f.trap_press = -1;
  for (size_t k = 0; k < f.button_spots.size(); k++) {
    const ButtonSpot& bs = f.button_spots[k];
    if (bs.shoot || bs.ent < 0 || bs.node < 0) continue;
    FloorTrap tr;
    tr.button = static_cast<int32_t>(k);
    bool first = true;
    for (const int door : bs.doors)
      for (size_t b = 0; b < f.bridges.size(); b++) {
        const Fighter::Bridge& br = f.bridges[b];
        if (!br.floor || br.ent != door) continue;
        const server::WorldEntity& d = s.world.entities[static_cast<size_t>(door)];
        // Lava or slime under it: a harm node within its half-diagonal across, lower down.
        const sh::Vec3f mid{(d.absmin.x + d.absmax.x) * 0.5f, (d.absmin.y + d.absmax.y) * 0.5f,
                            d.absmax.z + 24};
        const float half = std::sqrt((d.absmax.x - d.absmin.x) * (d.absmax.x - d.absmin.x) +
                                     (d.absmax.y - d.absmin.y) * (d.absmax.y - d.absmin.y)) *
                           0.5f;
        if (!sh::nav_harm_near(g, mid, half + 32)) continue;
        tr.floors.push_back(static_cast<int32_t>(b));
        tr.lo = first ? d.absmin : sh::Vec3f{std::fmin(tr.lo.x, d.absmin.x),
                                             std::fmin(tr.lo.y, d.absmin.y),
                                             std::fmin(tr.lo.z, d.absmin.z)};
        tr.hi = first ? d.absmax : sh::Vec3f{std::fmax(tr.hi.x, d.absmax.x),
                                             std::fmax(tr.hi.y, d.absmax.y),
                                             std::fmax(tr.hi.z, d.absmax.z)};
        first = false;
      }
    if (tr.floors.empty()) continue;
    // The floor doors linked to those (touching, moved with them though no button names them:
    // dm2's east half of the floor).
    for (bool grew = true; grew;) {
      grew = false;
      for (size_t b = 0; b < f.bridges.size(); b++) {
        const Fighter::Bridge& br = f.bridges[b];
        bool have = false;
        for (const int32_t x : tr.floors) have = have || x == static_cast<int32_t>(b);
        if (!br.floor || have || br.ent < 0) continue;
        const server::WorldEntity& d = s.world.entities[static_cast<size_t>(br.ent)];
        const sh::Vec3f one{1, 1, 1};
        if (!boxes_meet(d.absmin - one, d.absmax + one, tr.lo, tr.hi)) continue;
        tr.floors.push_back(static_cast<int32_t>(b));
        tr.lo = {std::fmin(tr.lo.x, d.absmin.x), std::fmin(tr.lo.y, d.absmin.y),
                 std::fmin(tr.lo.z, d.absmin.z)};
        tr.hi = {std::fmax(tr.hi.x, d.absmax.x), std::fmax(tr.hi.y, d.absmax.y),
                 std::fmax(tr.hi.z, d.absmax.z)};
        grew = true;
      }
    }
    const server::WorldEntity& be = s.world.entities[static_cast<size_t>(bs.ent)];
    tr.blo = be.absmin;
    tr.bhi = be.absmax;
    // The teleporters landing on the floor (their node over its box, standing on it).
    for (size_t a = 0; a < g.nodes.size(); a++) {
      const sh::NavNode& n = g.nodes[a];
      for (int32_t j = 0; j < n.links; j++) {
        const sh::NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(j)];
        if (l.teleport && over_floor(tr, g.nodes[static_cast<size_t>(l.to)].origin))
          tr.teleports.push_back({static_cast<int32_t>(a), l.to, l.kind, l.time_s});
      }
    }
    f.floor_traps.push_back(std::move(tr));
  }
}

void floor_trap_think(Fighter& f, const server::RulesCommon& s, int me, double t) {
  for (FloorTrap& tr : f.floor_traps) {  // counted whatever the switch: the base's opens too
    const bool away = floor_away(f, tr);
    if (away && !tr.away) f.stats.trap_opens++;
    tr.away = away;
  }
  if (f.features[sh::Feature::trapflee]) {
    // On a trap's floor as it goes (as it sees it, or a mate called it): off it by the nearest way.
    const sh::Vec3f self = s.world.entities[static_cast<size_t>(me)].origin;
    int32_t flee = -1, exit = -1;
    for (size_t k = 0; k < f.floor_traps.size() && flee < 0; k++) {
      const FloorTrap& tr = f.floor_traps[k];
      if (!floor_away(f, tr) || !over_floor(tr, self)) continue;
      exit = floor_exit(f, s, tr, me, self);
      if (exit >= 0) flee = static_cast<int32_t>(k);
    }
    if (flee >= 0) {
      if (f.trap_flee < 0) f.stats.trap_flees++;
      if (exit != f.trap_exit) f.goal_node = -1;
      f.trap_flee = flee;
      f.trap_exit = exit;
    } else if (f.trap_flee >= 0) {
      f.trap_flee = -1;
      f.trap_exit = -1;
      f.goal_node = -1;
    }
  }
  if (!f.features[sh::Feature::floortrap] || f.floor_traps.empty()) return;
  if (f.trap_stay >= 0 && !floor_away(f, f.floor_traps[static_cast<size_t>(f.trap_stay)])) {
    f.trap_stay = -1;  // the floor back: on again
    f.goal_node = -1;
  }
  if (f.trap_press >= 0) {
    const FloorTrap& tr = f.floor_traps[static_cast<size_t>(f.trap_press)];
    if (floor_away(f, tr)) {
      f.stats.trap_fired++;  // done: the floor is gone, the stay is the shut floor's
      Said call;
      call.kind = SayKind::trap;
      mind_say(f, t, call, s.world.entities[static_cast<size_t>(me)].origin);
      const int32_t fired = f.trap_press;
      end_press(f);
      f.trap_stay = fired;  // stays at the button till the floor is back
    } else if (t >= f.trap_until) {
      end_press(f);
    }
    return;
  }
  const sh::Vec3f self = s.world.entities[static_cast<size_t>(me)].origin;
  for (size_t k = 0; k < f.floor_traps.size(); k++) {
    const FloorTrap& tr = f.floor_traps[k];
    if (floor_away(f, tr)) continue;
    const ButtonSpot& bs = f.button_spots[static_cast<size_t>(tr.button)];
    const float dx = bs.at.x - self.x, dy = bs.at.y - self.y;
    if (dx * dx + dy * dy > kTrapReach * kTrapReach || over_floor(tr, self) ||
        std::fabs(self.z + sh::kPlayerMins.z - tr.blo.z) > kTrapLevel)
      continue;
    // The way to it straight, off the floor (from the button's side of it, not across it).
    bool across_floor = false;
    for (int q = 1; q <= 8 && !across_floor; q++) {
      const float u = static_cast<float>(q) / 8;
      const sh::Vec3f p{self.x + dx * u, self.y + dy * u, self.z};
      across_floor = p.x >= tr.lo.x && p.x <= tr.hi.x && p.y >= tr.lo.y && p.y <= tr.hi.y;
    }
    if (across_floor) continue;
    // An enemy over it, seen or heard just now.
    bool enemy = false;
    for (const sh::Contact& c : f.bot.senses.contacts) {
      if (c.ent < 1) continue;
      const ge::ai::Memory& m = t - c.seen.at <= kTrapFresh_s ? c.seen : c.heard;
      if (t - m.at > kTrapFresh_s) continue;
      enemy = enemy || over_floor(tr, {static_cast<float>(m.x), static_cast<float>(m.y),
                                       static_cast<float>(m.z)});
    }
    if (!enemy) continue;
    // No mate over it as far as it knows: where it last saw each (a mate out of its sight is not
    // sent: his last place stands), and where a team line put him lately.
    bool mate = false;
    for (int slot = 0; slot < s.sv.set.maxclients && slot < 32; slot++) {
      if (((f.bot.senses.friends >> slot) & 1u) == 0 || slot + 1 == me) continue;
      const server::WorldEntity& e = s.world.entities[static_cast<size_t>(slot) + 1];
      mate = mate || ((e.origin.x != 0 || e.origin.y != 0) && over_floor(tr, e.origin));
    }
    for (const Mate& m : f.mind.mates)
      mate = mate || (t - m.at_t < kTrapMateFresh_s && over_floor(tr, m.at));
    if (mate) continue;
    f.trap_press = static_cast<int32_t>(k);
    f.trap_until = t + kTrapPress_s;
    f.stats.trap_presses++;
    f.goal_node = -1;
    return;
  }
}

void floor_trap_cmd(Fighter& f, const server::RulesCommon& s, int me, sh::MoveCmd& cmd) {
  if (!f.features[sh::Feature::floortrap] || f.floor_traps.empty()) return;
  const server::WorldEntity& self = s.world.entities[static_cast<size_t>(me)];
  const sh::Vec3f o = self.origin;
  const double yaw = cmd.angles.y * (ge::core::kPi / 180.0);
  const float fx = static_cast<float>(ge::core::libm::cos(yaw)),
              fy = static_cast<float>(ge::core::libm::sin(yaw));
  // The wish in the world: forward along the view, right a quarter turn clockwise of it.
  float wx = cmd.forward * fx + cmd.side * fy, wy = cmd.forward * fy - cmd.side * fx;
  bool changed = false;
  for (size_t k = 0; k < f.floor_traps.size(); k++) {
    const FloorTrap& tr = f.floor_traps[k];
    // The nearest point of the button's box, flat, and how far the player's box is from it.
    const float nx = std::fmax(tr.blo.x, std::fmin(o.x, tr.bhi.x)),
                ny = std::fmax(tr.blo.y, std::fmin(o.y, tr.bhi.y));
    float ux = nx - o.x, uy = ny - o.y;
    const float d = std::sqrt(ux * ux + uy * uy);
    if (d < 1) continue;
    ux /= d, uy /= d;
    const bool level = o.z + sh::kPlayerMaxs.z >= tr.blo.z && o.z + sh::kPlayerMins.z <= tr.bhi.z;
    if (static_cast<int32_t>(k) == f.trap_press) {
      // Pressing: into it, the view where the aim has it.
      if (d > 200) continue;
      wx = 320 * ux, wy = 320 * uy;
      cmd.buttons = static_cast<uint8_t>(cmd.buttons & ~sh::kButtonJump);
      changed = true;
      continue;
    }
    // Not pressing: kept off it, as a person keeps a hand off a trap's switch (the box `kTrapKeep`
    // clear: the wish's share toward it let go, a run into it braked).
    if (!level || d - 16 > kTrapKeep) continue;
    const float along = wx * ux + wy * uy;
    if (along > 0) wx -= along * ux, wy -= along * uy, changed = true;
    const float run = self.velocity.x * ux + self.velocity.y * uy;
    if (run > 0 && (d - 16) < run * kTrapKeep_s) wx -= 320 * ux, wy -= 320 * uy, changed = true;
  }
  // (`trapflee`) On a trap's floor as it goes: run straight at the way off it, no jump (the
  // route's nodes stay where the floor was).
  if (f.trap_flee >= 0 && f.trap_exit >= 0) {
    const sh::Vec3f to = f.bot.pilot.pilot.g->nodes[static_cast<size_t>(f.trap_exit)].origin;
    float ux = to.x - o.x, uy = to.y - o.y;
    const float d = std::sqrt(ux * ux + uy * uy);
    if (d >= 1) {
      wx = 320 * ux / d, wy = 320 * uy / d;
      // Up onto a ledge: jumped near it, else no jump (a hop on the floor lands where it went).
      const bool up = to.z > o.z + kFleeStep && d < kFleeJumpAt &&
                      (static_cast<int>(self.flags) & sh::kFlagOnGround) != 0;
      cmd.buttons = static_cast<uint8_t>(up ? cmd.buttons | sh::kButtonJump
                                            : cmd.buttons & ~sh::kButtonJump);
      changed = true;
    }
  }
  // While a floor is away (as it believes): kept off the teleporters landing on it, as off the
  // button (a fight's footwork in dm2's yellow-armour alcoves stepped into the one at its back:
  // the route has it shut, the feet did not). A teleporter's trigger is where its link starts.
  const sh::NavGraph& g = *f.bot.pilot.pilot.g;
  for (const FloorTrap& tr : f.floor_traps) {
    if (!floor_away(f, tr)) continue;
    for (const sh::NavExtraLink& l : tr.teleports) {
      const sh::Vec3f p = g.nodes[static_cast<size_t>(l.from)].origin;
      if (std::fabs(p.z - o.z) > 48) continue;
      float ux = p.x - o.x, uy = p.y - o.y;
      const float d = std::sqrt(ux * ux + uy * uy);
      if (d < 1 || d > kTeleKeep + 64) continue;
      ux /= d, uy /= d;
      const float along = wx * ux + wy * uy;
      if (along > 0 && d < kTeleKeep + 32) wx -= along * ux, wy -= along * uy, changed = true;
      const float run = self.velocity.x * ux + self.velocity.y * uy;
      if (run > 0 && d - kTeleKeep < run * kTrapKeep_s)
        wx -= 320 * ux, wy -= 320 * uy, changed = true;
    }
  }
  if (!changed) return;
  cmd.forward = sh::quantize_axis(wx * fx + wy * fy);
  cmd.side = sh::quantize_axis(wx * fy - wy * fx);
  f.stats.trap_kept += f.trap_press < 0 ? 1 : 0;
}

void floor_trap_shut(const Fighter& f, std::vector<sh::NavExtraLink>& shut) {
  if (!f.features[sh::Feature::floortrap]) return;
  for (const FloorTrap& tr : f.floor_traps)
    if (floor_away(f, tr)) shut.insert(shut.end(), tr.teleports.begin(), tr.teleports.end());
}

}  // namespace shooters_edge::bots
