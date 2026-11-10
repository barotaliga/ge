// Out of a pit by rocket (escape.hpp): the keys, the proof by the move, the escapes of a map.
//
// ge:summary The rocket out of a pit's liquid: its keys, its proof and every escape of a map
// ge:keywords escape lava slime pit rocket-escape prove_escape nav_add_escapes
// ge:skill shooter bots
// ge:test tests/shooter_escape_test.cpp
#include "shooter/escape.hpp"

#include <algorithm>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/hull.hpp"
#include "shooter/pmove.hpp"
#include "shooter/world.hpp"

namespace ge::shooter {

namespace {

constexpr float kSurfaceStep = 2;  // the surface is looked for up a column in steps this tall
constexpr float kSurfaceMost = 512;
constexpr float kEyes = 22;        // the eyes over the origin
constexpr float kFeet = 24;        // a standing node's origin over its floor
constexpr float kLanded_s = 0.1f;  // a landing counts this long after the blast
constexpr float kOffWall = 1;      // the takeoff stands this far off the wall it backs against
// The takeoff's eyes this far under the surface: the rise from there is the view's stroke down.
constexpr float kUnder = 24;
constexpr int kPitches = 3;  // NavEscapeSettings::pitches
constexpr int kLevels = 3;   // the shot's waterlevels tried: 3, 2, 1
constexpr int kSectors = 8;  // a column's ledges tried: the nearest in each of these directions
constexpr float kLandNear = 32;
constexpr float kNeverOut_s = 0.5f;  // no shot this long after the backing's limit: no way out  //
                                     // a landing this near its node, across and up or down

float across(Vec3f a, Vec3f b) {
  const float dx = b.x - a.x, dy = b.y - a.y;
  return root_of(dx * dx + dy * dy);
}

float yaw_at(Vec3f from, Vec3f to) {
  const double r =
      core::libm::atan2(static_cast<double>(to.y - from.y), static_cast<double>(to.x - from.x));
  return static_cast<float>(r * (180.0 / core::kPi));
}

// The grid cell a coordinate in cells falls in (rounded down).
int32_t cell_of(float v) {
  const auto i = static_cast<int32_t>(v);
  return v < static_cast<float>(i) ? i - 1 : i;
}

float wrap(float a) {
  while (a > 180) a -= 360;
  while (a <= -180) a += 360;
  return a;
}

}  // namespace

EscapeKeys escape_back_keys(const NavEscape& e, int waterlevel, Vec3f velocity, float backed_s) {
  EscapeKeys k;
  k.yaw = e.yaw;
  k.pitch = e.pitch;  // looking down already: the hand is there as the eyes come out
  k.forward = -1;
  k.jump = true;
  // Rising, at the escape's level; or no longer rising with the eyes out; or backed long enough.
  const bool rising_out = waterlevel <= e.fire_level && velocity.z > 0;
  const bool topped = waterlevel <= 2 && velocity.z <= 0;
  if (backed_s >= kEscapeBackMin_s &&
      (rising_out || topped || (waterlevel <= 2 && backed_s >= kEscapeBackMax_s))) {
    k.fire = true;
  }
  return k;
}

EscapeKeys escape_flight_keys(const NavEscape& e) {
  EscapeKeys k;
  k.yaw = e.yaw;
  k.forward = -1;
  return k;
}

NavProof prove_escape(const MoveWorld& world, const MoveSettings& move, const NavEscape& e,
                      const NavSettings& set, const RocketJumpSpec& rj, float* damage,
                      std::vector<Vec3f>* path) {
  NavProof p;
  MoveState s;
  s.origin = e.from;
  s.maxspeed = move.maxspeed;
  s.waterlevel = water_level(world, s.origin, &s.watertype);
  const float step_s = static_cast<float>(set.msec) * 0.001f;
  p.end = e.from;
  p.lo = e.from + kPlayerMins;
  p.hi = e.from + kPlayerMaxs;
  if (path) path->push_back(e.from);
  float backed_s = 0, burst_t = -1;
  bool fired = false, burst = false;
  Vec3f burst_at;
  const float limit = kEscapeBackMax_s + set.limit_s;
  for (int i = 0;; i++) {
    const float t = static_cast<float>(i + 1) * step_s;
    const EscapeKeys k =
        fired ? escape_flight_keys(e) : escape_back_keys(e, s.waterlevel, s.velocity, backed_s);
    MoveCmd c;
    c.msec = static_cast<uint8_t>(set.msec);
    c.angles = {k.pitch, k.yaw, 0};
    c.forward = static_cast<int16_t>(move.maxspeed * k.forward);
    c.buttons = k.jump ? kButtonJump : 0;
    player_move(s, c, move, world);
    if (!fired) backed_s += step_s;
    if (k.fire) {
      // The rocket, fired after the command's move, through the liquid to what it meets.
      const RocketBurst rb = rocket_burst(world, s.origin, {k.pitch, k.yaw, 0}, rj);
      if (!rb.ok) return p;
      fired = true;
      burst_at = rb.at;
      burst_t = t + rb.flight_s;
    }
    if (fired && !burst && t >= burst_t) {
      burst = true;
      const float d = rocket_blast(s, burst_at, rj);
      if (d <= 0) return p;
      if (damage) *damage = d;
    }
    p.commands++;
    if (path) path->push_back(s.origin);
    const Vec3f blo = s.origin + kPlayerMins, bhi = s.origin + kPlayerMaxs;
    for (int a = 0; a < 3; a++) {
      if (blo[a] < p.lo[a]) p.lo[a] = blo[a];
      if (bhi[a] > p.hi[a]) p.hi[a] = bhi[a];
    }
    p.end = s.origin;
    p.time_s = t;
    // Never surfaced to shoot (a ceiling over the liquid), or the blast spent with the swimmer
    // still in the liquid and sinking: not out.
    if (!fired && backed_s > kEscapeBackMax_s + kNeverOut_s) return p;
    if (burst && s.waterlevel >= 2 && s.velocity.z <= 0) return p;
    if (burst && s.ground >= 0 && t >= burst_t + kLanded_s) {
      p.landed = true;
      p.reached = s.waterlevel == 0;
      return p;
    }
    if (t >= limit) return p;
  }
}

NavEscapeCounts nav_add_escapes(const MoveWorld& world, const MoveSettings& move, NavGraph& g,
                                std::span<const uint8_t> land, const NavEscapeSettings& set) {
  NavEscapeCounts out;
  const size_t nodes = g.nodes.size();
  if (land.size() != nodes) return out;
  // The nodes in a liquid at least waist deep, and the grid columns they stand in.
  std::vector<uint8_t> liquid(nodes, 0);
  struct Col {
    int32_t cx, cy;
    int32_t node;
    bool burns;  // lava or slime: proven before water (the budget goes where staying kills)
  };
  std::vector<Col> cols;
  const float sp = g.set.spacing > 0 ? g.set.spacing : 32;
  for (size_t n = 0; n < nodes; n++) {
    int type = kContentsEmpty;
    if (water_level(world, g.nodes[n].origin, &type) < 2) continue;
    liquid[n] = 1;
    const Vec3f o = g.nodes[n].origin;
    cols.push_back({cell_of(o.x / sp), cell_of(o.y / sp), static_cast<int32_t>(n),
                    type == kContentsLava || type == kContentsSlime});
  }
  std::sort(cols.begin(), cols.end(), [](const Col& a, const Col& b) {
    return a.cx != b.cx ? a.cx < b.cx : a.cy != b.cy ? a.cy < b.cy : a.node < b.node;
  });
  cols.erase(std::unique(cols.begin(), cols.end(),
                         [](const Col& a, const Col& b) { return a.cx == b.cx && a.cy == b.cy; }),
             cols.end());
  std::stable_sort(cols.begin(), cols.end(),
                   [](const Col& a, const Col& b) { return a.burns && !b.burns; });
  const size_t first_escape = g.escapes.size();
  struct Cand {
    float d;
    int32_t n;
  };
  std::vector<Cand> cands;
  for (const Col& col : cols) {
    if (out.proofs >= set.max_proofs) break;
    // The surface up the column: the first point the eyes are out; the takeoff as far under it as
    // `kUnder` and the pit's floor let the box stand.
    const Vec3f seed = g.nodes[static_cast<size_t>(col.node)].origin;
    float out_z = 0;
    bool found = false;
    for (int step = 0; static_cast<float>(step) * kSurfaceStep <= kSurfaceMost; step++) {
      const Vec3f at{seed.x, seed.y, seed.z + static_cast<float>(step) * kSurfaceStep};
      if (water_level(world, at) < 3) {
        out_z = at.z;
        found = true;
        break;
      }
    }
    if (!found) continue;
    Vec3f from;
    found = false;
    for (int step = 0; static_cast<float>(step) * kSurfaceStep <= kUnder; step++) {
      const float under = kUnder - static_cast<float>(step) * kSurfaceStep;
      from = {seed.x, seed.y, out_z - kSurfaceStep - under};
      if (water_level(world, from) == 3 && !player_trace(world, from, from).startsolid) {
        found = true;
        break;
      }
    }
    if (!found) continue;
    out.columns++;
    const float surface = out_z + kEyes;
    cands.clear();
    for (size_t n = 0; n < nodes; n++) {
      if (!land[n]) continue;
      const Vec3f o = g.nodes[n].origin;
      const float d = across(from, o);
      const float rise = o.z - kFeet - surface;
      if (d > set.far || rise < set.climb || rise > set.up) continue;
      cands.push_back({d, static_cast<int32_t>(n)});
    }
    std::sort(cands.begin(), cands.end(),
              [](const Cand& a, const Cand& b) { return a.d != b.d ? a.d < b.d : a.n < b.n; });
    // The nearest in each of `kSectors` directions round the column (a ledge on every side, not
    // the nearest few of one), nearest first.
    {
      size_t kept = 0;
      bool sector_used[kSectors] = {};
      for (const Cand& cd : cands) {
        const float yaw = yaw_at(from, g.nodes[static_cast<size_t>(cd.n)].origin);
        int sector = static_cast<int>((wrap(yaw) + 180) / (360.0f / kSectors));
        sector = sector < 0 ? 0 : sector >= kSectors ? kSectors - 1 : sector;
        if (sector_used[sector]) continue;
        sector_used[sector] = true;
        cands[kept++] = cd;
      }
      cands.resize(kept);
    }
    bool held = false;
    int tried = 0;
    for (const Cand& cd : cands) {
      if (held || tried >= set.targets || out.proofs >= set.max_proofs) break;
      const Vec3f to = g.nodes[static_cast<size_t>(cd.n)].origin;
      const float toward = yaw_at(from, to);
      // A wall to back against under the ledge: the takeoff backed against it already (the
      // swimmer gets there under the surface and rises from there). None near: the takeoff is the
      // column itself, backing toward the ledge as he rises.
      const Vec3f dir = angle_vectors({0, toward, 0}).forward;
      const HullTrace tr = player_trace(world, from, from + dir * set.wall_near);
      if (tr.startsolid) continue;
      tried++;
      const Vec3f at_wall = tr.fraction < 1 ? tr.endpos - dir * kOffWall : from;
      if (player_trace(world, at_wall, at_wall).startsolid || water_level(world, at_wall) < 3)
        continue;
      // The soonest shot first (the liquid burns): from under the surface, then eyes out, then
      // waist out, at each pitch.
      for (int q = 0; q < kLevels * kPitches && !held; q++) {
        const float pitch = set.pitches[q / kLevels];
        if (out.proofs >= set.max_proofs) break;
        NavEscape e;
        e.from = at_wall;
        e.yaw = wrap(toward + 180);
        e.pitch = pitch;
        e.fire_level = static_cast<int8_t>(kLevels - q % kLevels);
        float dmg = 0;
        out.proofs++;
        const NavProof pr = prove_escape(world, move, e, g.set, set.rj, &dmg);
        if (!pr.reached) continue;
        // Landed standing on a node that may be landed on (near it, not a node far off the search
        // found nearest).
        const int32_t at = nav_nearest(g, pr.end);
        if (at < 0 || !land[static_cast<size_t>(at)]) continue;
        const Vec3f on = g.nodes[static_cast<size_t>(at)].origin;
        if (across(on, pr.end) > kLandNear || on.z - pr.end.z > kLandNear ||
            pr.end.z - on.z > kLandNear)
          continue;
        e.to = at;
        e.damage = dmg;
        e.time_s = pr.time_s;
        g.escapes.push_back(e);
        held = true;
        out.held++;
      }
    }
  }
  // Each liquid node's links to the escapes it swims to straight, the cheapest a landing.
  std::vector<NavExtraLink> links;
  struct Pick {
    float cost;
    int32_t escape;
  };
  std::vector<Pick> picks;
  for (size_t n = 0; n < nodes; n++) {
    if (!liquid[n]) continue;
    const Vec3f o = g.nodes[n].origin;
    picks.clear();
    for (size_t x = first_escape; x < g.escapes.size(); x++) {
      const NavEscape& e = g.escapes[x];
      if (e.to == static_cast<int32_t>(n)) continue;
      const float d = length(e.from - o);
      if (d > set.reach) continue;
      const HullTrace tr = player_trace(world, o, e.from);
      if (tr.startsolid || tr.fraction < 1) continue;
      // The swim to the takeoff, then the proof's backing and flight.
      const float cost = d / set.swim_speed + e.time_s + set.cost_s;
      bool replaced = false;
      for (Pick& pk : picks)
        if (g.escapes[static_cast<size_t>(pk.escape)].to == e.to) {
          if (cost < pk.cost) pk = {cost, static_cast<int32_t>(x)};
          replaced = true;
        }
      if (!replaced) picks.push_back({cost, static_cast<int32_t>(x)});
    }
    std::sort(picks.begin(), picks.end(), [](const Pick& a, const Pick& b) {
      return a.cost != b.cost ? a.cost < b.cost : a.escape < b.escape;
    });
    for (size_t k = 0; k < picks.size() && static_cast<int>(k) < set.per_node; k++) {
      const NavEscape& e = g.escapes[static_cast<size_t>(picks[k].escape)];
      NavExtraLink l{static_cast<int32_t>(n), e.to, NavKind::rocket, picks[k].cost};
      l.pitch = e.pitch;
      l.escape = picks[k].escape;
      links.push_back(l);
    }
  }
  out.links = static_cast<int64_t>(links.size());
  nav_add_links(g, links);
  return out;
}

}  // namespace ge::shooter
