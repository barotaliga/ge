// The compiled-module server (qvm_server.hpp): its settings, a map's start in the module's order,
// clients, the frame, client commands and the console, and its table of runner calls.
//
// ge:summary The compiled-module server's map start, clients, frame, commands and runner table
// ge:keywords qvm_load_map qvm_connect qvm_disconnect qvm_frame qvm_client_command qvm_console
// ge:keywords qvm_set_user_info qvm_pause qvm_carry qvm_runner fill_qvm_world
// ge:skill progs shooter
// ge:test tests/shooters_edge_qvm_server_test.cpp
#include "shooters_edge/server/qvm_server.hpp"

#include <cstring>

#include "progs/layout.hpp"
#include "shooter/client.hpp"
#include "shooter/frame.hpp"

namespace shooters_edge::server {

namespace {

namespace pg = ge::progs;
namespace con = ge::console;

const pg::LayoutFields& F = pg::kMultiplayerLayout.f;
const pg::LayoutGlobals& G = pg::kMultiplayerLayout.g;

constexpr int kFirstDynamic = 33;  // entities after the world and the 32 client slots

// The client commands the server keeps (the module never sees them), and the ones it runs when
// the module did not take them.
constexpr const char* kFixed[] = {
    "new",     "modellist",  "soundlist", "prespawn", "spawn",  "begin", "drop",         "pings",
    "setinfo", "serverinfo", "download",  "nextdl",   "ptrack", "snap",  "stopdownload", "upload"};
constexpr const char* kOverridable[] = {"rate", "pause", "say", "say_team", "join", "observe"};

template <size_t N>
bool listed(const char* const (&list)[N], const std::string& w) {
  for (const char* x : list)
    if (w == x) return true;
  return false;
}

void push(QvmServer& s, NetEvent ev) {
  ev.time = s.sv.time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

uint32_t everyone(const QvmServer& s) {
  uint32_t to = 0;
  for (int i = 0; i < sh::kMaxClients; i++)
    if (s.clients[i].active) to |= 1u << i;
  return to;
}

void print_to(QvmServer& s, uint32_t to, int level, const std::string& text) {
  NetEvent ev;
  ev.kind = EventKind::print;
  ev.to = to;
  ev.reliable = true;
  ev.level = level;
  ev.text = text;
  push(s, std::move(ev));
}

// Saves a client's spawn parms after the parms exports.
void after_parms(sim::QvmProgram& q, int self) {
  auto& s = *static_cast<QvmServer*>(q.owner);
  if (self < 1 || self > sh::kMaxClients || !s.clients[self - 1].active) return;
  for (int i = 0; i < 16; i++) s.clients[self - 1].spawn_parms[i] = sim::global_f(q, G.parm1 + i);
}

// The module's export with `self` the client (or the world) and `time` the server's; the server's
// own self and other restored after.
int32_t call_as(QvmServer& s, int self, int32_t command, int32_t a0 = 0, int32_t a1 = 0) {
  sh::Server& sv = s.sv;
  const int old_self = sv.g.self, old_other = sv.g.other;
  sv.g.self = self;
  sv.g.other = 0;
  sv.g.time = static_cast<float>(sv.time);
  const int32_t r = sim::qvm_export(s.q, sv, command, a0, a1);
  sv.g.self = old_self;
  sv.g.other = old_other;
  return r;
}

void set_words(QvmServer& s, const std::string& line) { pg::tokenize_words(line.c_str(), s.words); }

// A forced view (`fixangle`) is sent once, at the end of the frame: the view becomes `angles` and
// the flag clears.
void send_forced_views(QvmServer& s) {
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (!s.clients[i].active) continue;
    sh::Edict& ed = s.sv.edicts[static_cast<size_t>(i) + 1];
    if (!ed.fixangle) continue;
    ed.v_angle = ed.angles;
    ed.fixangle = false;
    if (s.q.data.ents != 0) sim::set_field_f(s.q, i + 1, F.fixangle, 0);
  }
}

// The chat a module left to the server: everyone (or the speaker's team) gets "<name>: <text>".
void server_say(QvmServer& s, int slot, bool team) {
  std::string text = s.words.args;
  if (text.size() >= 2 && text.front() == '"' && text.back() == '"')
    text = text.substr(1, text.size() - 2);
  const ClientInfo& c = s.clients[slot];
  uint32_t to = everyone(s);
  if (team) {
    const std::string mine = info_value(c.userinfo, "team");
    to = 0;
    for (int k = 0; k < sh::kMaxClients; k++)
      if (s.clients[k].active && info_value(s.clients[k].userinfo, "team") == mine) to |= 1u << k;
  }
  const std::string who = info_value(c.userinfo, "name");
  print_to(s, to, 3, (team ? "(" + who + ")" : who) + ": " + text + "\n");
}

void run_server_command(QvmServer& s, int slot) {
  const std::string cmd = pg::word(s.words, 0);
  ClientInfo& c = s.clients[slot];
  if (cmd == "pause") {
    qvm_pause(s, slot, !s.paused);
  } else if (cmd == "setinfo") {
    if (s.words.count >= 3) qvm_set_user_info(s, slot, pg::word(s.words, 1), pg::word(s.words, 2));
  } else if (cmd == "rate") {
    if (s.words.count >= 2) qvm_set_user_info(s, slot, "rate", pg::word(s.words, 1));
  } else if (cmd == "drop") {
    c.drop = true;
  } else if (cmd == "say" || cmd == "say_team") {
    server_say(s, slot, cmd == "say_team");
  } else {
    print_to(s, 1u << slot, 2, cmd + ": not available on this server yet\n");
  }
}

// `exec <file>`: its lines appended to the command buffer and run.
bool exec_file(QvmServer& s, const std::string& name) {
  std::vector<uint8_t> bytes;
  if (s.opt.read_file == nullptr || !s.opt.read_file(s.opt.files_ctx, name, bytes)) {
    qvm_print(s, "couldn't exec " + name);
    return false;
  }
  std::string text(bytes.begin(), bytes.end());
  if (!text.empty() && text.back() != '\n') text += '\n';
  s.command_buffer.insert(0, text);
  return true;
}

bool run_console_line(QvmServer& s, const std::string& line) {
  set_words(s, line);
  if (s.words.count == 0) return true;
  const std::string cmd = pg::word(s.words, 0);
  if (cmd == "set" && s.words.count >= 3) {
    s.cvars.set(pg::word(s.words, 1), pg::word(s.words, 2), con::Source::local, true);
    settings_from_cvars(s, s.sv.set, true);
    return true;
  }
  if (cmd == "serverinfo" && s.words.count >= 3) {
    std::string old;
    set_info_value(s.serverinfo, pg::word(s.words, 1), pg::word(s.words, 2), old);
    return true;
  }
  if (cmd == "localinfo" && s.words.count >= 3) {
    std::string old;
    set_info_value(s.localinfo, pg::word(s.words, 1), pg::word(s.words, 2), old);
    return true;
  }
  if ((cmd == "map" || cmd == "changelevel") && s.words.count >= 2) {
    s.pending_map = pg::word(s.words, 1);
    s.pending_change = cmd == "map" ? MapChange::new_game : MapChange::change_level;
    return true;
  }
  if (cmd == "restart") {
    s.pending_map = s.map_name;
    s.pending_change = MapChange::restart;
    return true;
  }
  if (cmd == "exec" && s.words.count >= 2) return exec_file(s, pg::word(s.words, 1));
  // A mod's config sets the chat flood rule; the endpoint's own `floodprot` setting enforces it
  // on the wire, so the module's line is kept (localinfo `floodprot`) and acknowledged.
  if (cmd == "floodprot") {
    std::string old;
    if (s.words.count >= 4) set_info_value(s.localinfo, "floodprot", s.words.args, old);
    return true;
  }
  if (cmd == "echo") {
    qvm_print(s, s.words.args);
    return true;
  }
  if (const con::Cvar* c = s.cvars.find(cmd); c != nullptr) {
    if (s.words.count >= 2) {
      s.cvars.set(cmd, pg::word(s.words, 1), con::Source::local);
      settings_from_cvars(s, s.sv.set, true);
    } else {
      qvm_print(s, cmd + " is \"" + c->value + "\"");
    }
    return true;
  }
  if (cmd == "mod" && s.q.data.ents != 0) {
    call_as(s, 0, sim::kQvmConsoleCommand);
    return true;
  }
  qvm_print(s, "Unknown command \"" + cmd + "\"");
  return false;
}

int model_index_of(const QvmServer& s, std::string_view name, int was) {
  if (name.empty()) return 0;
  const auto at = static_cast<size_t>(was);
  if (was > 0 && at < s.models.size() && s.models[at] == name) return was;
  for (size_t i = 1; i < s.models.size(); i++)
    if (s.models[i] == name) return static_cast<int>(i);
  return 0;
}

}  // namespace

void qvm_print(QvmServer& s, const std::string& line) {
  if (s.capturing) {
    if (s.capture.size() + line.size() + 1 > 1340) s.capture.clear();
    s.capture += line;
    if (line.empty() || line.back() != '\n') s.capture += '\n';
    return;
  }
  s.console.push_back(line);
}

void qvm_run_buffer(QvmServer& s) {
  for (int guard = 0; !s.command_buffer.empty() && guard < 10000; guard++) {
    const size_t nl = s.command_buffer.find('\n');
    const std::string line = s.command_buffer.substr(0, nl);
    s.command_buffer.erase(0, nl == std::string::npos ? std::string::npos : nl + 1);
    run_console_line(s, line);
  }
}

QvmServer::~QvmServer() { qvm_close_files(*this); }

bool create_qvm_server(QvmServer& s, std::vector<uint8_t> module, const QvmServerOptions& opt,
                       std::string& error) {
  s.opt = opt;
  s.module = std::move(module);
  if (!pg::load_qvm(s.module, s.q.vm, error)) {
    error = "the compiled module is refused: " + error;
    return false;
  }
  if (s.cvars.find("deathmatch") == nullptr) {
    add_mod_settings(s, s.opt.loop);
    s.cvars.add({"sv_pr2references", "0", con::kServer,
                 "1: the module's string fields are references (the module sets it at init)"});
    s.cvars.add({"developer", "0", con::kServer, "1: the module's developer prints shown"});
    // The rate cap as the module reads it (it checks each client's `rate` against it): the
    // endpoint's `sv_maxrate` when set, else the most the endpoint accepts.
    s.cvars.add({"sv_maxrate", "300000", con::kServer,
                 "the most bytes/s a client's rate may be, as the module reads it"});
    // The server's name and version as a module shows them (its `about` page), and the cvars a
    // module sets at init to describe itself: a name the module sets is kept, never refused.
    s.cvars.add({"version", s.opt.version, con::kServer, "the server's name and version",
                 con::CvarKind::text});
    for (const char* name : {"qwm_name", "qwm_fullname", "qwm_version", "qwm_buildnum",
                             "qwm_platform", "qwm_builddate", "qwm_homepage"})
      s.cvars.add({name, "", con::kServer, "the module's own description, set at its init",
                   con::CvarKind::text});
  }
  return true;
}

std::unique_ptr<QvmServer> make_qvm_server(std::vector<uint8_t> module, const QvmServerOptions& opt,
                                           std::string& error) {
  auto s = std::make_unique<QvmServer>();
  if (!create_qvm_server(*s, std::move(module), opt, error)) return nullptr;
  return s;
}

bool qvm_load_map(QvmServer& s, const MapSpec& map, std::string& error) {
  sh::ServerSettings set = s.opt.loop;
  set.first_dynamic = kFirstDynamic;
  set.full_reuses_last = true;
  if (set.max_edicts > sh::kMaxEdicts) set.max_edicts = sh::kMaxEdicts;

  // The module afresh: its memory as the file has it.
  if (!pg::reset_qvm(s.module, s.q.vm, error)) return false;
  sim::QvmProgram& q = s.q;
  q.vm.syscall = &qvm_syscall;
  q.vm.user = &s;
  q.owner = &s;
  q.after_parms = &after_parms;
  q.data = sim::QvmGameData{};
  q.references = false;
  q.calls = q.faults = 0;
  q.last_fault.clear();
  for (int64_t& c : q.export_counts) c = 0;
  for (int64_t& c : s.syscall_counts) c = 0;

  const std::string& move = s.cvars.find("sv_playermove")->value;
  set.playermove = move == "original" ? sh::PlayerMove::original : sh::PlayerMove::arena;
  settings_from_cvars(s, set, false);
  sh::create_server(s.sv, set, map.models, sim::qvm_program(q));
  settings_from_cvars(s, s.sv.set, true);
  q.home = &s.sv;
  q.sv = nullptr;
  q.nesting = 0;
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
  s.entities = map.entities;
  s.entity_at = 0;
  qvm_close_files(s);
  for (int32_t& b : s.bound) b = 0;
  s.loading = false;
  s.map_stopped = false;
  s.stop_reason.clear();
  s.changelevel_queued = false;
  s.pending_map.clear();
  s.pending_change = MapChange::new_game;
  s.paused = false;
  s.check_client = 0;
  s.check_time = -1;
  s.frames = 0;
  s.cvars.set("sv_pr2references", "0", con::Source::code);

  // Init: the level time in ms and the seed. The module may not touch entities yet.
  const auto level_ms = static_cast<int32_t>(set.start_time * 1000);
  const int32_t block =
      sim::qvm_reenter(q, sim::kQvmInit, level_ms, static_cast<int32_t>(s.opt.seed));
  if (!q.last_fault.empty()) {
    error = "the module's init faulted: " + q.last_fault;
    return false;
  }
  if (s.map_stopped) {
    error = "the module stopped at init: " + s.stop_reason;
    return false;
  }
  if (!sim::bind_game_data(q, block, set.max_edicts, error)) return false;
  q.references = s.cvars.find("sv_pr2references")->on();
  for (int e = 0; e < q.max_edicts; e++) {
    sim::qvm_clear_entity(q, e);
    q.live[static_cast<size_t>(e)] = e == 0 ? 1 : 0;
  }
  if (!q.last_fault.empty()) {
    error = "the module's clear edict faulted: " + q.last_fault;
    return false;
  }
  sim::set_global_f(q, G.serverflags, s.serverflags);

  // The world is the loop's (model 1, solid bsp, movetype push); the module reads it at the start
  // frame, then spawns the map.
  s.loading = true;
  s.sv.g.self = 0;
  s.sv.g.other = 0;
  s.sv.g.time = static_cast<float>(s.sv.time);
  sim::qvm_export(q, s.sv, sim::kQvmStartFrame, static_cast<int32_t>(s.sv.time * 1000), 0);
  sim::qvm_export(q, s.sv, sim::kQvmLoadEntities);
  qvm_run_buffer(s);
  s.loading = false;
  if (s.map_stopped) {
    error = "the module stopped the map: " + s.stop_reason;
    return false;
  }
  fill_qvm_world(s, s.world, true);
  return true;
}

void qvm_connect(QvmServer& s, int slot, const std::string& name, bool spectator, bool bot,
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
  const int e = slot + 1;
  sim::QvmProgram& q = s.q;
  if (q.data.ents == 0) return;
  sim::qvm_clear_entity(q, e);
  sim::set_field_f(q, e, F.colormap, static_cast<float>(e));
  sim::set_field_f(q, e, F.team, 0);
  if (parms != nullptr) {
    for (int k = 0; k < 16; k++) {
      c.spawn_parms[k] = parms[k];
      sim::set_global_f(q, G.parm1 + k, parms[k]);
    }
  }
  q.setting_up = e;
  q.setup_maxspeed = s.sv.set.move.maxspeed;
  sh::connect_client(s.sv, slot, spectator, bot, parms == nullptr);
  q.setting_up = 0;
  fill_qvm_world(s, s.world, false);
}

void qvm_disconnect(QvmServer& s, int slot) {
  if (!s.clients[slot].active) return;
  if (s.q.data.ents != 0) sh::disconnect_client(s.sv, slot);
  s.clients[slot] = ClientInfo{};
  bool players = false;
  for (const ClientInfo& c : s.clients) players = players || (c.active && !c.spectator);
  if (s.paused && !players) qvm_pause(s, -1, false);
  fill_qvm_world(s, s.world, false);
}

void qvm_frame(QvmServer& s, const sh::Packet* packets, int count) {
  if (s.q.data.ents == 0) return;
  if (s.paused) {
    s.paused_seconds += s.sv.set.mintic;
    call_as(s, 0, sim::kQvmPausedTic, static_cast<int32_t>(s.paused_seconds * 1000));
  } else {
    sh::server_frame(s.sv, packets, count);
  }
  s.frames++;
  send_forced_views(s);
  for (int i = 0; i < sh::kMaxClients; i++)
    if (s.clients[i].active && s.clients[i].drop) qvm_disconnect(s, i);
  qvm_run_buffer(s);
  fill_qvm_world(s, s.world, false);
}

CommandResult qvm_client_command(QvmServer& s, int slot, const std::string& line) {
  if (slot < 0 || slot >= sh::kMaxClients || !s.clients[slot].active) return CommandResult::bad;
  set_words(s, line);
  if (s.words.count == 0) return CommandResult::bad;
  const std::string cmd = pg::word(s.words, 0);
  CommandResult r = CommandResult::bad;
  if (listed(kFixed, cmd)) {
    run_server_command(s, slot);
    r = CommandResult::server;
  } else if (s.q.data.ents == 0) {
    r = CommandResult::bad;
  } else if (cmd == "say" || cmd == "say_team") {
    if (call_as(s, slot + 1, sim::kQvmClientSay, cmd == "say_team" ? 1 : 0) != 0) {
      r = CommandResult::mod;
    } else {
      set_words(s, line);
      server_say(s, slot, cmd == "say_team");
      r = CommandResult::server;
    }
  } else if (call_as(s, slot + 1, sim::kQvmClientCommand) != 0) {
    r = CommandResult::mod;
  } else if (listed(kOverridable, cmd)) {
    set_words(s, line);  // the module may have tokenized something else
    run_server_command(s, slot);
    r = CommandResult::server;
  } else {
    print_to(s, 1u << slot, 2, "Bad user command: " + cmd + "\n");
  }
  qvm_run_buffer(s);
  fill_qvm_world(s, s.world, false);
  return r;
}

bool qvm_console(QvmServer& s, const std::string& line) {
  const bool ok = run_console_line(s, line);
  qvm_run_buffer(s);
  fill_qvm_world(s, s.world, false);
  return ok;
}

void qvm_set_user_info(QvmServer& s, int slot, const std::string& key, const std::string& value) {
  if (slot < 0 || slot >= sh::kMaxClients || !s.clients[slot].active) return;
  ClientInfo& c = s.clients[slot];
  if (info_value(c.userinfo, key) == value) return;
  const bool live = s.q.data.ents != 0;
  set_words(s, "setinfo \"" + key + "\" \"" + value + "\"");
  if (live && call_as(s, slot + 1, sim::kQvmUserInfoChanged, 0) != 0) return;  // refused
  std::string old;
  set_info_value(c.userinfo, key, value, old);
  if (key == "cl_speedjump") s.sv.clients[slot].speedjump = value == "1";  // pm_speedjump's ask
  if (live) {
    set_words(s, "setinfo \"" + key + "\" \"" + value + "\"");
    call_as(s, slot + 1, sim::kQvmUserInfoChanged, 1);
  }
  fill_qvm_world(s, s.world, false);
}

bool qvm_pause(QvmServer& s, int slot, bool pause) {
  if (pause == s.paused) return false;
  if (slot >= 0) {
    if (s.clients[slot].spectator) {
      print_to(s, 1u << slot, 2, "Spectators can not pause.\n");
      return false;
    }
    if (pause && !s.cvars.find("pausable")->on()) {
      print_to(s, 1u << slot, 2, "Pause not allowed.\n");
      return false;
    }
  }
  s.paused = pause;
  if (pause) s.paused_seconds = 0;
  NetEvent ev;
  ev.kind = EventKind::pause;
  ev.to = everyone(s);
  ev.reliable = true;
  ev.level = pause ? 1 : 0;
  push(s, std::move(ev));
  return true;
}

void qvm_carry(QvmServer& s) {
  if (s.q.data.ents == 0) return;
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (!s.clients[i].active) continue;
    call_as(s, i + 1, sim::kQvmSetChangeParms);
    for (int k = 0; k < 16; k++) s.clients[i].spawn_parms[k] = sim::global_f(s.q, G.parm1 + k);
  }
}

