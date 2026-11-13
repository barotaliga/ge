// A real map's nav for the bots (map_nav.hpp).
//
// ge:summary A real map's nav: the doors and plats off the entity text, teleporters, the water
// ge:keywords build_map_nav map_movers teleport-links open-doors-as-floor swim-field
// ge:skill shooter bots
// ge:test tests/shooters_edge_team_test.cpp
#include "shooters_edge/bots/map_nav.hpp"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "progs/entity_text.hpp"
#include "shooter/nav_script.hpp"
#include "shooters_edge/bots/gap_tricks.hpp"
#include "shooters_edge/bots/item_routes.hpp"
#include "shooters_edge/bots/rocket_tricks.hpp"

namespace shooters_edge::bots {

namespace {

namespace sh = ge::shooter;
namespace pg = ge::progs;
namespace sv = shooters_edge::server;

// A model's box, either family: [0] the world, then the inline models ("*N").
struct ModelBox {
  bool ok = false;
  sh::Vec3f mins, maxs;
};

size_t model_count(const sv::MapSpec& spec) {
  return spec.brushes ? spec.brushes->models.size() : spec.models.size();
}

ModelBox model_box(const sv::MapSpec& spec, int m) {
  ModelBox b;
  if (m < 0 || static_cast<size_t>(m) >= model_count(spec)) return b;
  b.ok = true;
  if (spec.brushes) {
    const ge::brush::Model& bm = spec.brushes->models[static_cast<size_t>(m)];
    b.mins = {bm.mins[0], bm.mins[1], bm.mins[2]};
    b.maxs = {bm.maxs[0], bm.maxs[1], bm.maxs[2]};
  } else {
    b.mins = spec.models[static_cast<size_t>(m)].mins;
    b.maxs = spec.models[static_cast<size_t>(m)].maxs;
  }
  return b;
}

// Inline model `m` as a solid of the nav's world standing at `origin`.
sh::Solid model_solid(const sv::MapSpec& spec, ge::brush::BrushTracer* tracer, int m,
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
bool start_open(const pg::EntityBlock& b) {
  const char* sf = pg::block_value(b, "spawnflags");
  return sf && (std::atoi(sf) & 1) != 0;
}

// The edict the mod made of inline model `model`, or -1.
int edict_of(const sv::RulesCommon& s, const char* model) {
  for (int e = s.sv.set.maxclients + 1; e < s.sv.num_edicts; e++) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
    if (d.free || d.modelindex <= 0 || static_cast<size_t>(d.modelindex) >= s.models.size())
      continue;
    if (s.models[static_cast<size_t>(d.modelindex)] == model) return e;
  }
  return -1;
}

// Edict `e`'s box in the world: its own, else (a runner whose rows carry no box for a brush
// entity: the sequel's) inline model `model`'s box where the edict stands.
struct AbsBox {
  sh::Vec3f min, max;
};

AbsBox abs_box(const sv::RulesCommon& s, const sv::MapSpec& spec, int e, const char* model) {
  const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
  if (d.absmax.x > d.absmin.x || d.absmax.y > d.absmin.y || d.absmax.z > d.absmin.z || !model ||
      model[0] != '*')
    return {d.absmin, d.absmax};
  const ModelBox m = model_box(spec, std::atoi(model + 1));
  if (!m.ok) return {d.absmin, d.absmax};
  return {d.origin + m.mins, d.origin + m.maxs};
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
std::vector<uint8_t> named_doors(const sv::RulesCommon& s, const sv::MapSpec& spec,
                                 const std::vector<pg::EntityBlock>& blocks) {
  struct D {
    int e;
    bool named, apart;
    AbsBox box;
  };
  std::vector<D> ds;
  for (const pg::EntityBlock& b : blocks) {
    const char* cn = pg::block_value(b, "classname");
    const char* model = pg::block_value(b, "model");
    if (!cn || !model || model[0] != '*' || std::strcmp(cn, "func_door") != 0 || start_open(b))
      continue;
    const int e = edict_of(s, model);
    if (e < 0) continue;
    const char* sf = pg::block_value(b, "spawnflags");
    ds.push_back({e, pg::block_value(b, "targetname") != nullptr || pg::block_value(b, "health"),
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
bool floor_door(const pg::EntityBlock& b, const ModelBox& bm, const std::vector<uint8_t>& named,
                int e) {
  const char* cn = pg::block_value(b, "classname");
  return cn && std::strcmp(cn, "func_door") == 0 && !start_open(b) && plate(bm) && e >= 0 &&
         static_cast<size_t>(e) < named.size() && named[static_cast<size_t>(e)];
}

void open_doors_as_floor(const sv::RulesCommon& s, const sv::MapSpec& spec,
                         ge::brush::BrushTracer* tracer, const std::vector<pg::EntityBlock>& blocks,
                         sh::MoveWorld& world, std::vector<NavBridge>& bridges) {
  const std::vector<uint8_t> named = named_doors(s, spec, blocks);
  for (const pg::EntityBlock& b : blocks) {
    const char* model = pg::block_value(b, "model");
    if (!model || model[0] != '*') continue;
    const int m = std::atoi(model + 1);
    if (m <= 0 || static_cast<size_t>(m) >= model_count(spec) || world.count >= sh::kMaxSolids)
      continue;
    const int e = edict_of(s, model);
    const ModelBox bm = model_box(spec, m);
    if (!floor_door(b, bm, named, e)) continue;
    sh::Solid& so = world.solids[world.count++];
    so = model_solid(spec, tracer, m, s.sv.edicts[static_cast<size_t>(e)].origin);  // at rest
    NavBridge br;
    br.ent = e;
    br.spawn = so.origin;
    br.floor = true;
    bridges.push_back(br);
  }
  for (const pg::EntityBlock& b : blocks) {
    const char* cn = pg::block_value(b, "classname");
    const char* model = pg::block_value(b, "model");
    if (!cn || !model || model[0] != '*' || std::strcmp(cn, "func_door") != 0 || !start_open(b))
      continue;
    const int m = std::atoi(model + 1);
    if (m <= 0 || static_cast<size_t>(m) >= model_count(spec) || world.count >= sh::kMaxSolids)
      continue;
    const int e = edict_of(s, model);
    if (e < 0) continue;
    const ModelBox bm = model_box(spec, m);
    sh::Solid& so = world.solids[world.count++];
    so = model_solid(spec, tracer, m, s.sv.edicts[static_cast<size_t>(e)].origin);
    if (plate(bm)) {
      NavBridge br;
      br.ent = e;
      br.spawn = so.origin;
      br.touch = pg::block_value(b, "targetname") == nullptr;
      const char* wait = pg::block_value(b, "wait");
      br.wait = wait ? static_cast<float>(std::atof(wait)) : 3;  // a door's default
      bridges.push_back(br);
      so.origin = {};  // out, where the map drew it
    }
  }
}

std::vector<sh::NavMover> movers_of(const sv::RulesCommon& s, const sv::MapSpec& spec,
                                    ge::brush::BrushTracer* tracer,
                                    const std::vector<pg::EntityBlock>& blocks) {
  std::vector<sh::NavMover> out;
  const std::vector<uint8_t> named = named_doors(s, spec, blocks);
  for (const pg::EntityBlock& b : blocks) {
    const char* cn = pg::block_value(b, "classname");
    const char* model = pg::block_value(b, "model");
    if (!cn || !model || model[0] != '*') continue;
    const bool door = std::strcmp(cn, "func_door") == 0 || std::strcmp(cn, "func_door_secret") == 0;
    const bool plat = std::strcmp(cn, "func_plat") == 0;
    if (!door && !plat) continue;
    if (door && start_open(b)) continue;  // a floor to the nav (open_doors_as_floor)
    const int m = std::atoi(model + 1);
    if (m <= 0 || static_cast<size_t>(m) >= model_count(spec)) continue;
    const int ent = edict_of(s, model);
    const ModelBox bm = model_box(spec, m);
    if (door && floor_door(b, bm, named, ent)) continue;  // too
    if (ent < 0) continue;
    sh::NavMover mv;
    mv.ent = ent;
    const AbsBox ab = abs_box(s, spec, ent, model);
    mv.absmin = ab.min;
    mv.absmax = ab.max;
    if (plat && (!spec.brushes || tracer != nullptr)) {
      // Drawn raised and lowered by its height (else its own height less an 8-unit lip); the
      // sequel's default speed is 200 units/s, the first game's 150.
      const char* h = pg::block_value(b, "height");
      const char* sp = pg::block_value(b, "speed");
      const float rise = h ? static_cast<float>(std::atof(h)) : (bm.maxs.z - bm.mins.z) - 8;
      const float fallback = spec.brushes ? 200.f : 150.f;
      const float speed = sp ? static_cast<float>(std::atof(sp)) : fallback;
      if (spec.brushes) {
        mv.tracer = tracer;
        mv.brush_model = static_cast<uint32_t>(m);
      } else {
        mv.model = &spec.models[static_cast<size_t>(m)];
      }
      mv.rest = s.sv.edicts[static_cast<size_t>(ent)].origin;  // lowered from where it was built
      mv.travel = {0, 0, rise};
      mv.ride_s = 1 + rise / (speed > 0 ? speed : fallback);
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
// its rules lift it (the first game's 27 units, the sequel's pad 10).
bool teleport_dest(const std::vector<pg::EntityBlock>& blocks, const char* target,
                   sh::Vec3f& dest) {
  for (const pg::EntityBlock& d : blocks) {
    const char* dn = pg::block_value(d, "targetname");
    const char* dc = pg::block_value(d, "classname");
    const char* org = pg::block_value(d, "origin");
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

std::vector<sh::NavExtraLink> teleport_links(const sh::NavGraph& g, const sv::MapSpec& spec,
                                             const std::vector<pg::EntityBlock>& blocks) {
  std::vector<sh::NavExtraLink> out;
  for (const pg::EntityBlock& b : blocks) {
    const char* cn = pg::block_value(b, "classname");
    const char* target = pg::block_value(b, "target");
    if (!cn || !target || pg::block_value(b, "targetname")) continue;
    // Where it is touched: a trigger brush's box, or the box over a pad's centre.
    ModelBox box;
    if (std::strcmp(cn, "trigger_teleport") == 0) {
      const char* model = pg::block_value(b, "model");
      if (!model || model[0] != '*' || std::atoi(model + 1) <= 0) continue;
      box = model_box(spec, std::atoi(model + 1));
      if (!box.ok) continue;
    } else if (std::strcmp(cn, "misc_teleporter") == 0) {
      const char* org = pg::block_value(b, "origin");
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
    const int32_t to = sh::nav_nearest(g, dest);
    if (to < 0) continue;
    for (size_t a = 0; a < g.nodes.size(); a++) {
      const sh::Vec3f o = g.nodes[a].origin;
      if (o.x + 16 < box.mins.x || o.x - 16 > box.maxs.x || o.y + 16 < box.mins.y ||
          o.y - 16 > box.maxs.y || o.z + 32 < box.mins.z || o.z - 24 > box.maxs.z)
        continue;
      sh::NavExtraLink e{static_cast<int32_t>(a), to, sh::NavKind::walk, 0.1f};
      e.teleport = true;
      out.push_back(e);
    }
  }
  return out;
}

// The map's entity blocks: its text (the first family), else the version 38 map's own entities.
std::vector<pg::EntityBlock> entity_blocks(const sv::MapSpec& spec) {
  std::vector<pg::EntityBlock> blocks;
  if (!spec.entities.empty() || !spec.brushes) {
    std::string error;
    if (!pg::parse_entity_text(spec.entities.c_str(), blocks, error)) blocks.clear();
    return blocks;
  }
  blocks.reserve(spec.brushes->entities.size());
  for (const ge::brush::Entity& e : spec.brushes->entities) {
    pg::EntityBlock b;
    b.pairs = e.pairs;
    blocks.push_back(std::move(b));
  }
  return blocks;
}

}  // namespace

std::vector<sh::NavMover> map_movers(const sv::RulesCommon& s, const sv::MapSpec& spec,
                                     ge::brush::BrushTracer* tracer) {
  return movers_of(s, spec, tracer, entity_blocks(spec));
}

bool build_map_nav(const sv::RulesCommon& s, const sv::MapSpec& spec, const char* start_class,
                   MapNav& out, std::string& error) {
  if (model_count(spec) == 0) {
    error = "the map " + spec.name + " has no world for bots to walk on";
    return false;
  }
  const std::vector<pg::EntityBlock> blocks = entity_blocks(spec);
  const auto t0 = std::chrono::steady_clock::now();
  out.tracer.reset();
  out.brushes = spec.brushes;
  if (spec.brushes) {
    out.tracer = std::make_unique<ge::brush::BrushTracer>(*spec.brushes);
    const ModelBox world = model_box(spec, 0);
    out.world.count = 1;
    out.world.solids[0] = sh::brushes_solid(*out.tracer, 0, {}, world.mins, world.maxs);
  } else {
    sh::map_world(spec.models[0], out.world);
  }
  out.bridges.clear();
  open_doors_as_floor(s, spec, out.tracer.get(), blocks, out.world, out.bridges);
  out.movers = movers_of(s, spec, out.tracer.get(), blocks);
  sh::build_nav(out.world, out.movers, out.move, sh::NavSettings{}, out.graph);
  const std::vector<sh::NavExtraLink> teleports = teleport_links(out.graph, spec, blocks);
  sh::nav_add_links(out.graph, teleports);
  for (NavBridge& br : out.bridges) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(br.ent)];
    const int mi = d.modelindex;
    const char* model = mi > 0 && static_cast<size_t>(mi) < s.models.size()
                            ? s.models[static_cast<size_t>(mi)].c_str()
                            : nullptr;
    const AbsBox ab = abs_box(s, spec, br.ent, model);
    // Out, and a player's half-width beyond its edge (a node there stands on it too).
    // A floor door's nodes stand on it where it rests; a bridge's where the map drew it.
    const sh::Vec3f shift = br.floor ? sh::Vec3f{} : br.spawn;
    const sh::Vec3f lo = ab.min - shift - sh::Vec3f{16, 16, 0},
                    hi = ab.max - shift + sh::Vec3f{16, 16, 0};
    for (size_t n = 0; n < out.graph.nodes.size(); n++) {
      const sh::Vec3f o = out.graph.nodes[n].origin;
      if (o.x >= lo.x && o.x <= hi.x && o.y >= lo.y && o.y <= hi.y && o.z >= hi.z &&
          o.z <= hi.z + 40)
        br.nodes.push_back(static_cast<int32_t>(n));
    }
  }
  out.teleport_links = static_cast<int64_t>(teleports.size());
  // SCRIPTED ROUTES to key items (item_routes.hpp): each proven by the move from the node at its
  // entry to the node at its end and added as one link; one that does not hold is left out.
  {
    std::string base = spec.name;
    if (const size_t slash = base.find_last_of('/'); slash != std::string::npos)
      base = base.substr(slash + 1);
    if (const size_t dot = base.rfind(".bsp"); dot != std::string::npos) base.resize(dot);
    out.scripts_made.clear();
    std::vector<sh::NavExtraLink> scripted;
    for (const ItemRoute& r : item_routes(base)) {
      MapNav::ScriptMade made;
      made.name = r.name;
      if (r.n < 2) continue;
      const ItemRouteStep& first = r.steps[0];
      const ItemRouteStep& last = r.steps[r.n - 1];
      made.from = sh::nav_nearest(out.graph, {first.x, first.y, first.z + 24});
      made.to = sh::nav_nearest(out.graph, {last.x, last.y, last.z + 24});
      if (made.from >= 0 && made.to >= 0 && made.from != made.to) {
        sh::NavScript sc;
        for (size_t k = 0; k < r.n; k++)
          sc.steps.push_back({{r.steps[k].x, r.steps[k].y, r.steps[k].z + 24}, r.steps[k].hop});
        sc.steps.back().at = out.graph.nodes[static_cast<size_t>(made.to)].origin;
        const sh::NavProof pr = sh::prove_script(
            out.world, out.move, out.graph.nodes[static_cast<size_t>(made.from)].origin, sc,
            out.graph.set);
        if (pr.reached) {
          made.held = true;
          made.time_s = pr.time_s;
          for (const sh::NavMover& m : out.movers)
            if (pr.lo.x <= m.absmax.x && pr.lo.y <= m.absmax.y && pr.lo.z <= m.absmax.z &&
                m.absmin.x <= pr.hi.x && m.absmin.y <= pr.hi.y && m.absmin.z <= pr.hi.z) {
              made.mover = m.ent;
              break;
            }
          sh::NavExtraLink e{made.from, made.to, sh::NavKind::script, pr.time_s};
          e.mover = made.mover;
          e.script = static_cast<int32_t>(out.graph.scripts.size());
          out.graph.scripts.push_back(std::move(sc));
          scripted.push_back(e);
        }
      }
      out.scripts_made.push_back(made);
    }
    sh::nav_add_links(out.graph, scripted);
  }
  // LAVA AND SLIME: a floor node standing in either (a shallow pool at the feet too: it burns
  // there as well) harms whoever goes there; no way leads into one (the links out are kept: a
  // player knocked in walks out).
  {
    sh::NavGraph& g = out.graph;
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
    out.harm_links = sh::nav_close_entries(g, out.harm);
  }
  // Water: part of the graph (shooter/swim.hpp `nav_add_swim`): its cells are swim nodes, a pool's
  // floor joined to the water over it, a dry edge to the water under it by the climb out the
  // move proves. One route search goes through it as through any room.
  sh::NavGraph& graph = out.graph;
  out.wet.assign(graph.nodes.size(), 0);
  for (size_t n = 0; n < graph.nodes.size(); n++) {
    const sh::Vec3f o = graph.nodes[n].origin;
    out.wet[n] = sh::water_contents(out.world, {o.x, o.y, o.z - 10}) == sh::kContentsWater ? 1 : 0;
  }
  out.swim_nav = sh::nav_add_swim(out.world, graph, out.wet);
  out.wet.resize(graph.nodes.size(), 1);   // every swim node is wet
  out.harm.resize(graph.nodes.size(), 0);  // and none harms
  // The map's main part: the most a start spot reaches.
  const auto find_main = [&] {
    out.main_n = 0;
    ge::ai::Search sr;
    for (const pg::EntityBlock& b : blocks) {
      const char* cn = pg::block_value(b, "classname");
      const char* org = pg::block_value(b, "origin");
      if (!cn || !org || std::strcmp(cn, start_class) != 0) continue;
      const int32_t k = sh::nav_reach(graph, sh::nav_nearest(graph, origin_of(org)), sr);
      if (k > out.main_n) {
        out.main_n = k;
        std::swap(out.main, sr);
      }
    }
  };
  find_main();
  // The pockets it does not reach, and those that reach none of it back, bridged by the links the
  // build never tries (shooter/nav.hpp `nav_bridge_pockets`); the main part found again.
  if (out.main_n > 0) {
    std::vector<uint8_t> main(graph.nodes.size());
    for (size_t n = 0; n < main.size(); n++)
      main[n] = out.main.reached(static_cast<int32_t>(n)) ? 1 : 0;
    // A rocket jump only into a pocket that holds an item.
    std::vector<uint8_t> wanted(graph.nodes.size(), 0);
    for (const pg::EntityBlock& b : blocks) {
      const char* cn = pg::block_value(b, "classname");
      const char* org = pg::block_value(b, "origin");
      if (!cn || !org || (std::strncmp(cn, "item_", 5) != 0 && std::strncmp(cn, "weapon_", 7) != 0))
        continue;
      const sh::Vec3f at = origin_of(org);
      const int32_t n = sh::nav_nearest(graph, {at.x, at.y, at.z + 24});
      if (n >= 0) wanted[static_cast<size_t>(n)] = 1;
    }
    sh::NavBridgeSettings bs;
    bs.wanted = wanted;
    bs.avoid = out.harm;
    out.bridges_made = sh::nav_bridge_pockets(out.world, out.move, graph, main, bs);
    find_main();
  }
  // The map's tricks: rocket links people's jumps prove, landing where a way back on foot leads
  // to the main part.
  {
    std::string base = spec.name;
    if (const size_t slash = base.find_last_of('/'); slash != std::string::npos)
      base = base.substr(slash + 1);
    if (const size_t dot = base.rfind(".bsp"); dot != std::string::npos) base.resize(dot);
    const std::vector<sh::NavTrick> tricks = nav_tricks(rocket_tricks(base));
    if (!tricks.empty() && out.main_n > 0) {
      std::vector<uint8_t> main(graph.nodes.size());
      for (size_t n = 0; n < main.size(); n++)
        main[n] = out.main.reached(static_cast<int32_t>(n)) && !out.wet[n] ? 1 : 0;
      std::vector<uint8_t> back = sh::nav_back(graph, main);
      for (size_t n = 0; n < back.size(); n++) back[n] = back[n] && !out.harm[n] ? 1 : 0;
      // The tricks as rocket jumps and as grenade jumps; then a grenade jump for each rocket
      // jump the pockets were bridged by (the way into a pocket for a player with no launcher).
      std::vector<sh::NavTrick> bridged;
      for (const sh::NavNode& n : graph.nodes)
        for (int32_t k = 0; k < n.links; k++) {
          const sh::NavLink& l =
              graph.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)];
          if (l.kind != sh::NavKind::rocket) continue;
          sh::NavTrick t;
          t.from = n.origin;
          t.to = graph.nodes[static_cast<size_t>(l.to)].origin;
          bridged.push_back(t);
        }
      sh::NavTrickSettings ts;
      ts.grenades = true;
      out.tricks_made = sh::nav_add_tricks(out.world, out.move, graph, tricks, back, ts);
      ts.rockets = false;
      ts.near = 8;  // from the bridge's own takeoff
      out.bridged_grenades = sh::nav_add_tricks(out.world, out.move, graph, bridged, back, ts);
    }
  }
  // The traps: nodes no way on foot leads back from to the main part (a room a rocket jump or a
  // drop leads into and nothing out of): no goal of a bot's.
  {
    std::vector<uint8_t> main(graph.nodes.size());
    const bool found = out.main.nodes() == graph.nodes.size();
    for (size_t n = 0; n < main.size(); n++)
      main[n] = found && out.main.reached(static_cast<int32_t>(n)) && !out.wet[n] ? 1 : 0;
    const std::vector<uint8_t> back = sh::nav_back(graph, main);
    out.trap.assign(graph.nodes.size(), 0);
    for (size_t n = 0; n < back.size() && found; n++) out.trap[n] = back[n] && !out.harm[n] ? 0 : 1;
  }
  // The jumps a miss of kills (over lava or slime, or landing at an edge over it): flown again
  // past the target and from a slower run, kept only when both hold, and dearer (dm2's lava room
  // took half the bots' deaths: a jump down onto its ramp carried them off the edge).
  out.hardened = sh::nav_harden_jumps(out.world, out.move, graph, out.harm);
  // THE GAP TRICKS (gap_tricks.hpp): people's speed jumps over a pit as jump links at a hopper's
  // speed, landing where a way back on foot leads to the main part; after the hardening, which
  // would price a jump over lava as a run's miss.
  {
    std::string base = spec.name;
    if (const size_t slash = base.find_last_of('/'); slash != std::string::npos)
      base = base.substr(slash + 1);
    if (const size_t dot = base.rfind(".bsp"); dot != std::string::npos) base.resize(dot);
    const std::vector<sh::NavGapTrick> gaps = nav_gap_tricks(gap_tricks(base));
    if (!gaps.empty() && out.main_n > 0) {
      std::vector<uint8_t> back(graph.nodes.size());
      for (size_t n = 0; n < back.size(); n++)
        back[n] =
            out.main.reached(static_cast<int32_t>(n)) && !out.wet[n] && !out.harm[n] && !out.trap[n]
                ? 1
                : 0;
      out.gaps_made = sh::nav_add_gap_tricks(out.world, out.move, graph, gaps, back);
    }
  }
  // OUT OF A PIT BY ROCKET (shooter/escape.hpp): from every liquid at least waist deep, a rocket
  // fired down at the surface throws a swimmer backed against the wall onto the ledge behind him
  // (dm2's lava under the red armour has no other way out); landing on the main part's dry floor.
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
    out.escapes_made = sh::nav_add_escapes(out.world, out.move, graph, land);
  }
  std::vector<uint8_t> exits(graph.nodes.size(), 0);
  for (size_t n = 0; n < graph.nodes.size(); n++)
    exits[n] = out.main_n > 0 && out.main.reached(static_cast<int32_t>(n)) && !out.wet[n] ? 1 : 0;
  sh::build_swim(out.world, graph, exits, sh::SwimSettings{}, out.swim);
  // THE BRINK (shooter/brink.hpp): the harm kept on the graph, the walks along its lips marked.
  out.brink_links = sh::nav_mark_harm(graph, out.harm);
  sh::nav_room(graph);  // the room either side a walker strays into (pilot.hpp's lane)
  out.build_ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  if (graph.nodes.empty()) {
    error = "Bots can't play on " + spec.name + ": there is no floor they can walk on.";
    return false;
  }
  return true;
}

sh::MoveSettings sequel_nav_move() {
  sh::MoveSettings m;
  m.maxspeed = 300;
  m.friction = 6;
  return m;
}

}  // namespace shooters_edge::bots
