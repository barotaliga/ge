// A real map's nav for the bots (map_nav.hpp).
//
// ge:summary A real map's nav: the doors and plats off the entity text, teleporters, the water
// ge:keywords build_map_nav map_movers teleport-links open-doors-as-floor swim-field
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_team_test.cpp
#include "shooterbots/map_nav.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <tuple>
#include <unordered_map>
#include <utility>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/hull.hpp"
#include "shooter/map_spec.hpp"
#include "shooter/pmove.hpp"
#include "shooter/world.hpp"
#include "shooterbots/entities.hpp"
#include "shooterbots/nav_script.hpp"

namespace ge::shooterbots {

namespace {

namespace sh = ge::shooter;

namespace sb = ge::shooterbots;

constexpr float kHatchDrop = 64;    // a floor door's drop: the floor under it this far down or more
constexpr float kHatchAcross = 24;  // and the node under within this across, units
constexpr float kOwnDrop = 24;  // a bridge's own node: no floor of the map's within this under it

// A model's box, either family: [0] the world, then the inline models ("*N").
struct ModelBox {
  bool ok = false;
  sh::Vec3f mins, maxs;
};

size_t model_count(const ge::shooter::MapSpec& spec) {
  return spec.brushes ? spec.brushes->models.size() : spec.models.size();
}

ModelBox model_box(const ge::shooter::MapSpec& spec, int m) {
  ModelBox b;
  if (m < 0 || static_cast<size_t>(m) >= model_count(spec)) return b;
  b.ok = true;
  if (spec.brushes) {
    const ge::brush::Model& bm = spec.brushes->models[static_cast<size_t>(m)];
    b.mins = {bm.mins[0], bm.mins[1], bm.mins[2]};
    b.maxs = {bm.maxs[0], bm.maxs[1], bm.maxs[2]};
    // The world's box as the map's faces reach, a little out: a map may give its world a box far
    // past its walls (fact3's is 99999 every way), which would grow the walk and swim grids past
    // memory.
    const std::vector<ge::brush::Vertex>& vs = spec.brushes->vertices;
    if (m == 0 && !vs.empty()) {
      sh::Vec3f lo = {vs[0].pos[0], vs[0].pos[1], vs[0].pos[2]}, hi = lo;
      for (const ge::brush::Vertex& v : vs)
        for (int i = 0; i < 3; i++) {
          lo[i] = std::min(lo[i], v.pos[i]);
          hi[i] = std::max(hi[i], v.pos[i]);
        }
      for (int i = 0; i < 3; i++) {
        b.mins[i] = std::max(b.mins[i], lo[i] - 32);
        b.maxs[i] = std::min(b.maxs[i], hi[i] + 32);
      }
    }
  } else {
    b.mins = spec.models[static_cast<size_t>(m)].mins;
    b.maxs = spec.models[static_cast<size_t>(m)].maxs;
  }
  return b;
}

// Inline model `m` as a solid of the nav's world standing at `origin`.
sh::Solid model_solid(const ge::shooter::MapSpec& spec, ge::brush::BrushTracer* tracer, int m,
                      sh::Vec3f origin) {
  const ModelBox box = model_box(spec, m);
  if (spec.brushes && tracer != nullptr)
    return sh::brushes_solid(*tracer, static_cast<uint32_t>(m), origin, box.mins, box.maxs);
  sh::Solid so;
  so.kind = sh::SolidKind::brush;
  so.origin = origin;
  if (spec.brushes) return so;  // no tracer: nothing to trace (never built so)
  const sh::BrushModel& bm = spec.models[static_cast<size_t>(m)];
  so.point_hull = bm.hulls[0];
  so.clip_hull = bm.hulls[1];
  so.mins = bm.mins;
  so.maxs = bm.maxs;
  return so;
}

// A door that starts open (spawnflags 1) stays where it spawned until a button moves it: a plate
// over a pit, a wall drawn aside.
bool start_open(const sb::EntityBlock& b) {
  const char* sf = sb::block_value(b, "spawnflags");
  return sf && (std::atoi(sf) & 1) != 0;
}

constexpr int kDoorToggle = 32;  // a door's flag: it stays where a press sends it
constexpr float kSinkWide = 64;  // a sinking floor's top this wide each way is stood on, units

// Whether door `b` (model box `bm`) is a floor that sinks or rises and comes back: named (a
// button or a trigger moves it), straight down or up, its top wide enough to stand on, not
// started open, not a toggle, and back after its wait.
bool sink_lift(const sb::EntityBlock& b, const ModelBox& bm) {
  const char* an = sb::block_value(b, "angle");
  const char* wt = sb::block_value(b, "wait");
  if (sb::block_value(b, "targetname") == nullptr || an == nullptr || start_open(b)) return false;
  const int angle = std::atoi(an);
  if (angle != -1 && angle != -2) return false;
  if (wt != nullptr && std::atof(wt) < 0) return false;
  const char* sf = sb::block_value(b, "spawnflags");
  if (sf && (std::atoi(sf) & kDoorToggle) != 0) return false;  // stays where it went
  return bm.maxs.x - bm.mins.x >= kSinkWide && bm.maxs.y - bm.mins.y >= kSinkWide;
}

// The edict the mod made of inline model `model`, or -1.
int edict_of(const ge::shooterbots::BotView& s, const char* model) {
  for (int e = s.sv.set.maxclients + 1; e < s.sv.num_edicts; e++) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
    if (d.free || d.modelindex <= 0 || static_cast<size_t>(d.modelindex) >= s.models.size())
      continue;
    if (s.models[static_cast<size_t>(d.modelindex)] == model) return e;
  }
  return -1;
}

// Edict `e`'s box in the world: its own, else (a runner whose rows carry no box for a brush
// entity: Quake II's) inline model `model`'s box where the edict stands.
struct AbsBox {
  sh::Vec3f min, max;
};

AbsBox abs_box(const ge::shooterbots::BotView& s, const ge::shooter::MapSpec& spec, int e,
               const char* model) {
  const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
  if (d.absmax.x > d.absmin.x || d.absmax.y > d.absmin.y || d.absmax.z > d.absmin.z || !model ||
      model[0] != '*')
    return {d.absmin, d.absmax};
  const ModelBox m = model_box(spec, std::atoi(model + 1));
  if (!m.ok) return {d.absmin, d.absmax};
  return {d.origin + m.mins, d.origin + m.maxs};
}

// A turning plate this thick or less is a bridge (base2's is 40 units), units.
constexpr float kTurnPlate = 48;
constexpr float kSlabThick = 128;  // a slab that slides shut into a bridge, at most this thick

bool liquid(int contents) {
  return contents == sh::kContentsWater || contents == sh::kContentsSlime ||
         contents == sh::kContentsLava;
}

// The plain walls standing in the map (a func_wall nothing names: nothing turns it off or takes it
// away) as solids of the nav's world: left out, its links ran through them (bunk1's walls that seal
// its deathmatch shortcuts in single player sent him into one for minutes). A wall with a name is
// the way's to bar until it is gone (way_through's gates).
void add_walls(const ge::shooterbots::BotView& s, const ge::shooter::MapSpec& spec,
               ge::brush::BrushTracer* tracer, const std::vector<sb::EntityBlock>& blocks,
               sh::MoveWorld& world) {
  for (const sb::EntityBlock& b : blocks) {
    const char* cn = sb::block_value(b, "classname");
    const char* model = sb::block_value(b, "model");
    if (!cn || std::strcmp(cn, "func_wall") != 0 || !model || model[0] != '*' ||
        sb::block_value(b, "targetname") != nullptr)
      continue;
    const int m = std::atoi(model + 1);
    if (m <= 0 || static_cast<size_t>(m) >= model_count(spec) || world.count >= sh::kMaxSolids)
      continue;
    const int e = edict_of(s, model);  // none when the mode leaves it out
    if (e < 0) continue;
    world.solids[world.count++] =
        model_solid(spec, tracer, m, s.sv.edicts[static_cast<size_t>(e)].origin);
  }
}

// Those doors as solids of the nav's world, where they stand (the rest of the movers are left
// out of it and tagged on the links they close).
// A plate: no thicker than a step's height, wider than a player both ways.
bool plate(const ModelBox& bm) {
  const sh::Vec3f d = bm.maxs - bm.mins;
  return d.z <= 32 && d.x >= 48 && d.y >= 48;
}

// The doors (not starting open) whose chain a name or a shot moves: a door with no name of its own
// moves with every door its box touches (unless one of them keeps apart, spawnflags 4), and the
// chain opens only by its name when any door in it has one. Edicts, by edict number.
std::vector<uint8_t> named_doors(const ge::shooterbots::BotView& s,
                                 const ge::shooter::MapSpec& spec,
                                 const std::vector<sb::EntityBlock>& blocks) {
  struct D {
    int e;
    bool named, apart;
    AbsBox box;
  };
  std::vector<D> ds;
  for (const sb::EntityBlock& b : blocks) {
    const char* cn = sb::block_value(b, "classname");
    const char* model = sb::block_value(b, "model");
    if (!cn || !model || model[0] != '*' || std::strcmp(cn, "func_door") != 0 || start_open(b))
      continue;
    const int e = edict_of(s, model);
    if (e < 0) continue;
    const char* sf = sb::block_value(b, "spawnflags");
    ds.push_back({e, sb::block_value(b, "targetname") != nullptr || sb::block_value(b, "health"),
                  sf && (std::atoi(sf) & 4) != 0, abs_box(s, spec, e, model)});
  }
  std::vector<uint8_t> out(static_cast<size_t>(s.sv.num_edicts), 0);
  // Spread the name along touching boxes until nothing changes (chains are a few doors long).
  for (bool grew = true; grew;) {
    grew = false;
    for (D& a : ds)
      for (const D& b : ds) {
        if (a.named || !b.named || a.apart || b.apart) continue;
        const AbsBox& x = a.box;
        const AbsBox& y = b.box;
        if (x.min.x <= y.max.x && y.min.x <= x.max.x && x.min.y <= y.max.y && y.min.y <= x.max.y &&
            x.min.z <= y.max.z && y.min.z <= x.max.z) {
          a.named = true;
          grew = true;
        }
      }
  }
  for (const D& d : ds)
    if (d.named) out[static_cast<size_t>(d.e)] = 1;
  return out;
}

// A FLOOR DOOR: a plate resting shut that only a button (or a shot) moves: a floor to the nav at
// rest, its nodes shut while it is away (dm2's big room and its entries, slid out by t1).
bool floor_door(const sb::EntityBlock& b, const ModelBox& bm, const std::vector<uint8_t>& named,
                int e) {
  const char* cn = sb::block_value(b, "classname");
  return cn && std::strcmp(cn, "func_door") == 0 && !start_open(b) && plate(bm) && e >= 0 &&
         static_cast<size_t>(e) < named.size() && named[static_cast<size_t>(e)];
}

// A SLIDE-OUT BRIDGE: a named plate drawn resting in its slot (not started open), sliding level
// (an angle of 0, 90, 180 or 270) to where it spans a gap, with room to stand on it there and a
// floor under where it rests (bunk1's over the hole between its two floors, run out by the button
// on the walkway above). Its travel in `move` (a door's: across its own size less its lip).
bool slides_out(const sb::EntityBlock& b, const ModelBox& bm, const sh::MoveWorld& world,
                sh::Vec3f& move) {
  const char* cn = sb::block_value(b, "classname");
  const char* an = sb::block_value(b, "angle");
  if (!cn || std::strcmp(cn, "func_door") != 0 || start_open(b) || !an ||
      !sb::block_value(b, "targetname") || !plate(bm))
    return false;
  const int angle = std::atoi(an);
  const sh::Vec3f dir = angle == 0     ? sh::Vec3f{1, 0, 0}
                        : angle == 90  ? sh::Vec3f{0, 1, 0}
                        : angle == 180 ? sh::Vec3f{-1, 0, 0}
                        : angle == 270 ? sh::Vec3f{0, -1, 0}
                                       : sh::Vec3f{};
  if (dir.x == 0 && dir.y == 0) return false;
  const char* lp = sb::block_value(b, "lip");
  const float lip = lp ? static_cast<float>(std::atof(lp)) : 8;
  const sh::Vec3f size = bm.maxs - bm.mins;
  const float travel = std::fabs(dir.x) * size.x + std::fabs(dir.y) * size.y - lip;
  if (travel <= 0) return false;
  move = dir * travel;
  const sh::Vec3f rest{(bm.mins.x + bm.maxs.x) * 0.5f, (bm.mins.y + bm.maxs.y) * 0.5f, 0};
  const sh::Vec3f out = rest + move;
  return !sh::box_free(world, {rest.x, rest.y, bm.mins.z - 25}) &&
         sh::box_free(world, {out.x, out.y, bm.maxs.z + 25}) &&
         sh::box_free(world, {out.x, out.y, bm.mins.z - 25});
}

void open_doors_as_floor(const ge::shooterbots::BotView& s, const ge::shooter::MapSpec& spec,
                         ge::brush::BrushTracer* tracer, const std::vector<sb::EntityBlock>& blocks,
                         sh::MoveWorld& world, std::vector<NavBridge>& bridges) {
  const std::vector<uint8_t> named = named_doors(s, spec, blocks);
  for (const sb::EntityBlock& b : blocks) {
    const char* model = sb::block_value(b, "model");
    if (!model || model[0] != '*') continue;
    const int m = std::atoi(model + 1);
    if (m <= 0 || static_cast<size_t>(m) >= model_count(spec) || world.count >= sh::kMaxSolids)
      continue;
    const int e = edict_of(s, model);
    const ModelBox bm = model_box(spec, m);
    sh::Vec3f move;
    if (e >= 0 && slides_out(b, bm, world, move)) {
      const sh::Vec3f rest = s.sv.edicts[static_cast<size_t>(e)].origin;
      world.solids[world.count++] = model_solid(spec, tracer, m, rest + move);  // out
      NavBridge br;
      br.ent = e;
      br.spawn = rest;
      br.out_at = rest + move;
      const char* wait = sb::block_value(b, "wait");
      br.wait = wait ? static_cast<float>(std::atof(wait)) : 3;  // a door's default
      bridges.push_back(br);
      continue;
    }
    if (!floor_door(b, bm, named, e)) continue;
    sh::Solid& so = world.solids[world.count++];
    so = model_solid(spec, tracer, m, s.sv.edicts[static_cast<size_t>(e)].origin);  // at rest
    NavBridge br;
    br.ent = e;
    br.spawn = so.origin;
    br.floor = true;
    bridges.push_back(br);
  }
  // The turning doors that start open and are plates where the map drew them: a floor there,
  // turned up out of it until used.
  for (const sb::EntityBlock& b : blocks) {
    const char* cn = sb::block_value(b, "classname");
    const char* model = sb::block_value(b, "model");
    if (!cn || !model || model[0] != '*' || std::strcmp(cn, "func_door_rotating") != 0 ||
        !start_open(b) || !sb::block_value(b, "targetname"))
      continue;
    const int m = std::atoi(model + 1);
    if (m <= 0 || static_cast<size_t>(m) >= model_count(spec) || world.count >= sh::kMaxSolids)
      continue;
    const int e = edict_of(s, model);
    if (e < 0) continue;
    const ModelBox bm = model_box(spec, m);
    const sh::Vec3f d = bm.maxs - bm.mins;
    if (d.z > kTurnPlate || d.x < 48 || d.y < 48) continue;
    const sh::Edict& de = s.sv.edicts[static_cast<size_t>(e)];
    world.solids[world.count++] = model_solid(spec, tracer, m, de.origin);  // as drawn
    NavBridge br;
    br.ent = e;
    br.spawn = de.origin;
    br.turns = true;
    br.spawn_angles = de.angles;
    bridges.push_back(br);
  }
  // The doors that start open, by where the map drew them: a name spreads along touching boxes
  // (doors link as drawn: dm2's two plates over the lava move together off one button).
  std::vector<std::pair<ModelBox, bool>> open_doors;
  for (const sb::EntityBlock& b : blocks) {
    const char* cn = sb::block_value(b, "classname");
    const char* model = sb::block_value(b, "model");
    if (!cn || !model || model[0] != '*' || std::strcmp(cn, "func_door") != 0 || !start_open(b))
      continue;
    const int m = std::atoi(model + 1);
    if (m <= 0 || static_cast<size_t>(m) >= model_count(spec)) continue;
    open_doors.push_back({model_box(spec, m), sb::block_value(b, "targetname") != nullptr});
  }
  for (bool grew = true; grew;) {
    grew = false;
    for (auto& [x, xn] : open_doors)
      for (const auto& [y, yn] : open_doors)
        if (!xn && yn && x.mins.x <= y.maxs.x + 2 && y.mins.x <= x.maxs.x + 2 &&
            x.mins.y <= y.maxs.y + 2 && y.mins.y <= x.maxs.y + 2 && x.mins.z <= y.maxs.z + 2 &&
            y.mins.z <= x.maxs.z + 2) {
          xn = true;
          grew = true;
        }
  }
  size_t open_k = 0;
  for (const sb::EntityBlock& b : blocks) {
    const char* cn = sb::block_value(b, "classname");
    const char* model = sb::block_value(b, "model");
    if (!cn || !model || model[0] != '*' || std::strcmp(cn, "func_door") != 0 || !start_open(b))
      continue;
    const int m = std::atoi(model + 1);
    if (m > 0 && static_cast<size_t>(m) < model_count(spec)) open_k++;
    if (m <= 0 || static_cast<size_t>(m) >= model_count(spec) || world.count >= sh::kMaxSolids)
      continue;
    const int e = edict_of(s, model);
    if (e < 0) continue;
    const ModelBox bm = model_box(spec, m);
    sh::Solid& so = world.solids[world.count++];
    so = model_solid(spec, tracer, m, s.sv.edicts[static_cast<size_t>(e)].origin);
    // A pillar that sinks to open and rises to shut, with room to stand on its top where the map
    // drew it: a bridge raised out of a pit (e2m6's, raised by the trigger round the start room's
    // pillar), as a plate slid out is.
    const char* angle = sb::block_value(b, "angle");
    const sh::Vec3f size = bm.maxs - bm.mins;
    const bool pillar = angle && std::atof(angle) == -2 && size.x >= 48 && size.y >= 48 &&
                        sh::box_free(world, {(bm.mins.x + bm.maxs.x) * 0.5f,
                                             (bm.mins.y + bm.maxs.y) * 0.5f, bm.maxs.z + 25});
    // A slab that slides shut sideways over a gap (nothing under where the map drew it, or a
    // liquid), with room to stand on: a bridge closed over the lava (e1m7's, shut by the boss's
    // death, the only way on to the exit).
    const sh::Vec3f mid{(bm.mins.x + bm.maxs.x) * 0.5f, (bm.mins.y + bm.maxs.y) * 0.5f, 0};
    const bool slab = angle && std::atof(angle) >= 0 && size.x >= 48 && size.y >= 48 &&
                      size.z <= kSlabThick && sh::box_free(world, {mid.x, mid.y, bm.maxs.z + 25}) &&
                      (sh::box_free(world, {mid.x, mid.y, bm.mins.z - 25}) ||
                       liquid(sh::water_contents(world, {mid.x, mid.y, bm.mins.z + 4})));
    if (plate(bm) || pillar || slab) {
      NavBridge br;
      br.ent = e;
      br.spawn = so.origin;
      br.touch = !open_doors[open_k - 1].second;  // no name of its own nor its group's
      const char* wait = sb::block_value(b, "wait");
      br.wait = wait ? static_cast<float>(std::atof(wait)) : 3;  // a door's default
      bridges.push_back(br);
      so.origin = {};  // out, where the map drew it
    }
  }
}

sh::Vec3f origin_of(const char* org);  // below

// (`add_crawlways`) A ducked box swept through the world's brushes.
constexpr sh::Vec3f kDuckMins{-16, -16, -24}, kDuckMaxs{16, 16, 4};
constexpr float kCrawlCell = 16;    // the crawl's grid, units
constexpr int kCrawlCells = 1500;   // cells one crawl visits at most
constexpr float kCrawlSpeed = 100;  // ducked, units/s (the move's own crawl, less a little)

sh::HullTrace duck_trace(const sh::Solid& w, sh::Vec3f a, sh::Vec3f b, sh::Vec3f mn, sh::Vec3f mx) {
  return sh::solid_box_trace(w, a, mn, mx, b);
}

// A crawl's step from `o` to the cell at (x, y): up a step, across ducked, down onto a floor.
bool crawl_step(const sh::Solid& w, sh::Vec3f o, float x, float y, sh::Vec3f& to) {
  o.z += 0.25f;  // off the floor it stands on (a box touching a brush starts in it)
  // Straight across first (a roof that slopes down takes no step up), then up a step and across.
  for (const float rise : {0.0f, 18.0f}) {
    sh::Vec3f u = o;
    if (rise > 0) {
      const sh::HullTrace up = duck_trace(w, o, {o.x, o.y, o.z + rise}, kDuckMins, kDuckMaxs);
      if (up.startsolid) return false;
      u = up.endpos;
      if (up.fraction < 1) u.z = std::max(o.z, u.z - 0.25f);  // off the roof it rose to
    }
    const sh::HullTrace across = duck_trace(w, u, {x, y, u.z}, kDuckMins, kDuckMaxs);
    if (across.startsolid || across.fraction < 1) continue;
    const sh::HullTrace down = duck_trace(w, across.endpos, {x, y, u.z - 64}, kDuckMins, kDuckMaxs);
    if (down.startsolid || down.fraction >= 1 || down.plane.normal.z < 0.7f) continue;
    to = down.endpos;
    return true;
  }
  return false;
}

bool stands_at(const sh::Solid& w, sh::Vec3f p) {
  p.z += 0.25f;
  return !duck_trace(w, p, p, sh::kPlayerMins, sh::kPlayerMaxs).startsolid;
}

}  // namespace

