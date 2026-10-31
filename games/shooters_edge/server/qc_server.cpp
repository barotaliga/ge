// The multiplayer mod server (qc_server.hpp): its settings, a map's loading and spawning, clients
// connecting and leaving, the frame, the hook log, and the clock source.
//
// ge:summary The mod server's settings, map loading and spawning, clients, frame and hook log
// ge:keywords create_qc_server make_qc_server load_map connect disconnect qc_frame
// ge:keywords save_change_parms apply_settings note_hook fixed_clock date_text inhibit
// ge:skill progs shooter
// ge:test tests/shooters_edge_qc_server_test.cpp
#include "shooters_edge/server/qc_server.hpp"

#include <cstdio>

#include "shooter/frame.hpp"
#include "shooters_edge/server/builtins.hpp"
#include "shooters_edge/server/catalogue.hpp"
#include "shooters_edge/server/client_data.hpp"
#include "shooters_edge/server/commands.hpp"

namespace shooters_edge::server {

namespace {

namespace con = ge::console;

constexpr int kFirstDynamic = 33;  // entities after the world and the 32 client slots
constexpr int kNotInDeathmatch = 2048, kNotEasy = 256, kNotMedium = 512, kNotHard = 1024;

void add_settings(QcServer& s) {
  const uint32_t srv = con::kServer, info = con::kServer | con::kServerInfo;
  s.cvars.add({"deathmatch", "1", info, "the match's rules: 0 cooperative, 1 and up deathmatch"});
  s.cvars.add({"skill", "1", srv, "the monsters' skill, 0..3"});
  s.cvars.add({"teamplay", "0", info, "team rules, as the mod reads them"});
  s.cvars.add({"coop", "0", srv, "a single-player program's cooperative game (on: deathmatch 0)"});
  s.cvars.add({"registered", "0", srv,
               "1 when the packs carry the full game (a single-player program opens its "
               "episodes); the host sets it"});
  s.cvars.add({"timelimit", "0", info, "the match's length, minutes (0 none)"});
  s.cvars.add({"fraglimit", "0", info, "frags that end the match (0 none)"});
  s.cvars.add({"samelevel", "0", info, "1: the map does not change at the end"});
  s.cvars.add({"sv_gravity", "800", info, "the world's gravity, units/s^2"});
  s.cvars.add({"sv_maxspeed", "320", info, "a player's run speed, units/s"});
  s.cvars.add({"sv_playermove", "auto", info,
               "the player move: arena (the predicted move), original (the server's own step) or "
               "auto (original for a single-player program); read at map start",
               con::CvarKind::text});
  s.cvars.add({"sv_idealpitchscale", "0.8", srv,
               "a single-player program's view tilt on a slope: degrees a unit of the floor's "
               "step (0 off)"});
  s.cvars.add({"edgefriction", "2", info, "the original move's friction factor at a ledge"});
  s.cvars.add(
      {"sv_nostep", "0", info, "1: the original move never steps up", con::CvarKind::boolean});
  s.cvars.add({"sv_phs", "1", srv, "1: sounds go only to clients that can hear them"});
  s.cvars.add({"sv_specprint", "0", srv,
               "bits: 1 spectators get a copy of a tracked player's centerprints, 2 sprints"});
  s.cvars.add({"sv_longfraglog", "0", srv, "1: the frag log's long lines (teams and the date)"});
  s.cvars.add({"sv_aim", "0.93", srv,
               "a single-player program's autoaim: the least cosine a target is aimed at (1 off)"});
  s.cvars.add({"sv_progs", "", srv, "the path of the progs to run (empty: the built-in test mod)",
               con::CvarKind::text});
  s.cvars.add({"hostname", "unnamed", info, "the server's name", con::CvarKind::text});
}

// Records the loop's own hook calls (StartFrame, the thinks, connect, ...) as they start.
void before_call(sim::QcProgram& q, sh::Func fn) {
  auto& s = *static_cast<QcServer*>(q.owner);
  const sh::Program& h = q.hooks;
  const int self = s.sv.g.self;
  const struct {
    sh::Func fn;
    HookId id;
  } loop_hooks[] = {{h.start_frame, HookId::start_frame},
                    {h.player_pre_think, HookId::player_pre_think},
                    {h.player_post_think, HookId::player_post_think},
                    {h.spectator_think, HookId::spectator_think},
                    {h.client_connect, HookId::client_connect},
                    {h.spectator_connect, HookId::spectator_connect},
                    {h.put_client_in_server, HookId::put_client_in_server},
                    {h.client_disconnect, HookId::client_disconnect},
                    {h.spectator_disconnect, HookId::spectator_disconnect},
                    {h.set_new_parms, HookId::set_new_parms},
                    {h.set_change_parms, HookId::set_change_parms}};
  for (const auto& lh : loop_hooks) {
    if (lh.fn == sh::kNoFunc || lh.fn != fn) continue;
    note_hook(s, lh.id, self);
    break;
  }
}

// Saves the spawn parms after SetNewParms and SetChangeParms.
void after_call(sim::QcProgram& q, sh::Func fn) {
  auto& s = *static_cast<QcServer*>(q.owner);
  const sh::Program& h = q.hooks;
  const int self = s.sv.g.self;
  const int slot = client_slot_of(s, self);
  if ((fn == h.set_new_parms || fn == h.set_change_parms) && slot >= 0) {
    for (int i = 0; i < 16; i++)
      s.clients[slot].spawn_parms[i] = pg::global_float(q.vm, s.q.layout->g.parm1 + i);
  }
}

// The rules a map starts under, in the globals the program's form has: `serverflags`, and in the
// single-player form `deathmatch`, `coop` and `teamplay` (`coop` on forces `deathmatch` 0).
void write_rules(QcServer& s) {
  pg::Vm& vm = s.q.vm;
  const pg::LayoutGlobals& g = s.q.layout->g;
  auto number = [&](const char* name) { return static_cast<float>(s.cvars.find(name)->number); };
  const bool coop = s.cvars.find("coop")->on();
  if (g.serverflags >= 0) pg::set_global_float(vm, g.serverflags, s.serverflags);
  if (g.deathmatch >= 0) pg::set_global_float(vm, g.deathmatch, coop ? 0.0f : number("deathmatch"));
  if (g.coop >= 0) pg::set_global_float(vm, g.coop, coop ? number("coop") : 0.0f);
  if (g.teamplay >= 0) pg::set_global_float(vm, g.teamplay, number("teamplay"));
}

int32_t named(const pg::Progs& p, const char* name) {
  const int fn = pg::find_function(p, name);
  return fn < 0 ? 0 : fn;
}

void update_visclients(QcServer& s) {
  const int f = s.q.fields.visclients;
  if (f < 0) return;
  pg::Vm& vm = s.q.vm;
  for (int e = 0; e < s.sv.num_edicts; e++) {
    if (e != 0 && s.sv.edicts[static_cast<size_t>(e)].free) continue;
    const sh::Vec3f at = s.sv.edicts[static_cast<size_t>(e)].origin;
    uint32_t bits = 0;
    for (int i = 0; i < sh::kMaxClients; i++) {
      if (!s.clients[i].active || i + 1 == e) continue;
      if (sees(s, eye_of(s, i + 1), at)) bits |= 1u << i;
    }
    *pg::entity_field(vm, e, f) = pg::as_word(static_cast<float>(bits));
  }
}

// A forced view (`fixangle`, set by a program that turns a player: a spawn, a teleport) is sent
// to the client once, at the end of the frame: the view becomes `angles` and the flag clears, so
// the client's next command turns the view again. Left set, every command's angles would be
// ignored and the player would aim along his spawn's angles forever.
void send_forced_views(QcServer& s) {
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (!s.clients[i].active) continue;
    sh::Edict& ed = s.sv.edicts[static_cast<size_t>(i) + 1];
    if (!ed.fixangle) continue;
    ed.v_angle = ed.angles;
    ed.fixangle = false;
  }
}

void run_command_buffer(QcServer& s) {
  while (!s.command_buffer.empty()) {
    const size_t nl = s.command_buffer.find('\n');
    const std::string line = s.command_buffer.substr(0, nl);
    s.command_buffer.erase(0, nl == std::string::npos ? std::string::npos : nl + 1);
    server_command(s, line);
  }
}

}  // namespace

