// The listen server (listen.hpp).
//
// ge:summary The listen server: the mod server and its local player, maps loaded and changed
// ge:keywords listen-server listen_frame listen_advance load_named_map baseline_progs forced-view
// ge:skill shooter progs
// ge:test tests/shooters_edge_listen_test.cpp
#include "shooters_edge/client/listen.hpp"

#include <cstddef>
#include <cstring>
#include <string_view>

#include "progs/layout.hpp"
#include "progs/qc_compile.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/mod_match.hpp"

// The baseline mod's sources, built into the game (CMakeLists.txt: ge_embed_binary), in the
// order its project file compiles them.
extern const unsigned char se_qc_defs[];
extern const std::size_t se_qc_defs_size;
extern const unsigned char se_qc_util[];
extern const std::size_t se_qc_util_size;
extern const unsigned char se_qc_combat[];
extern const std::size_t se_qc_combat_size;
extern const unsigned char se_qc_weapons[];
extern const std::size_t se_qc_weapons_size;
extern const unsigned char se_qc_items[];
extern const std::size_t se_qc_items_size;
extern const unsigned char se_qc_player[];
extern const std::size_t se_qc_player_size;
extern const unsigned char se_qc_world[];
extern const std::size_t se_qc_world_size;

namespace shooters_edge::client {

namespace pg = ge::progs;

namespace {

// The message bytes of the baseline mod's explosion (qc/defs.qc's SVC_TEMPENTITY, TE_EXPLOSION).
constexpr uint8_t kTempEntity = 23, kExplosion = 3;

pg::QcSource source(const char* name, const unsigned char* bytes, std::size_t size) {
  pg::QcSource s;
  s.name = name;
  s.text.reserve(size);
  for (std::size_t i = 0; i < size; i++)
    if (bytes[i] != '\r') s.text.push_back(static_cast<char>(bytes[i]));  // a Windows checkout
  return s;
}

std::string text_field(const Listen& l, int e, int field) {
  const pg::Vm& vm = l.server->q.vm;
  const int32_t v = static_cast<int32_t>(*pg::entity_field(vm, e, field));
  const char* t = pg::vm_string(vm, v);
  return t == nullptr ? "" : t;
}

bool start_map(Listen& l, std::string& error) {
  sv::QcServerOptions so;
  so.seed = l.opt.seed;
  so.loop.max_edicts = 1024;
  l.server = sv::make_qc_server(*l.progs, so, error);
  if (!l.server) return false;
  for (const std::string& line : l.opt.console) sv::server_command(*l.server, line);
  if (!sv::load_map(*l.server, l.map->spec, error)) {
    error = "the map " + l.map->name + " did not load: " + error +
            "; next: read the console's lines above";
    return false;
  }
  if (l.map->from_file) sv::use_map_sets(*l.server, l.map->bsp);
  sv::connect(*l.server, 0, l.opt.player, false, false);
  l.sequence = 0;
  for (sh::MoveCmd& c : l.cmds) c = {};
  l.frames = 0;
  l.owed = 0;
  l.pending_map.clear();
  l.explosions.clear();
  l.server->pending_map.clear();
  const sh::Edict& me = l.server->sv.edicts[1];
  l.view = me.v_angle;
  l.forced = true;
  return true;
}

}  // namespace

bool baseline_progs(pg::Progs& out, std::string& error) {
  const pg::QcSource sources[] = {
      source("defs.qc", se_qc_defs, se_qc_defs_size),
      source("util.qc", se_qc_util, se_qc_util_size),
      source("combat.qc", se_qc_combat, se_qc_combat_size),
      source("weapons.qc", se_qc_weapons, se_qc_weapons_size),
      source("items.qc", se_qc_items, se_qc_items_size),
      source("player.qc", se_qc_player, se_qc_player_size),
      source("world.qc", se_qc_world, se_qc_world_size),
  };
  pg::QcResult r = pg::compile_qc(sources);
  if (!r.ok) {
    error = "the baseline mod did not compile:";
    for (const pg::QcDiagnostic& d : r.diagnostics) error += "\n  " + pg::diagnostic_text(d);
    error += "\nnext: fix games/shooters_edge/qc at the lines named, then rebuild";
    return false;
  }
  out = std::move(r.progs);
  return true;
}

std::unique_ptr<LoadedMap> map_from_bytes(std::span<const uint8_t> bytes, const std::string& name,
                                          std::string& error) {
  auto m = std::make_unique<LoadedMap>();
  m->name = name;
  m->file.assign(bytes.begin(), bytes.end());
  if (!sv::read_bsp_map(m->file, name, &m->bsp, &m->spec, error)) return nullptr;
  m->from_file = true;
  return m;
}

std::unique_ptr<LoadedMap> load_named_map(const SearchPath& files, const std::string& name,
                                          std::string& error) {
  if (name == "arena") {
    auto m = std::make_unique<LoadedMap>();
    m->name = name;
    m->spec.name = name;
    m->spec.models = sv::arena_models(true);
    m->spec.entities = sv::kBaselineArenaEntities;
    return m;
  }
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes = find_file(files, "maps/" + name + ".bsp", store);
  if (bytes.empty()) {
    error = "no map maps/" + name + ".bsp in " +
            (files.folder.empty() ? std::string("the search path") : files.folder) +
            " or its packs; next: check the name (`map arena` is built in) and -basedir";
    return nullptr;
  }
  return map_from_bytes(bytes, name, error);
}

bool listen_start(Listen& l, const pg::Progs& progs, std::unique_ptr<LoadedMap> map,
                  const ListenOptions& opt, std::string& error) {
  l.progs = &progs;
  l.opt = opt;
  l.map = std::move(map);
  return start_map(l, error);
}

bool listen_change(Listen& l, std::unique_ptr<LoadedMap> map, std::string& error) {
  if (l.server) sv::save_change_parms(*l.server);
  l.server.reset();  // its views into the old map go before the map does
  l.map = std::move(map);
  return start_map(l, error);
}

void listen_frame(Listen& l, sh::MoveCmd cmd) {
  sv::QcServer& s = *l.server;
  cmd.msec = static_cast<uint8_t>(s.sv.set.mintic * 1000 + 0.5);
  cmd.angles = l.view;
  if (l.impulse != 0) {
    cmd.impulse = static_cast<uint8_t>(l.impulse);
    l.impulse = 0;
  }
  l.cmds[0] = l.cmds[1];
  l.cmds[1] = l.cmds[2];
  l.cmds[2] = cmd;
  sh::Packet p;
  p.client = 0;
  p.sequence = ++l.sequence;
  for (int i = 0; i < 3; i++) p.cmds[i] = l.cmds[i];
  sv::qc_frame(s, &p, 1);
  l.frames++;
  for (const sv::NetEvent& ev : s.out.events) {
    if ((ev.kind == sv::EventKind::print || ev.kind == sv::EventKind::centerprint) &&
        (ev.to & 1u) != 0)
      l.prints.push_back(ev.text);
    // The baseline mod's explosion: its temporary-entity message's first two bytes.
    if (ev.kind == sv::EventKind::multicast && ev.bytes.size() >= 2 && ev.bytes[0] == kTempEntity &&
        ev.bytes[1] == kExplosion)
      l.explosions.push_back(ev.origin);
  }
  sv::clear_frame(s.out);
  // The mod's forced view (a spawn, a teleport) leaves the player's angles off the ones sent.
  const sh::Edict& me = s.sv.edicts[1];
  l.forced = me.v_angle.x != cmd.angles.x || me.v_angle.y != cmd.angles.y;
  if (l.forced) l.view = {me.v_angle.x, me.v_angle.y, 0};
  if (!s.pending_map.empty()) l.pending_map = s.pending_map;
}

int listen_advance(Listen& l, double wall_s, const sh::MoveCmd& cmd) {
  const double tic = l.server->sv.set.mintic;
  l.owed += wall_s;
  int n = 0;
  while (l.owed >= tic && n < kMostFrames && l.pending_map.empty()) {
    l.owed -= tic;
    listen_frame(l, cmd);
    n++;
  }
  if (l.owed > tic) l.owed = 0;  // a stall: the time is dropped, never paid down
  return n;
}

PlayerState player_state(const Listen& l) {
  PlayerState p;
  const sh::Edict& me = l.server->sv.edicts[1];
  p.origin = me.origin;
  p.velocity = me.velocity;
  p.v_angle = me.v_angle;
  p.health = me.health;
  p.dead = me.health <= 0;
  p.on_ground = (me.flags & sh::kFlagOnGround) != 0;
  const pg::Vm& vm = l.server->q.vm;
  p.view_ofs = {pg::as_float(*pg::entity_field(vm, 1, pg::kFViewOfs)),
                pg::as_float(*pg::entity_field(vm, 1, pg::kFViewOfs + 1)),
                pg::as_float(*pg::entity_field(vm, 1, pg::kFViewOfs + 2))};
  return p;
}

void missiles_of(const Listen& l, std::vector<Missile>& out) {
  out.clear();
  const sv::QcServer& s = *l.server;
  for (int e = sh::kMaxClients + 1; e < s.sv.num_edicts; e++) {
    const sh::Edict& ed = s.sv.edicts[static_cast<size_t>(e)];
    if (ed.free) continue;
    const std::string cn = text_field(l, e, pg::kFClassname);
    if (cn != "rocket" && cn != "grenade" && cn != "spike" && cn != "missile") continue;
    out.push_back({e, ed.origin, cn != "spike"});
  }
}

float field_named(const Listen& l, int entity, const char* name) {
  const pg::Def* d = pg::find_field(*l.progs, name);
  if (d == nullptr) return 0;
  return pg::as_float(*pg::entity_field(l.server->q.vm, entity, d->offset));
}

}  // namespace shooters_edge::client
