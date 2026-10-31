// The listen server (listen.hpp).
//
// ge:summary The listen server: the mod server and its local player, maps loaded and changed
// ge:keywords listen-server listen_frame listen_advance load_named_map baseline_progs forced-view
// ge:skill shooter progs
// ge:test tests/shooters_edge_listen_test.cpp
#include "shooters_edge/client/listen.hpp"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string_view>

#include "core/num.hpp"
#include "progs/layout.hpp"
#include "progs/qc_compile.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/campaign_save.hpp"
#include "shooters_edge/server/classic_save.hpp"
#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/mod_match.hpp"

// The baseline mod's sources, built into the game (CMakeLists.txt: ge_embed_binary), in the
// order its project file compiles them.
extern const unsigned char se_qc_arena_engine[];
extern const std::size_t se_qc_arena_engine_size;
extern const unsigned char se_qc_common_defs[];
extern const std::size_t se_qc_common_defs_size;
extern const unsigned char se_qc_arena_glue[];
extern const std::size_t se_qc_arena_glue_size;
extern const unsigned char se_qc_common_util[];
extern const std::size_t se_qc_common_util_size;
extern const unsigned char se_qc_common_combat[];
extern const std::size_t se_qc_common_combat_size;
extern const unsigned char se_qc_common_weapons[];
extern const std::size_t se_qc_common_weapons_size;
extern const unsigned char se_qc_common_items[];
extern const std::size_t se_qc_common_items_size;
extern const unsigned char se_qc_common_player[];
extern const std::size_t se_qc_common_player_size;
extern const unsigned char se_qc_common_map[];
extern const std::size_t se_qc_common_map_size;
extern const unsigned char se_qc_arena_world[];
extern const std::size_t se_qc_arena_world_size;

