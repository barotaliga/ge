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

namespace shooters_edge::bots {

namespace {

namespace sh = ge::shooter;
namespace pg = ge::progs;
namespace sv = shooters_edge::server;

// A door that starts open (spawnflags 1) stays where it spawned until a button moves it: a plate
// over a pit, a wall drawn aside.
bool start_open(const pg::EntityBlock& b) {
  const char* sf = pg::block_value(b, "spawnflags");
  return sf && (std::atoi(sf) & 1) != 0;
}

// The edict the mod made of inline model `model`, or -1.
int edict_of(const sv::QcServer& s, const char* model) {
  for (int e = s.sv.set.maxclients + 1; e < s.sv.num_edicts; e++) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
    if (d.free || d.modelindex <= 0 || static_cast<size_t>(d.modelindex) >= s.models.size())
      continue;
    if (s.models[static_cast<size_t>(d.modelindex)] == model) return e;
  }
  return -1;
}

// Those doors as solids of the nav's world, where they stand (the rest of the movers are left
// out of it and tagged on the links they close).
// A plate: no thicker than a step's height, wider than a player both ways.
bool plate(const sh::BrushModel& bm) {
  const sh::Vec3f d = bm.maxs - bm.mins;
  return d.z <= 32 && d.x >= 48 && d.y >= 48;
}

void open_doors_as_floor(const sv::QcServer& s, const sv::MapSpec& spec,
                         const std::vector<pg::EntityBlock>& blocks, sh::MoveWorld& world,
                         std::vector<NavBridge>& bridges) {
  for (const pg::EntityBlock& b : blocks) {
    const char* cn = pg::block_value(b, "classname");
    const char* model = pg::block_value(b, "model");
    if (!cn || !model || model[0] != '*' || std::strcmp(cn, "func_door") != 0 || !start_open(b))
      continue;
    const int m = std::atoi(model + 1);
    if (m <= 0 || static_cast<size_t>(m) >= spec.models.size() || world.count >= sh::kMaxSolids)
      continue;
    const int e = edict_of(s, model);
    if (e < 0) continue;
    const sh::BrushModel& bm = spec.models[static_cast<size_t>(m)];
    sh::Solid& so = world.solids[world.count++];
    so = sh::Solid{};
    so.kind = sh::SolidKind::brush;
    so.origin = s.sv.edicts[static_cast<size_t>(e)].origin;
    if (plate(bm)) {
      NavBridge br;
      br.ent = e;
      br.spawn = so.origin;
      bridges.push_back(br);
      so.origin = {};  // out, where the map drew it
    }
    so.point_hull = bm.hulls[0];
    so.clip_hull = bm.hulls[1];
    so.mins = bm.mins;
    so.maxs = bm.maxs;
  }
}

std::vector<sh::NavMover> movers_of(const sv::QcServer& s, const sv::MapSpec& spec,
                                    const std::vector<pg::EntityBlock>& blocks) {
  std::vector<sh::NavMover> out;
  for (const pg::EntityBlock& b : blocks) {
    const char* cn = pg::block_value(b, "classname");
    const char* model = pg::block_value(b, "model");
    if (!cn || !model || model[0] != '*') continue;
    const bool door = std::strcmp(cn, "func_door") == 0 || std::strcmp(cn, "func_door_secret") == 0;
    const bool plat = std::strcmp(cn, "func_plat") == 0;
    if (!door && !plat) continue;
    if (door && start_open(b)) continue;  // a floor to the nav (open_doors_as_floor)
    const int m = std::atoi(model + 1);
    if (m <= 0 || static_cast<size_t>(m) >= spec.models.size()) continue;
    const int ent = edict_of(s, model);
    if (ent < 0) continue;
    sh::NavMover mv;
    mv.ent = ent;
    mv.absmin = s.sv.edicts[static_cast<size_t>(ent)].absmin;
    mv.absmax = s.sv.edicts[static_cast<size_t>(ent)].absmax;
    if (plat) {
      const sh::BrushModel& bm = spec.models[static_cast<size_t>(m)];
      const char* h = pg::block_value(b, "height");
      const char* sp = pg::block_value(b, "speed");
      const float rise = h ? static_cast<float>(std::atof(h)) : (bm.maxs.z - bm.mins.z) - 8;
      const float speed = sp ? static_cast<float>(std::atof(sp)) : 150;
      mv.model = &bm;
      mv.rest = s.sv.edicts[static_cast<size_t>(ent)].origin;  // lowered from where it was built
      mv.travel = {0, 0, rise};
      mv.ride_s = 1 + rise / (speed > 0 ? speed : 150);
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

std::vector<sh::NavExtraLink> teleport_links(const sh::NavGraph& g, const sv::MapSpec& spec,
                                             const std::vector<pg::EntityBlock>& blocks) {
  std::vector<sh::NavExtraLink> out;
  for (const pg::EntityBlock& b : blocks) {
    const char* cn = pg::block_value(b, "classname");
    const char* model = pg::block_value(b, "model");
    const char* target = pg::block_value(b, "target");
    if (!cn || std::strcmp(cn, "trigger_teleport") != 0 || !model || model[0] != '*' || !target ||
        pg::block_value(b, "targetname"))
      continue;
    const int m = std::atoi(model + 1);
    if (m <= 0 || static_cast<size_t>(m) >= spec.models.size()) continue;
    const sh::BrushModel& box = spec.models[static_cast<size_t>(m)];
    sh::Vec3f dest;
    bool found = false;
    for (const pg::EntityBlock& d : blocks) {
      const char* dn = pg::block_value(d, "targetname");
      const char* dc = pg::block_value(d, "classname");
      const char* org = pg::block_value(d, "origin");
      if (!dn || !dc || !org || std::strcmp(dn, target) != 0 ||
          std::strcmp(dc, "info_teleport_destination") != 0)
        continue;
      dest = origin_of(org);
      dest.z += 27;  // the mod lifts a destination 27 units
      found = true;
      break;
    }
    if (!found) continue;
    const int32_t to = sh::nav_nearest(g, dest);
    if (to < 0) continue;
    for (size_t a = 0; a < g.nodes.size(); a++) {
      const sh::Vec3f o = g.nodes[a].origin;
      if (o.x + 16 < box.mins.x || o.x - 16 > box.maxs.x || o.y + 16 < box.mins.y ||
          o.y - 16 > box.maxs.y || o.z + 32 < box.mins.z || o.z - 24 > box.maxs.z)
        continue;
      out.push_back({static_cast<int32_t>(a), to, sh::NavKind::walk, 0.1f});
    }
  }
  return out;
}

}  // namespace

std::vector<sh::NavMover> map_movers(const sv::QcServer& s, const sv::MapSpec& spec) {
  std::vector<pg::EntityBlock> blocks;
  std::string error;
  if (!pg::parse_entity_text(spec.entities.c_str(), blocks, error)) return {};
  return movers_of(s, spec, blocks);
}

bool build_map_nav(const sv::QcServer& s, const sv::MapSpec& spec, const char* start_class,
                   MapNav& out, std::string& error) {
  if (spec.models.empty()) {
    error = "the map " + spec.name + " has no world model; next: load a map first";
    return false;
  }
  std::vector<pg::EntityBlock> blocks;
  std::string perr;
  if (!pg::parse_entity_text(spec.entities.c_str(), blocks, perr)) blocks.clear();
  const auto t0 = std::chrono::steady_clock::now();
  sh::map_world(spec.models[0], out.world);
  out.bridges.clear();
  open_doors_as_floor(s, spec, blocks, out.world, out.bridges);
  out.movers = movers_of(s, spec, blocks);
  sh::build_nav(out.world, out.movers, sh::MoveSettings{}, sh::NavSettings{}, out.graph);
  const std::vector<sh::NavExtraLink> teleports = teleport_links(out.graph, spec, blocks);
  sh::nav_add_links(out.graph, teleports);
  for (NavBridge& br : out.bridges) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(br.ent)];
    // Out, and a player's half-width beyond its edge (a node there stands on it too).
    const sh::Vec3f lo = d.absmin - br.spawn - sh::Vec3f{16, 16, 0},
                    hi = d.absmax - br.spawn + sh::Vec3f{16, 16, 0};
    for (size_t n = 0; n < out.graph.nodes.size(); n++) {
      const sh::Vec3f o = out.graph.nodes[n].origin;
      if (o.x >= lo.x && o.x <= hi.x && o.y >= lo.y && o.y <= hi.y && o.z >= hi.z &&
          o.z <= hi.z + 40)
        br.nodes.push_back(static_cast<int32_t>(n));
    }
  }
  out.teleport_links = static_cast<int64_t>(teleports.size());
  // Water: the links prove no swim, so a pool is closed to entry (no route leads into it, and an
  // item under water is out of reach); a player knocked in finds his own way out (the fighter).
  sh::NavGraph& graph = out.graph;
  out.wet.assign(graph.nodes.size(), 0);
  for (size_t n = 0; n < graph.nodes.size(); n++) {
    const sh::Vec3f o = graph.nodes[n].origin;
    out.wet[n] = sh::water_contents(out.world, {o.x, o.y, o.z - 10}) <= sh::kContentsWater ? 1 : 0;
  }
  out.closed_links = sh::nav_close_entries(graph, out.wet, &out.entries);
  // The way out of the water: to any dry node of the map's main part (the most a start reaches).
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
  std::vector<uint8_t> exits(graph.nodes.size(), 0);
  for (size_t n = 0; n < graph.nodes.size(); n++)
    exits[n] = out.main_n > 0 && out.main.reached(static_cast<int32_t>(n)) && !out.wet[n] ? 1 : 0;
  sh::build_swim(out.world, graph, exits, sh::SwimSettings{}, out.swim);
  out.build_ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  if (graph.nodes.empty()) {
    error = "no floor a bot can walk on " + spec.name + "; next: run the se-nav lab on the map";
    return false;
  }
  return true;
}

}  // namespace shooters_edge::bots