void fill_qvm_world(QvmServer& s, RulesWorld& w, bool full) {
  sim::QvmProgram& q = s.q;
  if (q.data.ents == 0 || s.sv.edicts.empty()) return;
  if (full) {
    w.map_name = s.map_name;
    w.models = s.models;
    w.sounds = s.sounds;
    w.vwep = s.vwep;
    w.statics.clear();
    for (const StaticEntity& st : s.statics)
      w.statics.push_back({st.modelindex, static_cast<int>(st.frame), static_cast<int>(st.colormap),
                           static_cast<int>(st.skin), st.origin, st.angles,
                           st.extras.has_alpha ? st.extras.alpha : 1.0f});
    w.max_clients = s.sv.set.maxclients;
    w.weapon_frames = true;
    w.map_title = std::string(sim::entity_text(q, 0, F.message));
    w.entities.assign(static_cast<size_t>(s.sv.set.max_edicts), WorldEntity{});
    for (WorldClient& c : w.clients) c = WorldClient{};
  }
  for (int k = 0; k < kWorldLightStyles; k++)
    if (w.lightstyles[k] != s.lightstyles[k]) w.lightstyles[k] = s.lightstyles[k];
  if (w.serverinfo != s.serverinfo) w.serverinfo = s.serverinfo;
  w.time = s.sv.time;
  w.serverflags = sim::global_f(q, G.serverflags);
  w.paused = s.paused;
  for (int i = 0; i < sh::kMaxClients; i++) {
    const ClientInfo& from = s.clients[i];
    WorldClient& c = w.clients[i];
    c.active = from.active;
    c.spectator = from.spectator;
    c.bot = from.bot;
    c.userid = from.userid;
    c.ping_ms = from.ping_ms;
    c.entered_at = from.entered_at;
    if (c.userinfo != from.userinfo) c.userinfo = from.userinfo;
    const std::string name = info_value(from.userinfo, "name");
    if (c.name != name) c.name = name;
    PlayerNumbers& n = c.numbers;
    if (!from.active) {
      n = PlayerNumbers{};
      continue;
    }
    const int e = i + 1;
    n.health = sim::field_f(q, e, F.health);
    n.armour = sim::field_f(q, e, F.armorvalue);
    n.armour_type = sim::field_f(q, e, F.armortype);
    n.ammo[0] = sim::field_f(q, e, F.ammo_shells);
    n.ammo[1] = sim::field_f(q, e, F.ammo_nails);
    n.ammo[2] = sim::field_f(q, e, F.ammo_rockets);
    n.ammo[3] = sim::field_f(q, e, F.ammo_cells);
    n.current_ammo = sim::field_f(q, e, F.currentammo);
    n.weapon = sim::field_f(q, e, F.weapon);
    n.weapon_frame = sim::field_f(q, e, F.weaponframe);
    n.items = sim::field_f(q, e, F.items);
    n.view_z = sim::field_f(q, e, F.view_ofs + 2);
    n.frags = sim::field_f(q, e, F.frags);
    n.weapon_model = model_index_of(s, sim::entity_text(q, e, F.weaponmodel), n.weapon_model);
  }
  const int n = s.sv.num_edicts < q.max_edicts ? s.sv.num_edicts : q.max_edicts;
  if (static_cast<size_t>(n) > w.entities.size()) w.entities.resize(static_cast<size_t>(n));
  w.entity_count = n;
  for (int e = 0; e < n; e++) {
    const sh::Edict& ed = s.sv.edicts[static_cast<size_t>(e)];
    WorldEntity& r = w.entities[static_cast<size_t>(e)];
    r.free = ed.free && e != 0;
    r.origin = ed.origin;
    r.angles = ed.angles;
    r.velocity = ed.velocity;
    r.mins = ed.mins;
    r.maxs = ed.maxs;
    r.absmin = ed.absmin;
    r.absmax = ed.absmax;
    r.view_angles = ed.v_angle;
    r.flags = ed.flags;
    r.waterlevel = ed.waterlevel;
    r.health = ed.health;
    r.modelindex = static_cast<int>(sim::field_f(q, e, F.modelindex));
    r.frame = static_cast<int>(sim::field_f(q, e, F.frame));
    r.colormap = static_cast<int>(sim::field_f(q, e, F.colormap));
    r.skin = static_cast<int>(sim::field_f(q, e, F.skin));
    r.effects = static_cast<int>(sim::field_f(q, e, F.effects));
    const pg::EntityExtras& x = s.extras[static_cast<size_t>(e)];
    r.alpha = x.has_alpha ? x.alpha : 1.0f;
  }
}