namespace shooters_edge::client {

namespace pg = ge::progs;

namespace {

// The message bytes of the baseline mod's explosion (qc/common/defs.qc's SVC_TEMPENTITY,
// TE_EXPLOSION).
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

// What the client keeps of a map is new: the commands, the counts, the view the server set.
void entered(Listen& l) {
  l.sequence = 0;
  for (sh::MoveCmd& c : l.cmds) c = {};
  l.frames = 0;
  l.owed = 0;
  l.pending_map.clear();
  l.explosions.clear();
  l.blows.clear();
  l.effects.clear();
  l.server->pending_map.clear();
  const sh::Edict& me = l.server->sv.edicts[1];
  l.view = me.v_angle;
  l.forced = true;
  l.entered_at = l.server->sv.time;
  l.campaign = CampaignView{};
  const sv::CampaignStats st = sv::campaign_stats(*l.server);
  join_stats(l.campaign, st.total_secrets, st.total_monsters, st.found_secrets, st.killed_monsters);
}

// The lobby's lines, printed to the player like the server's.
void said(Listen& l) {
  if (!l.room) return;
  for (std::string& line : l.room->lobby.said) l.prints.push_back(std::move(line));
  l.room->lobby.said.clear();
}

bool start_map(Listen& l, std::string& error) {
  sv::QcServerOptions so;
  so.seed = l.opt.seed;
  so.loop.max_edicts = 1024;
  l.match = sv::make_qc_server(*l.progs, so, error);
  l.server = l.match.get();
  if (!l.server) return false;
  for (const std::string& line : l.opt.console) sv::server_command(*l.server, line);
  if (!sv::load_map(*l.server, l.map->spec, error)) {
    error = "the map " + l.map->name + " did not load: " + error +
            "; next: read the console's lines above";
    return false;
  }
  if (l.map->from_file) sv::use_map_sets(*l.server, l.map->bsp);
  sv::connect(*l.server, 0, l.opt.player, false, false);
  if (l.opt.lobby) {
    if (!l.room) {
      l.room = std::make_unique<Room>();
      l.room->seed = l.opt.seed;
    }
    l.room->lobby.settings = l.opt.lobby_settings;
    if (!l.room->lobby.seats[0].used)
      sv::lobby_join(l.room->lobby, 0, l.opt.player, false, l.server->sv.time);
    room_enter(*l.room, *l.server, l.map->spec);
  } else {
    l.room.reset();
  }
  entered(l);
  said(l);
  return true;
}

// The campaign host's map loader: the map read ahead when it is the one asked for, else read from
// the search path. The listen keeps it (the spec's models point into it) until the next call.
bool campaign_map(void* ctx, const std::string& name, sv::MapSpec& out, std::string& error) {
  Listen& l = *static_cast<Listen*>(ctx);
  std::unique_ptr<LoadedMap> m = std::move(l.next);
  if (!m || m->name != name) {
    if (l.opt.files == nullptr) {
      error = "no search path to read maps/" + name + ".bsp from; next: set ListenOptions::files";
      return false;
    }
    m = load_named_map(*l.opt.files, name, error);
    if (!m) return false;
  }
  l.map = std::move(m);
  out = l.map->spec;
  l.map_changed = true;
  return true;
}

// The campaign's map now playing, as the listen's: the server, its sets, the player's arrival.
void campaign_entered(Listen& l) {
  l.server = l.campaign_host->server.get();
  l.maps_started = l.campaign_host->history.size();
  if (l.map->from_file) sv::use_map_sets(*l.server, l.map->bsp);
  entered(l);
}

// The campaign host's options for the listen's settings (its loader the listen's).
sv::CampaignOptions campaign_options(Listen& l, const ListenOptions& opt) {
  sv::CampaignOptions co;
  co.server.seed = opt.seed;
  co.server.loop.max_edicts = 1024;
  co.load = campaign_map;
  co.load_ctx = &l;
  co.registered = opt.registered;
  co.settings = opt.console;
  return co;
}

// The program's client messages this frame (the all-clients stream, then the player's own), read
// and emptied: nothing else drains a listen server's reliable streams.
void read_streams(Listen& l) {
  sv::MessageSink& out = l.server->out;
  const double now = l.server->sv.time;
  if (l.campaign_host) {
    read_campaign(l.campaign, out.all, now);
    read_campaign(l.campaign, out.one[0], now);
    const size_t first = l.effects.size();
    read_effects(l.campaign, out.unreliable, l.effects);
    // The particle bursts the program asked for (the server's events: blood, a big burst).
    for (const sv::NetEvent& ev : out.events) {
      if (ev.kind != sv::EventKind::particle || (ev.to & 1u) == 0) continue;
      Effect e;
      e.code = -1;
      e.kind = ev.text == "explosion" ? EffectKind::explosion : EffectKind::blood;
      e.at = e.end = ev.origin;
      l.effects.push_back(e);
    }
    for (size_t i = first; i < l.effects.size(); i++)
      if (l.effects[i].kind == EffectKind::explosion) l.explosions.push_back(l.effects[i].at);
  }
  out.all.clear();
  out.one[0].clear();
}

}  // namespace

bool baseline_progs(pg::Progs& out, std::string& error) {
  const pg::QcSource sources[] = {
      source("engine.qc", se_qc_arena_engine, se_qc_arena_engine_size),
      source("../common/defs.qc", se_qc_common_defs, se_qc_common_defs_size),
      source("glue.qc", se_qc_arena_glue, se_qc_arena_glue_size),
      source("../common/util.qc", se_qc_common_util, se_qc_common_util_size),
      source("../common/combat.qc", se_qc_common_combat, se_qc_common_combat_size),
      source("../common/weapons.qc", se_qc_common_weapons, se_qc_common_weapons_size),
      source("../common/items.qc", se_qc_common_items, se_qc_common_items_size),
      source("../common/player.qc", se_qc_common_player, se_qc_common_player_size),
      source("../common/map.qc", se_qc_common_map, se_qc_common_map_size),
      source("world.qc", se_qc_arena_world, se_qc_arena_world_size),
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
  if (single_player(progs)) return listen_campaign(l, progs, std::move(map), opt, error);
  l.campaign_host.reset();
  l.progs = &progs;
  l.opt = opt;
  l.map = std::move(map);
  return start_map(l, error);
}

bool listen_change(Listen& l, std::unique_ptr<LoadedMap> map, std::string& error) {
  if (l.campaign_host) {
    const std::string name = map->name;
    l.next = std::move(map);
    if (!sv::campaign_change(*l.campaign_host, sv::MapChange::new_game, name, error)) return false;
    campaign_entered(l);
    l.map_changed = false;
    return true;
  }
  if (l.server) sv::save_change_parms(*l.server);
  l.server = nullptr;
  l.match.reset();                            // its views into the old map go before the map does
  if (l.room) sv::lobby_back(l.room->lobby);  // a new map: warmup again
  l.map = std::move(map);
  return start_map(l, error);
}

bool listen_campaign(Listen& l, const pg::Progs& progs, std::unique_ptr<LoadedMap> map,
                     const ListenOptions& opt, std::string& error) {
  l.server = nullptr;
  l.match.reset();
  l.room.reset();  // a campaign has no lobby
  l.campaign_host.reset();
  l.progs = &progs;
  l.opt = opt;
  l.failed.clear();
  const std::string name = map->name;
  l.next = std::move(map);
  sv::CampaignOptions co = campaign_options(l, opt);
  l.campaign_host = std::make_unique<sv::Campaign>();
  if (!sv::campaign_new_game(*l.campaign_host, progs, co, name, error)) {
    l.campaign_host.reset();
    return false;
  }
  sv::campaign_join(*l.campaign_host, 0, opt.player);
  campaign_entered(l);
  l.map_changed = false;
  return true;
}

std::string listen_save_problem(const Listen& l) {
  if (!l.campaign_host || l.server == nullptr)
    return "only a single-player game saves; next: start one (SINGLE PLAYER > NEW GAME)";
  if (l.campaign.end != LevelEnd::none) return "the level is over; next: save on the next map";
  return sv::campaign_save_problem(*l.campaign_host);
}

bool listen_save(const Listen& l, std::vector<uint8_t>& out, std::string& error,
                 int64_t saved_unix) {
  error = listen_save_problem(l);
  if (!error.empty()) {
    error = "save: " + error;
    return false;
  }
  return sv::campaign_save(*l.campaign_host, out, error, saved_unix);
}

namespace {

// The map a save names, read for its load and handed to the listen only once the load is good.
struct LoadAhead {
  const SearchPath* files = nullptr;
  std::unique_ptr<LoadedMap> map;
};

bool load_ahead(void* ctx, const std::string& name, sv::MapSpec& out, std::string& error) {
  LoadAhead& a = *static_cast<LoadAhead*>(ctx);
  if (!a.map || a.map->name != name) {
    if (a.files == nullptr) {
      error = "no search path to read maps/" + name + ".bsp from; next: set ListenOptions::files";
      return false;
    }
    a.map = load_named_map(*a.files, name, error);
    if (!a.map) return false;
  }
  out = a.map->spec;
  return true;
}

}  // namespace

namespace {

// A saved game loaded by `load` (our save or the classic text) as the game now playing; the game
// before it is left playing when the load fails.
template <class Load>
bool load_game(Listen& l, const pg::Progs& progs, const ListenOptions& opt,
               std::unique_ptr<LoadedMap> map, Load load) {
  LoadAhead ahead{opt.files, std::move(map)};
  sv::CampaignOptions co = campaign_options(l, opt);
  co.load = load_ahead;
  co.load_ctx = &ahead;
  auto host = std::make_unique<sv::Campaign>();
  if (!load(*host, co)) return false;
  host->opt.load = campaign_map;
  host->opt.load_ctx = &l;
  l.server = nullptr;
  l.match.reset();
  l.room.reset();
  l.campaign_host = std::move(host);
  l.progs = &progs;
  l.opt = opt;
  l.failed.clear();
  l.next.reset();
  l.map = std::move(ahead.map);
  campaign_entered(l);
  l.map_changed = true;  // the window draws the save's map
  // The player's commands go on from the saved sequence.
  l.sequence = l.server->sv.clients[0].last_sequence;
  return true;
}

}  // namespace

bool listen_load(Listen& l, const pg::Progs& progs, const ListenOptions& opt,
                 std::span<const uint8_t> bytes, std::string_view name, std::string& error,
                 std::unique_ptr<LoadedMap> map) {
  if (!load_game(l, progs, opt, std::move(map),
                 [&](sv::Campaign& c, const sv::CampaignOptions& co) {
                   return sv::campaign_load(c, progs, co, bytes, name, error);
                 }))
    return false;
  l.entered_at = l.server->sv.base_time;  // his level time from the map's start
  return true;
}

bool listen_save_text(const Listen& l, std::string& out, std::string& error) {
  error = listen_save_problem(l);
  if (!error.empty()) {
    error = "save: " + error;
    return false;
  }
  return sv::classic_save_text(*l.campaign_host, out, error);
}

bool listen_load_text(Listen& l, const pg::Progs& progs, const ListenOptions& opt,
                      std::string_view text, std::string_view name, std::string& error,
                      std::unique_ptr<LoadedMap> map) {
  if (!load_game(l, progs, opt, std::move(map),
                 [&](sv::Campaign& c, const sv::CampaignOptions& co) {
                   return sv::classic_load_text(c, progs, co, text, name, opt.player, error);
                 }))
    return false;
  // The text keeps no start: his level time from the settled start of the map.
  l.entered_at = sv::kSettleFrames * sv::kSettleSeconds + 1.0;
  return true;
}

bool single_player(const pg::Progs& progs) {
  return progs.layout != nullptr && progs.layout->single_player;
}

bool wants_campaign(const ListenOptions& opt) {
  bool campaign = false;
  for (const std::string& line : opt.console) {
    const std::string_view s = line;
    constexpr std::string_view kName = "deathmatch ";
    if (s.substr(0, kName.size()) == kName) campaign = std::atof(line.c_str() + kName.size()) == 0;
  }
  return campaign;
}

std::string map_title(const Listen& l) {
  const std::string t = text_field(l, 0, l.server->q.layout->f.message);
  return t.empty() ? l.map->name : t;
}

double listen_tic(const Listen& l) {
  return l.campaign_host ? sv::kCampaignStep : l.server->sv.set.mintic;
}

void listen_frame(Listen& l, sh::MoveCmd cmd) {
  cmd.msec = static_cast<uint8_t>(ge::core::num::round(listen_tic(l) * 1000));
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
  if (l.campaign_host) {
    if (!sv::campaign_frame(*l.campaign_host, &p, 1, l.failed)) {
      l.server = nullptr;
      return;
    }
    if (l.campaign_host->history.size() != l.maps_started) campaign_entered(l);
  } else if (l.room) {  // the bots' packets after the player's, in slot order
    std::vector<sh::Packet>& ps = l.room->packets;
    ps.clear();
    ps.push_back(p);
    room_packets(*l.room, *l.server, cmd.msec, ps);
    sv::qc_frame(*l.server, ps.data(), static_cast<int>(ps.size()));
    for (int k = 1; k < sh::kMaxClients; k++) l.server->out.one[k].clear();  // nobody reads them
  } else {
    sv::qc_frame(*l.server, &p, 1);
  }
  sv::QcServer& s = *l.server;
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
  read_streams(l);
  sv::clear_frame(s.out);
  if (const sv::ClientBlow& b = s.clients[0].blow; b.hit)
    l.blows.push_back({b.armor, b.blood, b.from});
  if (l.campaign.turned) {
    l.view = {l.campaign.angles.x, l.campaign.angles.y, 0};
    l.campaign.turned = false;
  }
  // The mod's forced view (a spawn, a teleport) leaves the player's angles off the ones sent.
  const sh::Edict& me = s.sv.edicts[1];
  l.forced = me.v_angle.x != cmd.angles.x || me.v_angle.y != cmd.angles.y;
  if (l.forced) l.view = {me.v_angle.x, me.v_angle.y, 0};
  if (!l.campaign_host && !s.pending_map.empty()) l.pending_map = s.pending_map;
  if (l.room) {
    // The countdown run out: the map restarted with every seat on it, the match's limits back.
    const bool due = l.pending_map.empty() && sv::lobby_due(l.room->lobby, s.sv.time);
    if (due) {
      std::string error;
      l.server = nullptr;
      l.match.reset();
      if (!start_map(l, error)) {
        l.failed = error;
        return;
      }
      l.restarted = true;
    }
    said(l);
  }
}

bool listen_lobby(Listen& l, std::string_view line) {
  const std::string_view word = line.substr(0, line.find(' '));
  const bool bot_word = word == "addbot" || word == "removebot";
  if (!l.room || l.server == nullptr) {
    if (bot_word || word == "ready" || word == "break" || word == "notready" ||
        word == "toggleready" || word == "team" || word == "start" || word == "lobby") {
      l.prints.push_back(std::string(word) +
                         ": no lobby runs; next: play a match (`map <name>` with deathmatch 1) "
                         "with `sv_lobby 1`");
      return true;
    }
    return false;
  }
  if (word == "addbot") {
    const size_t sp = line.find(' ');
    const std::string arg = sp == std::string_view::npos ? "" : std::string(line.substr(sp + 1));
    sh::AimSkill skill = sh::AimSkill::average;
    if (!arg.empty() && !skill_named(arg, skill)) {
      l.prints.push_back("addbot: no skill " + arg + "; next: addbot novice|average|expert");
      return true;
    }
    if (l.room->lobby.phase == sv::LobbyPhase::match) {
      l.prints.push_back("addbot: the match is on; next: add bots in the warmup");
      return true;
    }
    std::string error;
    if (room_add_bot(*l.room, *l.server, l.map->spec, skill, error) < 0)
      l.prints.push_back("addbot: " + error);
  } else if (word == "removebot") {
    if (!room_remove_bot(*l.room, *l.server)) l.prints.push_back("removebot: no bot is seated");
  } else if (!sv::lobby_command(l.room->lobby, 0, line, l.server->sv.time)) {
    return false;
  }
  said(l);
  return true;
}

int listen_advance(Listen& l, double wall_s, const sh::MoveCmd& cmd) {
  const double tic = listen_tic(l);
  l.owed += wall_s;
  int n = 0;
  while (l.owed >= tic && n < kMostFrames && l.pending_map.empty() && l.server != nullptr) {
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
  const uint32_t* view_ofs = pg::entity_field(vm, 1, pg::vm_layout(vm).f.view_ofs);
  p.view_ofs = {pg::as_float(view_ofs[0]), pg::as_float(view_ofs[1]), pg::as_float(view_ofs[2])};
  p.punch = me.punchangle;
  if (const int f = pg::vm_layout(vm).f.idealpitch; f >= 0)
    p.ideal_pitch = pg::as_float(*pg::entity_field(vm, 1, f));
  return p;
}

void missiles_of(const Listen& l, std::vector<Missile>& out) {
  out.clear();
  const sv::QcServer& s = *l.server;
  for (int e = sh::kMaxClients + 1; e < s.sv.num_edicts; e++) {
    const sh::Edict& ed = s.sv.edicts[static_cast<size_t>(e)];
    if (ed.free) continue;
    const std::string cn = text_field(l, e, l.server->q.layout->f.classname);
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