std::vector<sh::Vec3f> crawl_path(const MapCrawl& cr, int a, int b) {
  std::vector<int> up_a, up_b;
  for (int k = a; k >= 0; k = cr.cells[static_cast<size_t>(k)].parent) up_a.push_back(k);
  for (int k = b; k >= 0; k = cr.cells[static_cast<size_t>(k)].parent) up_b.push_back(k);
  while (up_a.size() > 1 && up_b.size() > 1 && up_a[up_a.size() - 2] == up_b[up_b.size() - 2]) {
    up_a.pop_back();
    up_b.pop_back();
  }
  std::vector<sh::Vec3f> out;
  out.reserve(up_a.size() + up_b.size());
  for (const int k : up_a) out.push_back(cr.cells[static_cast<size_t>(k)].at);
  for (size_t k = up_b.size() - 1; k-- > 0;)
    out.push_back(cr.cells[static_cast<size_t>(up_b[k])].at);
  return out;
}

int add_crawlways(const sh::Solid& w, sb::NavGraph& g, std::vector<MapCrawl>* keep) {
  // One crawl (a region under the roof) is searched once, as a tree from where it was first
  // entered; a node that steps into it is joined through the tree to a node it comes out by.
  using Crawl = MapCrawl;
  const auto key = [](sh::Vec3f p) {
    const auto q = [](float v) {
      return static_cast<uint64_t>(static_cast<int64_t>(std::floor(v / 8)) & 0x1fffff);
    };
    return q(p.x) | q(p.y) << 21 | q(p.z) << 42;
  };
  std::vector<Crawl> crawls;
  std::unordered_map<uint64_t, std::pair<int, int>> where;  // cell -> (crawl, cell)
  std::vector<sb::NavExtraLink> links;
  std::vector<std::pair<int32_t, int32_t>> made;
  constexpr float kDirs[8][2] = {{1, 0}, {-1, 0}, {0, 1},  {0, -1},
                                 {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
  const auto search = [&](sh::Vec3f entry) {
    Crawl cr;
    cr.cells.push_back({entry, -1});
    const int id = static_cast<int>(crawls.size());
    where[key(entry)] = {id, 0};
    for (size_t c = 0; c < cr.cells.size() && cr.cells.size() < kCrawlCells; c++) {
      const sh::Vec3f o = cr.cells[c].at;
      if (stands_at(w, o)) {  // out from under the roof: the crawl ends here, by a node
        const int32_t near = sb::nav_nearest(g, o);
        if (near >= 0) {
          const sh::Vec3f no = g.nodes[static_cast<size_t>(near)].origin;
          const float dx = no.x - o.x, dy = no.y - o.y, dz = no.z - o.z;
          if (dx * dx + dy * dy <= 56 * 56 && dz <= 20 && dz >= -20)
            cr.ends.push_back({static_cast<int>(c), near});
        }
        continue;
      }
      for (int k = 0; k < 4; k++) {
        sh::Vec3f to;
        if (!crawl_step(w, o, o.x + kDirs[k][0] * kCrawlCell, o.y + kDirs[k][1] * kCrawlCell, to))
          continue;
        const uint64_t kk = key(to);
        if (where.count(kk) != 0) continue;
        where[kk] = {id, static_cast<int>(cr.cells.size())};
        cr.cells.push_back({to, static_cast<int>(c)});
      }
    }
    crawls.push_back(std::move(cr));
    return id;
  };
  const auto one = [&](int32_t a, int32_t b, const std::vector<sh::Vec3f>& way) {
    if (std::find(made.begin(), made.end(), std::make_pair(a, b)) != made.end()) return;
    sb::NavScript sc;
    float len = 0;
    sh::Vec3f last = g.nodes[static_cast<size_t>(a)].origin;
    for (size_t k = 0; k < way.size(); k += 2) {
      sc.steps.push_back({way[k], false, true});
      const sh::Vec3f dd = way[k] - last;
      len += std::sqrt(dd.x * dd.x + dd.y * dd.y);
      last = way[k];
    }
    sc.steps.push_back({g.nodes[static_cast<size_t>(b)].origin, false, false});
    sb::NavExtraLink e{a, b, sb::NavKind::script, 0.5f + len / kCrawlSpeed};
    e.script = static_cast<int32_t>(g.scripts.size());
    g.scripts.push_back(std::move(sc));
    links.push_back(e);
    made.push_back({a, b});
  };
  // The places the graph already joins (undirected): a crawl is a way only between two of them.
  const size_t nodes = g.nodes.size();
  std::vector<int32_t> comp(nodes);
  for (size_t n = 0; n < nodes; n++) comp[n] = static_cast<int32_t>(n);
  const auto root = [&](int32_t a) {
    while (comp[static_cast<size_t>(a)] != a)
      a = comp[static_cast<size_t>(a)] = comp[static_cast<size_t>(comp[static_cast<size_t>(a)])];
    return a;
  };
  for (size_t n = 0; n < nodes; n++) {
    const sb::NavNode& nn = g.nodes[n];
    for (int32_t k = nn.first_link; k < nn.first_link + nn.links; k++) {
      const int32_t a = root(static_cast<int32_t>(n)), b = root(g.links[static_cast<size_t>(k)].to);
      if (a != b) comp[static_cast<size_t>(a)] = b;
    }
  }
  // The best way a crawl gives between two places: (crawl, from place, to place) -> the pair.
  struct Way {
    int32_t a, b;
    int ca, cb;
    size_t len;
  };
  std::vector<std::pair<std::tuple<int, int32_t, int32_t>, Way>> best;
  const auto snap = [](float v) { return std::round(v / kCrawlCell) * kCrawlCell; };
  for (size_t n = 0; n < nodes; n++) {
    const sb::NavNode& from = g.nodes[n];
    if (from.swim || from.mover >= 0 || from.ride >= 0) continue;
    for (const auto& d : kDirs) {
      // The first cell of the crawl's grid along the way where he no longer stands.
      sh::Vec3f entry;
      bool found = false;
      for (float r = 16; r <= 112 && !found; r += 8) {
        const float x = snap(from.origin.x + d[0] * r), y = snap(from.origin.y + d[1] * r);
        if (!crawl_step(w, from.origin, x, y, entry)) break;
        found = !stands_at(w, entry);
      }
      if (!found) continue;
      int id, cell;
      if (const auto it = where.find(key(entry)); it != where.end()) {
        id = it->second.first;
        cell = it->second.second;
      } else {
        id = search(entry);
        cell = 0;
      }
      const Crawl& cr = crawls[static_cast<size_t>(id)];
      const int32_t pa = root(static_cast<int32_t>(n));
      for (const auto& [c, end] : cr.ends) {
        const int32_t pb = root(end);
        if (pa == pb) continue;
        const size_t len = crawl_path(cr, cell, c).size();
        const auto k = std::make_tuple(id, pa, pb);
        auto it =
            std::find_if(best.begin(), best.end(), [&](const auto& e) { return e.first == k; });
        if (it == best.end())
          best.push_back({k, {static_cast<int32_t>(n), end, cell, c, len}});
        else if (len < it->second.len)
          it->second = {static_cast<int32_t>(n), end, cell, c, len};
      }
    }
  }
  for (const auto& [k, wy] : best) {
    const Crawl& cr = crawls[static_cast<size_t>(std::get<0>(k))];
    std::vector<sh::Vec3f> path = crawl_path(cr, wy.ca, wy.cb);
    one(wy.a, wy.b, path);
    std::reverse(path.begin(), path.end());
    one(wy.b, wy.a, path);
  }
  sb::nav_add_links(g, links);
  if (keep != nullptr) *keep = std::move(crawls);
  return static_cast<int>(links.size());
}

namespace {

std::vector<sb::NavMover> movers_of(const ge::shooterbots::BotView& s,
                                    const ge::shooter::MapSpec& spec,
                                    ge::brush::BrushTracer* tracer,
                                    const std::vector<sb::EntityBlock>& blocks) {
  std::vector<sb::NavMover> out;
  const std::vector<uint8_t> named = named_doors(s, spec, blocks);
  for (const sb::EntityBlock& b : blocks) {
    const char* cn = sb::block_value(b, "classname");
    const char* model = sb::block_value(b, "model");
    if (!cn || !model || model[0] != '*') continue;
    const bool door = std::strcmp(cn, "func_door") == 0 || std::strcmp(cn, "func_door_secret") == 0;
    const bool plat = std::strcmp(cn, "func_plat") == 0;
    const bool train = std::strcmp(cn, "func_train") == 0;
    if (!door && !plat && !train) continue;
    if (door && start_open(b)) continue;  // a floor to the nav (open_doors_as_floor)
    const int m = std::atoi(model + 1);
    if (m <= 0 || static_cast<size_t>(m) >= model_count(spec)) continue;
    const int ent = edict_of(s, model);
    const ModelBox bm = model_box(spec, m);
    if (door && floor_door(b, bm, named, ent)) continue;  // too
    if (ent < 0) continue;
    sb::NavMover mv;
    mv.ent = ent;
    const AbsBox ab = abs_box(s, spec, ent, model);
    mv.absmin = ab.min;
    mv.absmax = ab.max;
    if (plat && (!spec.brushes || tracer != nullptr)) {
      // Drawn raised and lowered by its height (else its own height less its lip, 8 units unless
      // it says: base2's lift sets 132, and taken as 8 its top went past the floor above, so no
      // ride up was found); Quake II's default speed is 200 units/s, Quake's 150. Quake's plat
      // reads no lip (always 8 under its height: e2m5's pit lift says 2, and taken as 2 its top
      // stood 6 over the nook it rises to, its rider's head in the nook's ceiling, no way off).
      const char* h = sb::block_value(b, "height");
      const char* sp = sb::block_value(b, "speed");
      const char* lp = spec.brushes ? sb::block_value(b, "lip") : nullptr;
      const float lip = lp ? static_cast<float>(std::atof(lp)) : 8.f;
      const float rise = h ? static_cast<float>(std::atof(h)) : (bm.maxs.z - bm.mins.z) - lip;
      const float fallback = spec.brushes ? 200.f : 150.f;
      const float speed = sp ? static_cast<float>(std::atof(sp)) : fallback;
      if (spec.brushes) {
        mv.tracer = tracer;
        mv.brush_model = static_cast<uint32_t>(m);
      } else {
        mv.model = &spec.models[static_cast<size_t>(m)];
      }
      mv.rest = s.sv.edicts[static_cast<size_t>(ent)].origin;  // lowered from where it was built
      // A named one waits raised until something fires it, then lowers and rides like any other
      // (bunk1's by its first buttons): its rest is down there, where it stands from then on.
      if (sb::block_value(b, "targetname") != nullptr && mv.rest.z > -rise / 2) {
        mv.rest.z -= rise;
        mv.absmin.z -= rise;
        mv.absmax.z -= rise;
      }
      mv.travel = {0, 0, rise};
      mv.ride_s = 1 + rise / (speed > 0 ? speed : fallback);
    }
    // A car whose own button rides inside it (base1's exit) is a hollow door, not a floor.
    bool car = false;
    for (const sb::EntityBlock& o : blocks) {
      const char* ocn = sb::block_value(o, "classname");
      const char* om = sb::block_value(o, "model");
      if (!door || !ocn || !om || om[0] != '*' || std::strcmp(ocn, "func_button") != 0) continue;
      const int k = std::atoi(om + 1);
      if (k <= 0 || static_cast<size_t>(k) >= model_count(spec)) continue;
      const ModelBox ob = model_box(spec, k);
      constexpr float kIn = 16;
      car = car || (ob.mins.x > bm.mins.x - kIn && ob.maxs.x < bm.maxs.x + kIn &&
                    ob.mins.y > bm.mins.y - kIn && ob.maxs.y < bm.maxs.y + kIn &&
                    ob.mins.z > bm.mins.z - kIn && ob.maxs.z < bm.maxs.z + kIn);
    }
    if (door && !car && sink_lift(b, bm) && (!spec.brushes || tracer != nullptr)) {
      // A FLOOR THAT SINKS (or rises) when its button is pressed and comes back after its wait
      // (bunk1's by its first hall, two seconds after the button beside it): ridden like a plat,
      // its whole height less its lip, at its speed (100 units/s unless it says).
      const char* sp = sb::block_value(b, "speed");
      const char* lp = sb::block_value(b, "lip");
      const char* an = sb::block_value(b, "angle");
      const float lip = lp ? static_cast<float>(std::atof(lp)) : 8.f;
      const float way = (bm.maxs.z - bm.mins.z) - lip;
      const float speed = sp ? static_cast<float>(std::atof(sp)) : 100.f;
      if (spec.brushes) {
        mv.tracer = tracer;
        mv.brush_model = static_cast<uint32_t>(m);
      } else {
        mv.model = &spec.models[static_cast<size_t>(m)];
      }
      // One that sinks is ridden up from where it sinks to (called down by its button, it
      // stands there its wait, then carries a rider back up): its rest is down there, like a
      // named plat's, so the nav boards it at the bottom and rides it to the top.
      mv.rest = s.sv.edicts[static_cast<size_t>(ent)].origin;
      if (an != nullptr && std::atoi(an) == -2) {
        mv.rest.z -= way;
        mv.absmin.z -= way;
        mv.absmax.z -= way;
      }
      mv.travel = {0, 0, way};
      mv.ride_s = 1 + way / (speed > 0 ? speed : 100.f);
    }
    if (train && (!spec.brushes || tracer != nullptr)) {
      // A MOVING PLATFORM: its path corners from its target round the chain, its model's lowest
      // corner set on each (as the rules place a train); 100 units/s unless it says, each
      // corner's wait.
      sh::Vec3f bmins = bm.mins;
      const char* tg = sb::block_value(b, "target");
      for (int guard = 0; tg != nullptr && guard < 32; guard++) {
        const sb::EntityBlock* corner = nullptr;
        for (const sb::EntityBlock& c : blocks) {
          const char* tn = sb::block_value(c, "targetname");
          if (tn != nullptr && std::strcmp(tn, tg) == 0) corner = &c;
        }
        if (corner == nullptr || sb::block_value(*corner, "origin") == nullptr) break;
        const sh::Vec3f at = origin_of(sb::block_value(*corner, "origin"));
        if (!mv.stops.empty() && guard > 0 && std::strcmp(tg, sb::block_value(b, "target")) == 0)
          break;  // round the chain
        mv.stops.push_back(at - bmins);
        const char* w = sb::block_value(*corner, "wait");
        mv.stop_wait_s.push_back(w ? std::max(0.f, static_cast<float>(std::atof(w))) : 0.f);
        tg = sb::block_value(*corner, "target");
      }
      if (mv.stops.size() >= 2) {
        if (spec.brushes) {
          mv.tracer = tracer;
          mv.brush_model = static_cast<uint32_t>(m);
        } else {
          mv.model = &spec.models[static_cast<size_t>(m)];
        }
        mv.rest = s.sv.edicts[static_cast<size_t>(ent)].origin;
        const char* sp = sb::block_value(b, "speed");
        mv.speed = sp ? static_cast<float>(std::atof(sp)) : 100.f;
      } else {
        mv.stops.clear();
        mv.stop_wait_s.clear();
      }
    }
    out.push_back(mv);
  }
  return out;
}

sh::Vec3f origin_of(const char* org) {
  char* end = nullptr;
  sh::Vec3f o;
  o.x = std::strtof(org, &end);
  o.y = std::strtof(end, &end);
  o.z = std::strtof(end, &end);
  return o;
}

// A teleporter's destination: the first block named `target` of a destination's class, lifted as
// its rules lift it (Quake's 27 units, Quake II's pad 10).
bool teleport_dest(const std::vector<sb::EntityBlock>& blocks, const char* target,
                   sh::Vec3f& dest) {
  for (const sb::EntityBlock& d : blocks) {
    const char* dn = sb::block_value(d, "targetname");
    const char* dc = sb::block_value(d, "classname");
    const char* org = sb::block_value(d, "origin");
    if (!dn || !dc || !org || std::strcmp(dn, target) != 0) continue;
    const bool first = std::strcmp(dc, "info_teleport_destination") == 0;
    const bool pad = std::strcmp(dc, "misc_teleporter_dest") == 0;
    if (!first && !pad) continue;
    dest = origin_of(org);
    dest.z += first ? 27.f : 10.f;
    return true;
  }
  return false;
}

std::vector<sb::NavExtraLink> teleport_links(sb::NavGraph& g, const ge::shooter::MapSpec& spec,
                                             const std::vector<sb::EntityBlock>& blocks) {
  std::vector<sb::NavExtraLink> out;
  for (const sb::EntityBlock& b : blocks) {
    const char* cn = sb::block_value(b, "classname");
    const char* target = sb::block_value(b, "target");
    if (!cn || !target || sb::block_value(b, "targetname")) continue;
    // Where it is touched: a trigger brush's box, or the box over a pad's centre.
    ModelBox box;
    if (std::strcmp(cn, "trigger_teleport") == 0) {
      const char* model = sb::block_value(b, "model");
      if (!model || model[0] != '*' || std::atoi(model + 1) <= 0) continue;
      box = model_box(spec, std::atoi(model + 1));
      if (!box.ok) continue;
    } else if (std::strcmp(cn, "misc_teleporter") == 0) {
      const char* org = sb::block_value(b, "origin");
      if (!org) continue;
      const sh::Vec3f o = origin_of(org);
      box.ok = true;
      box.mins = o + sh::Vec3f{-8, -8, 8};
      box.maxs = o + sh::Vec3f{8, 8, 24};
    } else {
      continue;
    }
    sh::Vec3f dest;
    if (!teleport_dest(blocks, target, dest)) continue;
    const int32_t to = sb::nav_nearest(g, dest);
    if (to < 0) continue;
    const size_t had = out.size();
    for (size_t a = 0; a < g.nodes.size(); a++) {
      const sh::Vec3f o = g.nodes[a].origin;
      if (o.x + 16 < box.mins.x || o.x - 16 > box.maxs.x || o.y + 16 < box.mins.y ||
          o.y - 16 > box.maxs.y || o.z + 32 < box.mins.z || o.z - 24 > box.maxs.z)
        continue;
      sb::NavExtraLink e{static_cast<int32_t>(a), to, sb::NavKind::walk, 0.1f};
      e.teleport = true;
      out.push_back(e);
    }
    if (out.size() > had) continue;
    // None touches it (a thin one on a pit's end wall, the last node a step short: e1m6's spike
    // pit held him for the whole map): a node of its own where the nearest node within
    // kTeleportReach, walked toward it, first touches it, linked both ways to that node.
    constexpr float kTeleportReach = 64;
    int32_t near = -1;
    float near_d = kTeleportReach;
    sh::Vec3f at;
    for (size_t a = 0; a < g.nodes.size(); a++) {
      const sh::Vec3f o = g.nodes[a].origin;
      if (o.z + 32 < box.mins.z || o.z - 24 > box.maxs.z) continue;
      const sh::Vec3f p{std::clamp(o.x, box.mins.x - 15, box.maxs.x + 15),
                        std::clamp(o.y, box.mins.y - 15, box.maxs.y + 15), o.z};
      const float dx = p.x - o.x, dy = p.y - o.y;
      const float d = std::sqrt(dx * dx + dy * dy);
      if (d < near_d) {
        near = static_cast<int32_t>(a);
        near_d = d;
        at = p;
      }
    }
    if (near < 0) continue;
    sb::NavNode pad = g.nodes[static_cast<size_t>(near)];
    pad.origin = at;
    pad.first_link = pad.links = 0;
    const auto id = static_cast<int32_t>(g.nodes.size());
    g.nodes.push_back(pad);
    out.push_back({near, id, sb::NavKind::walk, near_d / 320 + 0.05f});
    out.push_back({id, near, sb::NavKind::walk, near_d / 320 + 0.05f});
    sb::NavExtraLink e{id, to, sb::NavKind::walk, 0.1f};
    e.teleport = true;
    out.push_back(e);
  }
  return out;
}

// A push's ways (`trigger_push`): from each node in its brush (walked into, as a teleporter's) to
// the node where it throws him: up (angle -1), the nearest node standing over its top within
// kPushLand across; across (a heading), the nearest beyond its far face. e3m5 starts under one
// ("Jump up here"), the only way out of its first room.
std::vector<sb::NavExtraLink> push_links(const sb::NavGraph& g, const ge::shooter::MapSpec& spec,
                                         const std::vector<sb::EntityBlock>& blocks,
                                         const sh::MoveWorld& world, const sh::MoveSettings& move) {
  constexpr float kPushLand = 256;
  constexpr float kPushOver = 160;   // its landing at most this far over its top, units
  constexpr float kPushJump = 44;    // a standing jump's rise (its apex near 45), near enough
  constexpr float kPushMiddle = 20;  // an updraft's middle: this far in from its sides, units
  constexpr float kPushDown = 64;    // a flight's landing node at most this far from where it lands
  constexpr float kStepHeightNav = 32;  // and within this of its height, units
  std::vector<sb::NavExtraLink> out;
  // Every push that is on from the start, and the velocity it gives whoever is in it (its speed
  // times ten along its angle; straight up for -1).
  struct Push {
    ModelBox box;
    sh::Vec3f v;
  };
  std::vector<Push> all;
  for (const sb::EntityBlock& b : blocks) {
    const char* cn = sb::block_value(b, "classname");
    if (!cn || std::strcmp(cn, "trigger_push") != 0 || sb::block_value(b, "targetname")) continue;
    const char* model = sb::block_value(b, "model");
    if (!model || model[0] != '*' || std::atoi(model + 1) <= 0) continue;
    const ModelBox box = model_box(spec, std::atoi(model + 1));
    if (!box.ok) continue;
    const char* av = sb::block_value(b, "angle");
    const char* sv = sb::block_value(b, "speed");
    const double angle = av ? std::atof(av) : 0;
    const float speed = 10 * (sv && std::atof(sv) > 0 ? static_cast<float>(std::atof(sv)) : 1000);
    const double a = angle * ge::core::kPi / 180;
    const sh::Vec3f v = angle == -1 ? sh::Vec3f{0, 0, speed}
                        : angle == -2
                            ? sh::Vec3f{0, 0, -speed}
                            : sh::Vec3f{static_cast<float>(ge::core::libm::cos(a)) * speed,
                                        static_cast<float>(ge::core::libm::sin(a)) * speed, 0};
    all.push_back({box, v});
  }
  const auto in_push = [&](sh::Vec3f o) {
    for (size_t k = 0; k < all.size(); k++) {
      const ModelBox& p = all[k].box;
      if (o.x + 16 > p.mins.x && o.x - 16 < p.maxs.x && o.y + 16 > p.mins.y &&
          o.y - 16 < p.maxs.y && o.z + 32 > p.mins.z && o.z - 24 < p.maxs.z)
        return static_cast<int>(k);
    }
    return -1;
  };
  // Where a player let go in push `self` at `start` comes down, carried on through every push he
  // flies into (e3m5's tube up into a draft south, into another, then a long fall onto the far
  // ledge, 800 units on): false when he falls back into `self` (an updraft's top, left by the
  // keys, not by the draft) or is still in the air after kFlight_s.
  constexpr int kFlightSteps = 400;
  constexpr float kMaxVelocity = 2000;  // sv_maxvelocity's default, units/s  // ~5 s of 13 ms moves
  const auto fly = [&](size_t self, sh::Vec3f start, sh::Vec3f& at, float& secs) {
    sh::MoveState st;
    st.origin = start;
    st.maxspeed = move.maxspeed;
    sh::MoveCmd c;
    c.msec = 13;
    bool out_of_self = false;
    for (int i = 0; i < kFlightSteps; i++) {
      const int in = in_push(st.origin);
      if (in >= 0) {
        if (static_cast<size_t>(in) == self && out_of_self) return false;
        st.velocity = all[static_cast<size_t>(in)].v;
        st.ground = -1;
      }
      // The server bounds every component to sv_maxvelocity: a push's 10000 units/s flies 2000.
      for (float* c3 : {&st.velocity.x, &st.velocity.y, &st.velocity.z})
        *c3 = std::clamp(*c3, -kMaxVelocity, kMaxVelocity);
      if (in < 0 || static_cast<size_t>(in) != self) out_of_self = true;
      sh::player_move(st, c, move, world);
      if (st.ground >= 0 && in_push(st.origin) < 0) {
        at = st.origin;
        secs = static_cast<float>(i + 1) * 0.013f;
        return true;
      }
    }
    return false;
  };
  for (const sb::EntityBlock& b : blocks) {
    const char* cn = sb::block_value(b, "classname");
    if (!cn || std::strcmp(cn, "trigger_push") != 0 || sb::block_value(b, "targetname")) continue;
    const char* model = sb::block_value(b, "model");
    if (!model || model[0] != '*' || std::atoi(model + 1) <= 0) continue;
    const ModelBox box = model_box(spec, std::atoi(model + 1));
    if (!box.ok) continue;
    const char* av = sb::block_value(b, "angle");
    const double angle = av ? std::atof(av) : 0;
    if (angle == -2) continue;  // down: no way anywhere new
    const bool up = angle == -1;
    const double a = angle * ge::core::kPi / 180;
    const float dx = up ? 0.0f : static_cast<float>(ge::core::libm::cos(a));
    const float dy = up ? 0.0f : static_cast<float>(ge::core::libm::sin(a));
    const sh::Vec3f mid = (box.mins + box.maxs) * 0.5f;
    // Where he leaves it.
    const sh::Vec3f exit = up ? sh::Vec3f{mid.x, mid.y, box.maxs.z}
                              : sh::Vec3f{mid.x + dx * (box.maxs.x - box.mins.x) * 0.5f,
                                          mid.y + dy * (box.maxs.y - box.mins.y) * 0.5f, mid.z};
    int32_t land = -1;
    float land_d = kPushLand;
    size_t self = 0;  // this push in `all`
    while (self + 1 < all.size() &&
           (all[self].box.mins.x != box.mins.x || all[self].box.mins.y != box.mins.y ||
            all[self].box.mins.z != box.mins.z))
      self++;
    // Flown: the node where the flight comes down.
    sh::Vec3f down{0, 0, 0};
    float flight_s = 0;
    const sh::Vec3f from = up ? sh::Vec3f{mid.x, mid.y, box.mins.z + 24} : mid;
    const bool flown = fly(self, from, down, flight_s);
    if (flown) {
      float best = kPushDown;
      for (size_t n = 0; n < g.nodes.size(); n++) {
        const sh::Vec3f o = g.nodes[n].origin;
        if (g.nodes[n].swim || o.z > down.z + kStepHeightNav || o.z < down.z - kStepHeightNav)
          continue;
        const float d =
            std::sqrt((o.x - down.x) * (o.x - down.x) + (o.y - down.y) * (o.y - down.y));
        if (d < best) {
          land = static_cast<int32_t>(n);
          best = d;
        }
      }
    }
    const bool guess = land < 0;  // not flown, or no floor where it comes down
    for (size_t n = 0; guess && n < g.nodes.size(); n++) {
      const sh::Vec3f o = g.nodes[n].origin;
      if (g.nodes[n].swim) continue;
      const bool inside = o.x + 16 > box.mins.x && o.x - 16 < box.maxs.x && o.y + 16 > box.mins.y &&
                          o.y - 16 < box.maxs.y && o.z + 32 > box.mins.z && o.z - 24 < box.maxs.z;
      if (inside) continue;
      // A floor up at its top: not one beside it below, nor one far over it (e3m5's air tube's
      // landing is the ledge at its top, not the roof a floor above).
      if (up && (o.z < box.maxs.z - 64 || o.z > box.maxs.z + kPushOver)) continue;
      const float ex = o.x - exit.x, ey = o.y - exit.y, ez = o.z - exit.z;
      const float d = std::sqrt(ex * ex + ey * ey + ez * ez);
      if (!up && ex * dx + ey * dy < 0) continue;  // ahead of its face only
      if (d < land_d) {
        land = static_cast<int32_t>(n);
        land_d = d;
      }
    }
    if (land < 0) continue;
    const float height = up ? box.maxs.z - box.mins.z : 0.0f;
    // An updraft is walked into from its middle: the walk's end is what he steers for on the way
    // up, and one by its edge held him under the lip of e3m5's tube top, the draft pinning him to
    // the roof beside the opening for the whole map. Its edge only when its middle holds none.
    const auto middle = [&](sh::Vec3f o) {
      return o.x >= box.mins.x + kPushMiddle && o.x <= box.maxs.x - kPushMiddle &&
             o.y >= box.mins.y + kPushMiddle && o.y <= box.maxs.y - kPushMiddle;
    };
    bool any_middle = false;
    for (size_t n = 0; up && n < g.nodes.size() && !any_middle; n++) {
      const sh::Vec3f o = g.nodes[n].origin;
      any_middle = !g.nodes[n].swim && static_cast<int32_t>(n) != land && middle(o) &&
                   o.z + 32 >= box.mins.z && o.z - 24 <= box.maxs.z;
    }
    for (size_t n = 0; n < g.nodes.size(); n++) {
      const sh::Vec3f o = g.nodes[n].origin;
      if (g.nodes[n].swim || static_cast<int32_t>(n) == land) continue;
      if (o.x + 16 < box.mins.x || o.x - 16 > box.maxs.x || o.y + 16 < box.mins.y ||
          o.y - 16 > box.maxs.y || o.z - 24 > box.maxs.z)
        continue;
      if (any_middle && o.z + 32 >= box.mins.z && !middle(o)) continue;
      if (o.z + 32 < box.mins.z) {
        // Under it, out of reach standing: a standing jump into it from a node well inside its
        // plan (e3m5's "Jump up here"), the push carrying him on to the landing.
        if (!up || o.z + 32 + kPushJump < box.mins.z || o.x < box.mins.x + 8 ||
            o.x > box.maxs.x - 8 || o.y < box.mins.y + 8 || o.y > box.maxs.y - 8)
          continue;
        sb::NavExtraLink e{static_cast<int32_t>(n), land, sb::NavKind::jump, 1.5f + height / 400};
        e.updraft = true;
        out.push_back(e);
        continue;
      }
      sb::NavExtraLink e{static_cast<int32_t>(n), land, sb::NavKind::walk, 1.0f + height / 400};
      e.teleport = true;  // walked into, then carried
      out.push_back(e);
    }
  }
  return out;
}

// THE LADDERS (`ladder_links`): every brush of ladder contents, as the box its axial sides
// bound, joined with the ladder brushes it touches into one ladder. A node standing or swimming
// beside it (from `kLadderFoot` under its foot to `kLadderMin` under its top; a ladder often runs
// on down past the floor it is climbed from), beside one face and within `kLadderBeside` of it,
// gets a link up to the nearest node standing at its top (from a step under the top to
// `kLadderAbove` over it, within `kLadderTop` across of the ladder): the climb the move makes
// facing the ladder with the view up, unproven by the nav's move (which has no ladders), timed at
// the climb's 200 units/s.
constexpr float kLadderBeside = 40, kLadderTop = 64, kLadderAbove = 64, kLadderFoot = 64;
constexpr float kLadderMin = 48;  // a ladder this tall or more is climbed (a lower one is a step)
constexpr int32_t kLadderContents = 0x20000000;

struct LadderBox {
  sh::Vec3f mins, maxs;
};

// The brushes in the game: the world's, and those of each brush model an edict of `s` wears (a
// ladder of a model the mode never spawns, base2's trench ladder for deathmatch only, is none).
std::vector<uint8_t> brushes_in_game(const ge::shooterbots::BotView& s, const ge::brush::Map& m) {
  std::vector<uint8_t> in(m.brushes.size(), 0);
  std::vector<uint8_t> models(m.models.size(), 0);
  if (!models.empty()) models[0] = 1;
  for (int e = 1; e < s.sv.num_edicts; e++) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
    if (d.free || d.modelindex <= 0 || static_cast<size_t>(d.modelindex) >= s.models.size())
      continue;
    const std::string& name = s.models[static_cast<size_t>(d.modelindex)];
    if (name.size() < 2 || name[0] != '*') continue;
    const size_t k = static_cast<size_t>(std::atoi(name.c_str() + 1));
    if (k < models.size()) models[k] = 1;
  }
  std::vector<int32_t> stack;
  for (size_t k = 0; k < models.size(); k++) {
    if (!models[k]) continue;
    stack.assign(1, m.models[k].head_node);
    while (!stack.empty()) {
      const int32_t at = stack.back();
      stack.pop_back();
      if (at >= 0) {
        if (static_cast<size_t>(at) >= m.nodes.size()) continue;
        stack.push_back(m.nodes[static_cast<size_t>(at)].child[0]);
        stack.push_back(m.nodes[static_cast<size_t>(at)].child[1]);
        continue;
      }
      const size_t li = static_cast<size_t>(-(at + 1));
      if (li >= m.leaves.size()) continue;
      const ge::brush::Leaf& lf = m.leaves[li];
      for (uint32_t j = 0; j < lf.brushes; j++) {
        const size_t bi = static_cast<size_t>(lf.first_brush) + j;
        if (bi < m.leaf_brushes.size() && m.leaf_brushes[bi] < in.size())
          in[m.leaf_brushes[bi]] = 1;
      }
    }
  }
  return in;
}

std::vector<LadderBox> ladder_boxes(const ge::brush::Map& m, const std::vector<uint8_t>& in) {
  std::vector<LadderBox> boxes;
  for (size_t bi = 0; bi < m.brushes.size(); bi++) {
    const ge::brush::Brush& b = m.brushes[bi];
    if (!(b.contents & kLadderContents) || !in[bi]) continue;
    LadderBox box{{-1e9f, -1e9f, -1e9f}, {1e9f, 1e9f, 1e9f}};
    for (uint32_t k = 0; k < b.sides; k++) {
      const size_t si = static_cast<size_t>(b.first_side) + k;
      if (si >= m.brush_sides.size()) break;
      const size_t pi = m.brush_sides[si].plane;
      if (pi >= m.planes.size()) continue;
      const ge::brush::Plane& pl = m.planes[pi];
      for (int i = 0; i < 3; i++) {
        if (pl.n[i] == 1) box.maxs[i] = std::min(box.maxs[i], pl.d);
        if (pl.n[i] == -1) box.mins[i] = std::max(box.mins[i], -pl.d);
      }
    }
    bool ok = true;
    for (int i = 0; i < 3; i++) ok = ok && box.mins[i] > -1e8f && box.maxs[i] < 1e8f;
    if (ok && box.mins.x < box.maxs.x && box.mins.y < box.maxs.y && box.mins.z < box.maxs.z)
      boxes.push_back(box);
  }
  // Brushes that touch are one ladder (a tall one is built of several).
  for (bool joined = true; joined;) {
    joined = false;
    for (size_t i = 0; i < boxes.size() && !joined; i++)
      for (size_t j = i + 1; j < boxes.size() && !joined; j++) {
        bool meet = true;
        for (int k = 0; k < 3; k++)
          meet = meet && boxes[i].mins[k] <= boxes[j].maxs[k] + 1 &&
                 boxes[j].mins[k] <= boxes[i].maxs[k] + 1;
        if (!meet) continue;
        for (int k = 0; k < 3; k++) {
          boxes[i].mins[k] = std::min(boxes[i].mins[k], boxes[j].mins[k]);
          boxes[i].maxs[k] = std::max(boxes[i].maxs[k], boxes[j].maxs[k]);
        }
        boxes.erase(boxes.begin() + static_cast<std::ptrdiff_t>(j));
        joined = true;
      }
  }
  return boxes;
}

std::vector<sb::NavExtraLink> ladder_links(const ge::shooterbots::BotView& s, const sb::NavGraph& g,
                                           const ge::brush::Map& m) {
  std::vector<sb::NavExtraLink> out;
  for (const LadderBox& L : ladder_boxes(m, brushes_in_game(s, m))) {
    if (L.maxs.z - L.mins.z < kLadderMin) continue;
    for (size_t a = 0; a < g.nodes.size(); a++) {
      const sh::Vec3f o = g.nodes[a].origin;
      if (o.z < L.mins.z - kLadderFoot || o.z > L.maxs.z - kLadderMin) continue;
      // Beside one face, and which way it faces (the heading into the ladder).
      const float ox = o.x < L.mins.x ? L.mins.x - o.x : o.x > L.maxs.x ? o.x - L.maxs.x : 0;
      const float oy = o.y < L.mins.y ? L.mins.y - o.y : o.y > L.maxs.y ? o.y - L.maxs.y : 0;
      if ((ox > 0) == (oy > 0) || ox + oy > kLadderBeside) continue;
      const float face = ox > 0 ? (o.x < L.mins.x ? 0.f : 180.f) : (o.y < L.mins.y ? 90.f : 270.f);
      int32_t best = -1;
      float best_d = 1e30f;
      for (size_t b = 0; b < g.nodes.size(); b++) {
        const sb::NavNode& nb = g.nodes[b];
        const sh::Vec3f t = nb.origin;
        if (nb.swim || t.z < L.maxs.z - 18 || t.z > L.maxs.z + kLadderAbove) continue;
        const float tx = t.x < L.mins.x ? L.mins.x - t.x : t.x > L.maxs.x ? t.x - L.maxs.x : 0;
        const float ty = t.y < L.mins.y ? L.mins.y - t.y : t.y > L.maxs.y ? t.y - L.maxs.y : 0;
        if (tx > kLadderTop || ty > kLadderTop) continue;
        const float d = (t.x - o.x) * (t.x - o.x) + (t.y - o.y) * (t.y - o.y);
        if (d < best_d) {
          best_d = d;
          best = static_cast<int32_t>(b);
        }
      }
      if (best < 0) continue;
      const float rise = g.nodes[static_cast<size_t>(best)].origin.z - o.z;
      sb::NavExtraLink e{static_cast<int32_t>(a), best, sb::NavKind::walk, 0.5f + rise / 200.f};
      e.ladder = true;
      e.face = face;
      out.push_back(e);
    }
  }
  return out;
}

// The map's entity blocks: its text (the first family), else the version 38 map's own entities.
std::vector<sb::EntityBlock> entity_blocks(const ge::shooter::MapSpec& spec) {
  std::vector<sb::EntityBlock> blocks;
  if (!spec.entities.empty() || !spec.brushes) {
    std::string error;
    if (!sb::parse_entity_text(spec.entities.c_str(), blocks, error)) blocks.clear();
    return blocks;
  }
  blocks.reserve(spec.brushes->entities.size());
  for (const ge::brush::Entity& e : spec.brushes->entities) {
    sb::EntityBlock b;
    b.pairs = e.pairs;
    blocks.push_back(std::move(b));
  }
  return blocks;
}

}  // namespace

std::vector<sb::NavMover> map_movers(const ge::shooterbots::BotView& s,
                                     const ge::shooter::MapSpec& spec,
                                     ge::brush::BrushTracer* tracer) {
  return movers_of(s, spec, tracer, entity_blocks(spec));
}

bool build_map_nav(const ge::shooterbots::BotView& s, const ge::shooter::MapSpec& spec,
                   const sb::MapBook& book, const char* start_class, MapNav& out,
                   std::string& error, const sb::NavSettings& set) {
  if (model_count(spec) == 0) {
    error = "the map " + spec.name + " has no world for bots to walk on";
    return false;
  }
  const std::vector<sb::EntityBlock> blocks = entity_blocks(spec);
  const auto t0 = std::chrono::steady_clock::now();
  out.tracer.reset();
  out.brushes = spec.brushes;
  if (spec.brushes) {
    out.tracer = std::make_unique<ge::brush::BrushTracer>(*spec.brushes);
    const ModelBox world = model_box(spec, 0);
    out.world.count = 1;
    out.world.solids[0] = sh::brushes_solid(*out.tracer, 0, {}, world.mins, world.maxs);
  } else {
    sb::map_world(spec.models[0], out.world);
  }
  out.bridges.clear();
  open_doors_as_floor(s, spec, out.tracer.get(), blocks, out.world, out.bridges);
  add_walls(s, spec, out.tracer.get(), blocks, out.world);
  out.movers = movers_of(s, spec, out.tracer.get(), blocks);
  sb::build_nav(out.world, out.movers, out.move, set, out.graph);
  std::vector<sb::NavExtraLink> teleports = teleport_links(out.graph, spec, blocks);
  const std::vector<sb::NavExtraLink> pushes =
      push_links(out.graph, spec, blocks, out.world, out.move);
  teleports.insert(teleports.end(), pushes.begin(), pushes.end());
  sb::nav_add_links(out.graph, teleports);
  if (spec.brushes) {
    const std::vector<sb::NavExtraLink> ladders = ladder_links(s, out.graph, *spec.brushes);
    sb::nav_add_links(out.graph, ladders);
    out.ladder_links = static_cast<int64_t>(ladders.size());
  }
  for (NavBridge& br : out.bridges) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(br.ent)];
    const int mi = d.modelindex;
    const char* model = mi > 0 && static_cast<size_t>(mi) < s.models.size()
                            ? s.models[static_cast<size_t>(mi)].c_str()
                            : nullptr;
    AbsBox ab = abs_box(s, spec, br.ent, model);
    if (br.turns && model != nullptr) {  // turned up now: where the map drew it instead
      const ModelBox mb = model_box(spec, std::atoi(model + 1));
      ab = {br.spawn + mb.mins, br.spawn + mb.maxs};
    }
    // Out, and a player's half-width beyond its edge (a node there stands on it too).
    // A floor door's nodes stand on it where it rests (a turning one's too); a bridge's where the
    // map drew it.
    const sh::Vec3f shift = br.floor || br.turns ? sh::Vec3f{} : br.spawn - br.out_at;
    const sh::Vec3f lo = ab.min - shift - sh::Vec3f{16, 16, 0},
                    hi = ab.max - shift + sh::Vec3f{16, 16, 0};
    for (size_t n = 0; n < out.graph.nodes.size(); n++) {
      const sh::Vec3f o = out.graph.nodes[n].origin;
      if (o.x >= lo.x && o.x <= hi.x && o.y >= lo.y && o.y <= hi.y && o.z >= hi.z &&
          o.z <= hi.z + 40)
        br.nodes.push_back(static_cast<int32_t>(n));
    }
    // The ones it alone holds up: the box traced down in the map's own solids (the world, solid
    // 0, alone) meets no floor within a step.
    const int32_t count = out.world.count;
    out.world.count = 1;
    for (const int32_t n : br.nodes) {
      const sh::Vec3f o = out.graph.nodes[static_cast<size_t>(n)].origin;
      const sh::HullTrace down = sh::player_trace(out.world, o, {o.x, o.y, o.z - kOwnDrop});
      if (down.startsolid || down.fraction >= 1) br.own.push_back(n);
    }
    out.world.count = count;
  }
  // The drops through each floor door, from the nodes that walk onto the ones it alone holds up
  // to the highest node under each (a straight fall in the map's own solids, the door left out).
  out.hatches.clear();
  if (out.hatch_drops) {
    std::vector<sb::NavExtraLink> drops;
    std::vector<int> door_of;
    std::vector<sh::Vec3f> rest_of;
    const int32_t count = out.world.count;
    out.world.count = 1;
    for (const NavBridge& br : out.bridges) {
      if (!br.floor) continue;
      std::vector<uint8_t> on(out.graph.nodes.size(), 0);
      for (const int32_t n : br.nodes) on[static_cast<size_t>(n)] = 1;
      for (const int32_t n : br.own) {
        const sh::Vec3f o = out.graph.nodes[static_cast<size_t>(n)].origin;
        int32_t under = -1;
        for (size_t m = 0; m < out.graph.nodes.size(); m++) {
          const sh::Vec3f u = out.graph.nodes[m].origin;
          if (on[m] || u.z > o.z - kHatchDrop || std::fabs(u.x - o.x) > kHatchAcross ||
              std::fabs(u.y - o.y) > kHatchAcross)
            continue;
          if (under >= 0 && u.z <= out.graph.nodes[static_cast<size_t>(under)].origin.z) continue;
          if (sh::player_trace(out.world, o, u).fraction < 1) continue;
          under = static_cast<int32_t>(m);
        }
        if (under < 0) continue;
        const float fall = o.z - out.graph.nodes[static_cast<size_t>(under)].origin.z;
        for (size_t a = 0; a < out.graph.nodes.size(); a++) {
          if (on[a]) continue;
          const sb::NavNode& na = out.graph.nodes[a];
          for (int32_t k = 0; k < na.links; k++) {
            const sb::NavLink& l =
                out.graph.links[static_cast<size_t>(na.first_link) + static_cast<size_t>(k)];
            if (l.to != n || l.kind != sb::NavKind::walk) continue;
            drops.push_back(
                {static_cast<int32_t>(a), under, sb::NavKind::drop, l.time_s + 0.2f + fall / 800});
            door_of.push_back(br.ent);
            rest_of.push_back(br.spawn);
          }
        }
      }
    }
    out.world.count = count;
    sb::nav_add_links(out.graph, drops);
    for (size_t k = 0; k < drops.size(); k++) {
      const sb::NavNode& na = out.graph.nodes[static_cast<size_t>(drops[k].from)];
      for (int32_t j = na.links - 1; j >= 0; j--) {
        const size_t li = static_cast<size_t>(na.first_link) + static_cast<size_t>(j);
        const sb::NavLink& l = out.graph.links[li];
        if (l.to == drops[k].to && l.kind == sb::NavKind::drop) {
          out.hatches.push_back(
              {static_cast<int32_t>(li), drops[k].from, drops[k].to, door_of[k], rest_of[k]});
          break;
        }
      }
    }
  }
  out.teleport_links = static_cast<int64_t>(teleports.size());
  // SCRIPTED ROUTES to key items (the book's, shooterbots/map_book.hpp): each proven by the move
  // from the node at its entry to the node at its end and added as one link; one that does not hold
  // is left out.
  {
    std::string base = spec.name;
    if (const size_t slash = base.find_last_of('/'); slash != std::string::npos)
      base = base.substr(slash + 1);
    if (const size_t dot = base.rfind(".bsp"); dot != std::string::npos) base.resize(dot);
    out.scripts_made.clear();
    std::vector<sb::NavExtraLink> scripted;
    for (const sb::ItemRoute& r : sb::item_routes(book, base)) {
      MapNav::ScriptMade made;
      made.name = r.name;
      if (r.n < 2) continue;
      const sb::ItemRouteStep& first = r.steps[0];
      const sb::ItemRouteStep& last = r.steps[r.n - 1];
      made.from = sb::nav_nearest(out.graph, {first.x, first.y, first.z + 24});
      made.to = sb::nav_nearest(out.graph, {last.x, last.y, last.z + 24});
      if (made.from >= 0 && made.to >= 0 && made.from != made.to) {
        sb::NavScript sc;
        for (size_t k = 0; k < r.n; k++)
          sc.steps.push_back({{r.steps[k].x, r.steps[k].y, r.steps[k].z + 24}, r.steps[k].hop});
        sc.steps.back().at = out.graph.nodes[static_cast<size_t>(made.to)].origin;
        const sb::NavProof pr = sb::prove_script(
            out.world, out.move, out.graph.nodes[static_cast<size_t>(made.from)].origin, sc,
            out.graph.set);
        if (pr.reached) {
          made.held = true;
          made.time_s = pr.time_s;
          for (const sb::NavMover& m : out.movers)
            if (pr.lo.x <= m.absmax.x && pr.lo.y <= m.absmax.y && pr.lo.z <= m.absmax.z &&
                m.absmin.x <= pr.hi.x && m.absmin.y <= pr.hi.y && m.absmin.z <= pr.hi.z) {
              made.mover = m.ent;
              break;
            }
          sb::NavExtraLink e{made.from, made.to, sb::NavKind::script, pr.time_s};
          e.mover = made.mover;
          e.script = static_cast<int32_t>(out.graph.scripts.size());
          out.graph.scripts.push_back(std::move(sc));
          scripted.push_back(e);
        }
      }
      out.scripts_made.push_back(made);
    }
    sb::nav_add_links(out.graph, scripted);
  }
  // CRAWLWAYS (Quake II's: a roof too low to stand under, walked ducked; base1's way east is one,
  // "Crouch here"). From each node a step out to where a ducked box fits and a standing one does
  // not, the crawl is followed cell by cell (a step up, across ducked, down to a floor) to where
  // he stands again by another node: a scripted way each way, its steps ducked.
  if (spec.brushes && out.tracer != nullptr && out.world.count > 0)
    out.crawlways = add_crawlways(out.world.solids[0], out.graph, &out.crawls);
  // LAVA AND SLIME: a floor node standing in either (a shallow pool at the feet too: it burns
  // there as well) harms whoever goes there; no way leads into one (the links out are kept: a
  // player knocked in walks out).
  {
    sb::NavGraph& g = out.graph;
    out.harm.assign(g.nodes.size(), 0);
    for (size_t n = 0; n < g.nodes.size(); n++) {
      const sh::Vec3f o = g.nodes[n].origin;
      for (const float dz : {-22.f, -10.f, 4.f}) {
        const int c = sh::water_contents(out.world, {o.x, o.y, o.z + dz});
        if (c == sh::kContentsSlime || c == sh::kContentsLava) out.harm[n] = 1;
      }
    }
    // A bridge that moves when touched pulls out from under whoever steps onto it: its nodes
    // harm as the pit under them does.
    for (const NavBridge& br : out.bridges)
      if (br.touch)
        for (const int32_t n : br.nodes) out.harm[static_cast<size_t>(n)] = 1;
    out.harm_links = sb::nav_close_entries(g, out.harm);
  }
  // Water: part of the graph (shooterbots/swim.hpp `nav_add_swim`): its cells are swim nodes, a
  // pool's floor joined to the water over it, a dry edge to the water under it by the climb out the
  // move proves. One route search goes through it as through any room.
  sb::NavGraph& graph = out.graph;
  out.wet.assign(graph.nodes.size(), 0);
  for (size_t n = 0; n < graph.nodes.size(); n++) {
    const sh::Vec3f o = graph.nodes[n].origin;
    out.wet[n] = sh::water_contents(out.world, {o.x, o.y, o.z - 10}) == sh::kContentsWater ? 1 : 0;
  }
  sb::SwimNavSettings swim_set;
  swim_set.move = out.move;  // climbs out proven on the game's own water jump
  out.swim_nav = sb::nav_add_swim(out.world, graph, out.wet, swim_set);
  out.wet.resize(graph.nodes.size(), 1);   // every swim node is wet
  out.harm.resize(graph.nodes.size(), 0);  // and none harms
  // The map's main part: the most a start spot reaches.
  const auto find_main = [&] {
    out.main_n = 0;
    ge::ai::Search sr;
    for (const sb::EntityBlock& b : blocks) {
      const char* cn = sb::block_value(b, "classname");
      const char* org = sb::block_value(b, "origin");
      if (!cn || !org || std::strcmp(cn, start_class) != 0) continue;
      const int32_t k = sb::nav_reach(graph, sb::nav_nearest(graph, origin_of(org)), sr);
      if (k > out.main_n) {
        out.main_n = k;
        std::swap(out.main, sr);
      }
    }
  };
  find_main();
  // The pockets it does not reach, and those that reach none of it back, bridged by the links the
  // build never tries (shooterbots/nav.hpp `nav_bridge_pockets`); the main part found again.
  if (out.main_n > 0) {
    std::vector<uint8_t> main(graph.nodes.size());
    for (size_t n = 0; n < main.size(); n++)
      main[n] = out.main.reached(static_cast<int32_t>(n)) ? 1 : 0;
    // A rocket jump only into a pocket that holds an item.
    std::vector<uint8_t> wanted(graph.nodes.size(), 0);
    for (const sb::EntityBlock& b : blocks) {
      const char* cn = sb::block_value(b, "classname");
      const char* org = sb::block_value(b, "origin");
      if (!cn || !org || (std::strncmp(cn, "item_", 5) != 0 && std::strncmp(cn, "weapon_", 7) != 0))
        continue;
      const sh::Vec3f at = origin_of(org);
      const int32_t n = sb::nav_nearest(graph, {at.x, at.y, at.z + 24});
      if (n >= 0) wanted[static_cast<size_t>(n)] = 1;
    }
    sb::NavBridgeSettings bs;
    bs.wanted = wanted;
    bs.avoid = out.harm;
    out.bridges_made = sb::nav_bridge_pockets(out.world, out.move, graph, main, bs);
    find_main();
  }
  // The map's tricks: rocket links people's jumps prove, landing where a way back on foot leads
  // to the main part.
  {
    std::string base = spec.name;
    if (const size_t slash = base.find_last_of('/'); slash != std::string::npos)
      base = base.substr(slash + 1);
    if (const size_t dot = base.rfind(".bsp"); dot != std::string::npos) base.resize(dot);
    const std::vector<sb::NavTrick> tricks = sb::nav_tricks(sb::rocket_tricks(book, base));
    if (!tricks.empty() && out.main_n > 0) {
      std::vector<uint8_t> main(graph.nodes.size());
      for (size_t n = 0; n < main.size(); n++)
        main[n] = out.main.reached(static_cast<int32_t>(n)) && !out.wet[n] ? 1 : 0;
      std::vector<uint8_t> back = sb::nav_back(graph, main);
      for (size_t n = 0; n < back.size(); n++)
        back[n] = (back[n] || out.one_way_tricks) && !out.harm[n] ? 1 : 0;
      // The tricks as rocket jumps and as grenade jumps; then a grenade jump for each rocket
      // jump the pockets were bridged by (the way into a pocket for a player with no launcher).
      std::vector<sb::NavTrick> bridged;
      for (const sb::NavNode& n : graph.nodes)
        for (int32_t k = 0; k < n.links; k++) {
          const sb::NavLink& l =
              graph.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)];
          if (l.kind != sb::NavKind::rocket) continue;
          sb::NavTrick t;
          t.from = n.origin;
          t.to = graph.nodes[static_cast<size_t>(l.to)].origin;
          bridged.push_back(t);
        }
      sb::NavTrickSettings ts;
      ts.grenades = true;
      ts.combos = out.one_way_tricks;  // a record run's grenade and rocket together
      if (out.one_way_tricks) {
        // A record run's jump flown its own way (air control, another run-up): a takeoff and a
        // landing further from the runner's still make the same way past what blocks the floor.
        ts.near = 96;
        ts.land = 160;
        ts.takeoffs = 8;
      }
      out.tricks_made = sb::nav_add_tricks(out.world, out.move, graph, tricks, back, ts);
      ts.rockets = false;
      ts.combos = false;
      ts.near = 8;  // from the bridge's own takeoff
      out.bridged_grenades = sb::nav_add_tricks(out.world, out.move, graph, bridged, back, ts);
    }
  }
  // The traps: nodes no way on foot leads back from to the main part's core (a room a rocket jump
  // or a drop leads into and nothing out of): no goal of a bot's. The core, not the main part
  // itself: a pocket a drop leads into is reached from a start too (base1's east pool, where his
  // exploring dropped him and nothing reached the exit's button again).
  {
    std::vector<uint8_t> main(graph.nodes.size());
    const bool found = out.main.nodes() == graph.nodes.size();
    for (size_t n = 0; n < main.size(); n++)
      main[n] = found && out.main.reached(static_cast<int32_t>(n)) && !out.wet[n] ? 1 : 0;
    const std::vector<uint8_t> back = sb::nav_back(graph, sb::nav_core(graph, main));
    out.trap.assign(graph.nodes.size(), 0);
    for (size_t n = 0; n < back.size() && found; n++) out.trap[n] = back[n] && !out.harm[n] ? 0 : 1;
  }
  // The jumps a miss of kills (over lava or slime, or landing at an edge over it): flown again
  // past the target and from a slower run, kept only when both hold, and dearer (dm2's lava room
  // took half the bots' deaths: a jump down onto its ramp carried them off the edge).
  // A miss into a trap (base1's river under a rail a jump lands on) is as final as one into lava:
  // the jumps over or beside one are hardened alike.
  {
    std::vector<uint8_t> risk(graph.nodes.size(), 0);
    for (size_t n = 0; n < risk.size(); n++)
      risk[n] =
          (n < out.harm.size() && out.harm[n]) || (n < out.trap.size() && out.trap[n]) ? 1 : 0;
    out.hardened = sb::nav_harden_jumps(out.world, out.move, graph, risk);
  }
  // THE GAP TRICKS (the book's): people's speed jumps over a pit as jump links at a hopper's
  // speed, landing where a way back on foot leads to the main part; after the hardening, which
  // would price a jump over lava as a run's miss.
  {
    std::string base = spec.name;
    if (const size_t slash = base.find_last_of('/'); slash != std::string::npos)
      base = base.substr(slash + 1);
    if (const size_t dot = base.rfind(".bsp"); dot != std::string::npos) base.resize(dot);
    const std::vector<sb::NavGapTrick> gaps = sb::nav_gap_tricks(sb::gap_tricks(book, base));
    if (!gaps.empty() && out.main_n > 0) {
      std::vector<uint8_t> back(graph.nodes.size());
      for (size_t n = 0; n < back.size(); n++)
        back[n] =
            out.main.reached(static_cast<int32_t>(n)) && !out.wet[n] && !out.harm[n] && !out.trap[n]
                ? 1
                : 0;
      out.gaps_made = sb::nav_add_gap_tricks(out.world, out.move, graph, gaps, back);
    }
  }
  // OUT OF A PIT BY ROCKET (shooterbots/escape.hpp): from every liquid at least waist deep, a
  // rocket fired down at the surface throws a swimmer backed against the wall onto the ledge behind
  // him (dm2's lava under the red armour has no other way out); landing on the main part's dry
  // floor.
  {
    std::vector<uint8_t> land(graph.nodes.size(), 0);
    for (size_t n = 0; n < land.size(); n++)
      land[n] = out.main_n > 0 && out.main.reached(static_cast<int32_t>(n)) && !out.wet[n] &&
                        !out.harm[n] && !out.trap[n]
                    ? 1
                    : 0;
    // Never onto a bridge: the nav's world holds one where the map drew it, in or not.
    for (const NavBridge& br : out.bridges)
      for (const int32_t n : br.nodes)
        if (n >= 0 && static_cast<size_t>(n) < land.size()) land[static_cast<size_t>(n)] = 0;
    out.escapes_made = sb::nav_add_escapes(out.world, out.move, graph, land);
  }
  std::vector<uint8_t> exits(graph.nodes.size(), 0);
  for (size_t n = 0; n < graph.nodes.size(); n++)
    exits[n] = out.main_n > 0 && out.main.reached(static_cast<int32_t>(n)) && !out.wet[n] ? 1 : 0;
  sb::SwimSettings field_set;
  field_set.move = out.move;
  sb::build_swim(out.world, graph, exits, field_set, out.swim);
  // THE BRINK (shooterbots/brink.hpp): the harm kept on the graph, the walks along its lips marked.
  out.brink_links = sb::nav_mark_harm(graph, out.harm);
  out.side_brink_links = sb::nav_mark_side_harm(graph, out.world);
  sb::nav_room(graph);  // the room either side a walker strays into (pilot.hpp's lane)
  out.build_ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  if (graph.nodes.empty()) {
    error = "Bots can't play on " + spec.name + ": there is no floor they can walk on.";
    return false;
  }
  return true;
}

sh::MoveSettings q2_nav_move(float air_accelerate) {
  sh::MoveSettings m;
  m.maxspeed = 300;
  m.friction = 6;
  // In the water: half the wish swum, and a water jump that clears a lower lip higher.
  m.swimscale = 0.5;
  m.waterjump = 350;
  m.waterjump_reach = 30;
  m.waterjump_lip = 4;
  m.waterjump_open = 20;
  if (air_accelerate == 0) {
    m.air_accel = 1;
    m.air_wish_cap = 0;
  }
  return m;
}

}  // namespace ge::shooterbots