DateTime fixed_clock(void* /*ctx*/, double server_time) {
  DateTime d;
  d.year = 2026;
  d.month = 1;
  d.day = 2;
  const auto t = static_cast<int64_t>(server_time);
  d.hour = 12 + static_cast<int>(t / 3600 % 12);
  d.minute = static_cast<int>(t / 60 % 60);
  d.second = static_cast<int>(t % 60);
  return d;
}

std::string date_text(const DateTime& d) {
  static constexpr const char* kDays[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static constexpr const char* kMonths[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  // The weekday by Zeller's congruence (January and February counted in the year before).
  int y = d.year, m = d.month;
  if (m < 3) {
    m += 12;
    y -= 1;
  }
  const int h = (d.day + 13 * (m + 1) / 5 + y + y / 4 - y / 100 + y / 400) % 7;  // 0 Saturday
  const int weekday = (h + 6) % 7;
  const int month = d.month >= 1 && d.month <= 12 ? d.month - 1 : 0;
  char buf[64];
  std::snprintf(buf, sizeof buf, "%s %s %02d, %02d:%02d:%02d %04d", kDays[weekday], kMonths[month],
                d.day, d.hour, d.minute, d.second, d.year);
  return buf;
}

const char* hook_name(HookId h) {
  static constexpr const char* kNames[] = {
      "StartFrame",        "PlayerPreThink",   "PlayerPostThink",
      "SpectatorThink",    "ClientConnect",    "SpectatorConnect",
      "PutClientInServer", "ClientDisconnect", "SpectatorDisconnect",
      "SetNewParms",       "SetChangeParms",   "ClientKill",
      "UserCmd",           "GE_ClientCommand", "ChatMessage",
      "UserInfo_Changed",  "localinfoChanged", "ConsoleCmd",
      "GE_ShouldPause",    "GE_PausedTic",     "timeofday",
      "spawn function"};
  static_assert(sizeof kNames / sizeof kNames[0] == static_cast<size_t>(HookId::count));
  return kNames[static_cast<int>(h)];
}

void note_hook(QcServer& s, HookId h, int self) {
  s.hook_counts[static_cast<int>(h)]++;
  if (static_cast<int>(s.hook_log.size()) < kMaxHookLog) s.hook_log.push_back({s.sv.time, h, self});
}

QcServer& server_of(pg::Vm& vm) { return *static_cast<QcServer*>(sim::qc_of(vm).owner); }

int client_slot_of(const QcServer& s, int entity) {
  if (entity < 1 || entity > sh::kMaxClients) return -1;
  return s.clients[entity - 1].active ? entity - 1 : -1;
}

void console_print(QcServer& s, const std::string& line) { s.console.push_back(line); }

void set_netname(QcServer& s, int e, const std::string& name) {
  *pg::entity_field(s.q.vm, e, s.q.layout->f.netname) =
      static_cast<uint32_t>(pg::engine_string(s.q.vm, name.c_str()));
}

std::string info_value(const std::vector<std::pair<std::string, std::string>>& info,
                       const std::string& key) {
  for (const auto& [k, v] : info)
    if (k == key) return v;
  return {};
}

bool set_info_value(std::vector<std::pair<std::string, std::string>>& info, const std::string& key,
                    const std::string& value, std::string& old) {
  for (auto& [k, v] : info) {
    if (k != key) continue;
    if (v == value) return false;
    old = v;
    v = value;
    return true;
  }
  old.clear();
  if (value.empty()) return false;
  info.emplace_back(key, value);
  return true;
}

void apply_settings(QcServer& s) {
  s.sv.set.move.gravity = static_cast<float>(s.cvars.find("sv_gravity")->number);
  s.sv.set.move.maxspeed = static_cast<float>(s.cvars.find("sv_maxspeed")->number);
}

bool create_qc_server(QcServer& s, const pg::Progs& progs, const QcServerOptions& opt,
                      std::string& error) {
  s.opt = opt;
  s.progs = &progs;
  if (s.cvars.find("deathmatch") == nullptr) add_settings(s);
  if (pg::find_global(progs, "self") == nullptr) {
    error = "mod server: the progs has no engine-side globals; load a multiplayer progs";
    return false;
  }
  return true;
}

std::unique_ptr<QcServer> make_qc_server(const pg::Progs& progs, const QcServerOptions& opt,
                                         std::string& error) {
  auto s = std::make_unique<QcServer>();
  if (!create_qc_server(*s, progs, opt, error)) return nullptr;
  return s;
}

bool load_map(QcServer& s, const MapSpec& map, std::string& error) {
  const pg::Progs& progs = *s.progs;
  sh::ServerSettings set = s.opt.loop;
  set.first_dynamic = kFirstDynamic;
  set.full_reuses_last = true;
  if (set.max_edicts > pg::kMaxEntitiesDefault) set.max_edicts = pg::kMaxEntitiesDefault;

  // The adapter and its VM afresh: the progs' globals as the file has them, every string gone.
  if (!sim::create_qc_program(s.q, progs, set.max_edicts, error)) return false;
  s.q.owner = &s;
  s.q.before_call = &before_call;
  s.q.after_call = &after_call;
  s.q.rng = ge::core::make_rng(s.opt.seed);
  if (!install_catalogue(s, error)) return false;
  NamedHooks& n = s.named;
  n.user_cmd = named(progs, "UserCmd");
  n.client_command = named(progs, "GE_ClientCommand");
  n.chat_message = named(progs, "ChatMessage");
  n.user_info_changed = named(progs, "UserInfo_Changed");
  n.localinfo_changed = named(progs, "localinfoChanged");
  n.console_cmd = named(progs, "ConsoleCmd");
  n.paused_tic = named(progs, "GE_PausedTic");
  n.should_pause = named(progs, "GE_ShouldPause");
  n.timeofday = named(progs, "timeofday");

  // The player move, read at map start: auto follows the program's form.
  set.program_jumps = s.q.layout->single_player;
  const std::string& move = s.cvars.find("sv_playermove")->value;
  set.playermove = move == "original" || (move != "arena" && set.program_jumps)
                       ? sh::PlayerMove::original
                       : sh::PlayerMove::arena;
  set.edgefriction = static_cast<float>(s.cvars.find("edgefriction")->number);
  set.nostep = s.cvars.find("sv_nostep")->on();
  sh::create_server(s.sv, set, map.models, sim::qc_program(s.q));
  apply_settings(s);
  s.map_name = map.name;
  s.models.assign(1, "");
  s.models.push_back("maps/" + map.name + ".bsp");
  for (size_t i = 1; i < map.models.size(); i++) s.models.push_back("*" + std::to_string(i));
  s.models.reserve(kMaxModels);
  s.sounds.assign(1, "");
  s.sounds.reserve(kMaxSounds);  // never reallocated: the loop's sounds point into it
  s.vwep.clear();
  s.extras.assign(static_cast<size_t>(set.max_edicts), pg::EntityExtras{});
  s.statics.clear();
  for (std::string& l : s.lightstyles) l.clear();
  s.out = MessageSink{};
  s.map_messages.clear();
  s.loading = true;
  s.map_stopped = false;
  s.stop_reason.clear();
  s.changelevel_queued = false;
  s.pending_map.clear();
  s.pending_change = MapChange::new_game;
  s.paused = false;
  s.inhibited = 0;
  s.check_client = 0;
  s.check_time = -1;
  s.hook_log.clear();
  for (int64_t& c : s.hook_counts) c = 0;

  std::vector<pg::EntityBlock> blocks;
  if (!pg::parse_entity_text(map.entities.c_str(), blocks, error)) return false;
  const bool deathmatch = s.cvars.find("deathmatch")->on();
  const int skill = s.cvars.find("skill")->integer();
  pg::Vm& vm = s.q.vm;

  for (size_t i = 0; i < blocks.size(); i++) {
    const pg::EntityBlock& b = blocks[i];
    s.sv.g.self = 0;
    s.sv.g.other = 0;
    s.sv.g.time = static_cast<float>(s.sv.time);
    sim::enter_qc(s.q, s.sv);
    if (i == 0) {
      pg::set_global_int(vm, s.q.layout->g.mapname, pg::engine_string(vm, map.name.c_str()));
      write_rules(s);
      *pg::entity_field(vm, 0, s.q.layout->f.model) =
          static_cast<uint32_t>(pg::engine_string(vm, s.models[1].c_str()));
    }
    int e = 0;
    if (i > 0) {
      e = sim::qc_spawn_entity(s.q);
      if (e < 0) {
        sim::leave_qc(s.q);
        error = "load_map: no free entity for block " + std::to_string(i);
        return false;
      }
    }
    if (b.pairs.empty()) {
      if (e > 0) sim::qc_free_entity(s.q, e);
      sim::leave_qc(s.q);
      continue;
    }
    if (!pg::apply_entity_block(vm, e, b, s.extras[static_cast<size_t>(e)], s.map_messages,
                                error)) {
      sim::leave_qc(s.q);
      return false;
    }
    bool keep = true;
    if (e > 0) {
      const int flags =
          static_cast<int>(pg::as_float(*pg::entity_field(vm, e, s.q.layout->f.spawnflags)));
      const int skip = deathmatch   ? kNotInDeathmatch
                       : skill <= 0 ? kNotEasy
                       : skill == 1 ? kNotMedium
                                    : kNotHard;
      if ((flags & skip) != 0) {
        s.inhibited++;
        keep = false;
      }
    }
    int32_t fn = -1;
    const char* classname =
        pg::vm_string(vm, static_cast<int32_t>(*pg::entity_field(vm, e, s.q.layout->f.classname)));
    if (keep && (classname == nullptr || classname[0] == 0)) {
      s.map_messages.push_back("no classname for entity " + std::to_string(e));
      keep = false;
    }
    if (keep) {
      fn = pg::find_function(progs, classname);
      if (fn < 0) {
        s.map_messages.push_back(std::string("no spawn function for ") + classname);
        keep = false;
      }
    }
    if (!keep) {
      if (e > 0) sim::qc_free_entity(s.q, e);
      sim::leave_qc(s.q);
      continue;
    }
    pg::set_global_int(vm, s.q.layout->g.self, pg::entity_value(vm, e));
    pg::set_global_int(vm, s.q.layout->g.other, 0);
    pg::set_global_float(vm, s.q.layout->g.time, static_cast<float>(s.sv.time));
    note_hook(s, HookId::spawn_function, e);
    sim::run_qc(s.q, fn);
    sim::leave_qc(s.q);
    if (s.map_stopped) {
      error = "load_map: " + s.stop_reason;
      return false;
    }
  }
  s.loading = false;
  vm.world_locked = true;
  return true;
}

void connect(QcServer& s, int slot, const std::string& name, bool spectator, bool bot,
             const float* parms) {
  ClientInfo& c = s.clients[slot];
  c = ClientInfo{};
  c.active = true;
  c.spectator = spectator;
  c.bot = bot;
  c.userid = s.next_userid++;
  c.entered_at = s.sv.time;
  c.userinfo.emplace_back("name", name);
  if (bot) c.address = "bot";
  // The player's entity starts clean in the VM with its name, so every hook from SetNewParms on
  // reads its netname.
  const int e = slot + 1;
  pg::clear_entity(s.q.vm, e);
  s.q.live[static_cast<size_t>(e)] = 1;
  set_netname(s, e, name);
  if (parms != nullptr) {
    for (int k = 0; k < 16; k++) {
      c.spawn_parms[k] = parms[k];
      pg::set_global_float(s.q.vm, s.q.layout->g.parm1 + k, parms[k]);
    }
  }
  sh::connect_client(s.sv, slot, spectator, bot, parms == nullptr);
}

void disconnect(QcServer& s, int slot) {
  if (!s.clients[slot].active) return;
  sh::disconnect_client(s.sv, slot);
  s.clients[slot] = ClientInfo{};
  if (s.paused) request_pause(s, -1, false);
}

void save_change_parms(QcServer& s) {
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (!s.clients[i].active) continue;
    call_hook(s, HookId::set_change_parms, s.q.hooks.set_change_parms, i + 1, {});
    for (int k = 0; k < 16; k++)
      s.clients[i].spawn_parms[k] = pg::global_float(s.q.vm, s.q.layout->g.parm1 + k);
  }
}

void qc_frame(QcServer& s, const sh::Packet* packets, int count) {
  if (s.paused) {
    s.paused_seconds += s.sv.set.mintic;
    if (s.named.paused_tic > 0)
      call_hook(s, HookId::paused_tic, s.named.paused_tic, 0,
                {static_cast<float>(s.paused_seconds)});
  } else {
    sh::server_frame(s.sv, packets, count);
  }
  update_visclients(s);
  take_client_data(s);
  send_forced_views(s);
  for (int i = 0; i < sh::kMaxClients; i++)
    if (s.clients[i].active && s.clients[i].drop) disconnect(s, i);
  run_command_buffer(s);
}

}  // namespace shooters_edge::server