namespace {

QvmServer& qs(void* rules) { return *static_cast<QvmServer*>(rules); }

bool r_start(void* r, const MapSpec& map, std::string& error) {
  return qvm_load_map(qs(r), map, error);
}
void r_connect(void* r, int slot, const std::string& name, bool spectator, bool bot,
               const float* parms) {
  qvm_connect(qs(r), slot, name, spectator, bot, parms);
}
void r_drop(void* r, int slot) { qvm_disconnect(qs(r), slot); }
void r_frame(void* r, const sh::Packet* packets, int count) { qvm_frame(qs(r), packets, count); }
CommandResult r_command(void* r, int slot, const std::string& line) {
  return qvm_client_command(qs(r), slot, line);
}
bool r_console(void* r, const std::string& line) { return qvm_console(qs(r), line); }
void r_user_info(void* r, int slot, const std::string& key, const std::string& value) {
  qvm_set_user_info(qs(r), slot, key, value);
}
bool r_pause(void* r, int slot, bool on) { return qvm_pause(qs(r), slot, on); }
void r_carry(void* r) { qvm_carry(qs(r)); }
void r_destroy(void* r) { delete static_cast<QvmServer*>(r); }

constexpr RunnerCalls kQvmCalls = {"qvm",    &r_start,   &r_connect, &r_drop,
                                   &r_frame, &r_command, &r_console, &r_user_info,
                                   &r_pause, &r_carry,   &r_destroy, true};
constexpr RunnerCalls kQvmOverCalls = {"qvm",    &r_start,   &r_connect, &r_drop,
                                       &r_frame, &r_command, &r_console, &r_user_info,
                                       &r_pause, &r_carry,   nullptr,    true};

}  // namespace

RulesRunner qvm_runner(std::unique_ptr<QvmServer> s) {
  QvmServer* p = s.release();
  return {&kQvmCalls, p, p};
}

RulesRunner qvm_runner_over(QvmServer& s) { return {&kQvmOverCalls, &s, &s}; }

QvmServer* qvm_of(const RulesRunner& r) {
  return r.calls == &kQvmCalls || r.calls == &kQvmOverCalls ? static_cast<QvmServer*>(r.rules)
                                                            : nullptr;
}

}  // namespace shooters_edge::server
