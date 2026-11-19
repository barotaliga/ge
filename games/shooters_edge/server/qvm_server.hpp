// THE COMPILED-MODULE SERVER: a mod shipped as a compiled module (a `.qvm`, plugins/progs/qvm.hpp)
// running a match on the shooter loop, the third program form behind `RulesRunner`
// (rules_runner.hpp, form "qvm"). It keeps what every runner keeps (`RulesCommon`: the loop, the
// settings, the lists, the clients, the message sink, the rules' world) and the module's own: the
// VM and its adapter (sim/qvm_program.hpp), the map's entity text it reads token by token, the
// console's words it reads and writes, its open files, its extension calls. The system calls are
// qvm_calls.cpp's (`qvm_syscall`).
//
// A MAP (`qvm_load_map`), in the module's order: the memory reloaded from the file, the loop made
// over the map's brush models with the model list "", the map, "*1", "*2", ...; `sv_pr2references`
// reset to 0; INIT (the level time in ms, the server's seed), whose result is the game data block
// (refused outside interface 16..17); reference mode on when the module set `sv_pr2references`;
// every slot below the entity limit bound, and in reference mode cleared by the module's clear
// edict; the extension fields found; the world set up (model 1, solid bsp, movetype push); START
// FRAME (time 1.0) and LOAD ENTITIES while loading (precache and the signon allowed: the module
// reads the entity text with system call 3 and spawns everything itself); then the match runs.
//
// CLIENTS: entering clears the entity (and runs clear edict), sets colormap to its number, team 0,
// gravity 1 and the maximum speed; set new parms; connect and put in server (a spectator's with 1).
// Leaving runs disconnect. A CLIENT COMMAND: the server's own fixed ones (commands.hpp's list) run
// at once; `say` and `say_team` go to the say export (non-zero: handled, else the server prints
// the chat); anything else goes to the client command export with the console's words set to the
// line (non-zero: handled), else the server's overridable ones run or "Bad user command". A USER
// INFO change asks the userinfo export before (non-zero refuses it) and tells it after. THE
// CONSOLE: `set`, `serverinfo`, `localinfo`, `map`, `changelevel`, `restart`, `exec <file>`, a
// cvar's name, and `mod <words>` to the console export; anything else is "Unknown command".
//
// Everything is a function of the module's bytes, the map, the settings and what the server is
// told in order; the date the module asks for comes from `clock` (fixed by default, so a run
// repeats), file writes go only to `write_dir` and are never read back into a rule.
//
// ge:summary The compiled-module server: a .qvm mod running a match as the third rules runner
// ge:keywords qvm compiled-module server runner qvm_runner game-data-block load-entities
// ge:keywords entity-token console-words exports connect disconnect frame map-start
// ge:keywords sv_pr2references reference-mode clear-edict mod-console
// ge:skill progs shooter
// ge:test tests/shooters_edge_qvm_server_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "progs/text.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "shooters_edge/server/rules_runner.hpp"
#include "shooters_edge/sim/qvm_program.hpp"

namespace shooters_edge::server {

constexpr int kQvmSyscalls = 270;  // 0..93 and the extension calls bound at 256..268
constexpr int kQvmMaxFiles = 8;
constexpr int kQvmApiVersion = 16;  // the interface this server answers (no client-side exports)

struct QvmServerOptions {
  sh::ServerSettings loop;  // the loop's settings (first_dynamic, max_edicts set by the server)
  uint32_t seed = 1;        // init's seed
  ClockSource clock = &fixed_clock;
  void* clock_ctx = nullptr;
  // The game's files by the search path (fs_open's reads, `exec`, changelevel's map check): the
  // bytes of `name`, false when there is none. Null: no files.
  bool (*read_file)(void* ctx, const std::string& name, std::vector<uint8_t>& bytes) = nullptr;
  void* files_ctx = nullptr;
  // The folders on disk the file list walks, the mod's own first; the folder writes go to ("":
  // writes refused).
  std::vector<std::string> list_dirs;
  std::string write_dir;
  std::string version = "Shooter's Edge 0.1";  // the server's version text (infokey `version`)
};

struct QvmFile {
  bool open = false, write = false;
  std::string path;            // a write's file on disk
  std::vector<uint8_t> bytes;  // a read's content
  size_t pos = 0;
};

struct QvmServer : RulesCommon {
  QvmServerOptions opt;
  std::vector<uint8_t> module;                     // the module's file, reloaded at each map
  std::unique_ptr<ge::brush::BrushTracer> tracer;  // a version 38 map's brushes (map_models)
  sim::QvmProgram q;

  std::string entities;  // the map's entity text, read by entity_token
  size_t entity_at = 0;
  ge::progs::Words words;  // the console's words (argc, argv, args, tokenize)
  std::string command_buffer;
  std::vector<std::string> frag_log;
  QvmFile files[kQvmMaxFiles];
  int32_t bound[256] = {};  // the extension call bound at 256 + i (an id; 0 none)

  // readcmd's capture of the console's output (the last 1340 bytes' stretch).
  bool capturing = false;
  std::string capture;

  int64_t syscall_counts[kQvmSyscalls] = {};
  int64_t frames = 0;
  double paused_seconds = 0;
  double check_time = -1;  // checkclient: when the candidate last moved on
  int check_client = 0;
  int next_userid = 1;
  bool loading = false;
  bool changelevel_queued = false;

  // The module's own bots (add_bot): each one's command stored by set_bot_cmd for the next bot
  // frame, its packets' sequence, and when the next bot frame is due (server time, s).
  sh::MoveCmd bot_cmd[sh::kMaxClients] = {};
  uint32_t bot_sequence[sh::kMaxClients] = {};
  double next_bot_frame = 0;
  int64_t bot_frames = 0;
  int64_t bot_clips = 0;  // bot moves handed back to the module's blocked handler

  QvmServer() = default;
  QvmServer(const QvmServer&) = delete;
  QvmServer& operator=(const QvmServer&) = delete;
  ~QvmServer();  // the module's open files closed (a write's bytes reach the disk)
};

// A server for `module` (a .qvm's bytes): loaded once to check it, the settings registered. False
// with `error` when the file is refused.
bool create_qvm_server(QvmServer& s, std::vector<uint8_t> module, const QvmServerOptions& opt,
                       std::string& error);
std::unique_ptr<QvmServer> make_qvm_server(std::vector<uint8_t> module, const QvmServerOptions& opt,
                                           std::string& error);

bool qvm_load_map(QvmServer& s, const MapSpec& map, std::string& error);
void qvm_connect(QvmServer& s, int slot, const std::string& name, bool spectator, bool bot,
                 const float* parms = nullptr);
void qvm_disconnect(QvmServer& s, int slot);
void qvm_frame(QvmServer& s, const sh::Packet* packets, int count);
CommandResult qvm_client_command(QvmServer& s, int slot, const std::string& line);
// One console line; false when nothing took it.
bool qvm_console(QvmServer& s, const std::string& line);
void qvm_set_user_info(QvmServer& s, int slot, const std::string& key, const std::string& value);
bool qvm_pause(QvmServer& s, int slot, bool pause);
// Set change parms for every client (before a level change), their parms saved.
void qvm_carry(QvmServer& s);

// THE MODULE'S OWN BOTS (system calls 78..81). ADDING one takes the first slot free here and on
// the host's side (`RulesCommon::bot_seat`) while the players are under the limit: the slot's
// entity cleared, the user info `\name\..\topcolor\..\bottomcolor\..\emodel\6967\pmodel\13845
// \skin\..\*bot\1`, gravity 1, the server's maximum speed, the module's `isBot` field 1, then
// connect and put in server (no new parms); the slot, or -1. REMOVING runs disconnect and frees
// the slot. A BOT FRAME runs at the server's `maxfps` (77 outside 20..1000) after the physics,
// at most one a server frame with the time left carried, whether or not a bot plays (the module
// readies its routes in them): start frame with 1 and `frametime` its length, then each bot's
// stored command as one packet (the pre-think, the move, the post-think), the command then cleared.
// A bot's move that hits anything goes to the module's blocked handler first (shooter/client.hpp's
// THE BOT'S CLIPPED MOVE), which may steer it: its stored command then runs once more in place of
// the move.
int qvm_add_bot(QvmServer& s, const std::string& name, int bottom, int top,
                const std::string& skin);
void qvm_remove_bot(QvmServer& s, int slot);
// A bot's user info key set without asking the module; told to the host when every client sees
// it (a short-info key or one starting with `_`).
void qvm_set_bot_info(QvmServer& s, int slot, const std::string& key, const std::string& value);
void qvm_set_bot_cmd(QvmServer& s, int slot, const sh::MoveCmd& cmd);
// The bot frames due by the server's time (qvm_frame runs them).
void qvm_bot_frames(QvmServer& s);

// The server's console line (readcmd captures it while it runs).
void qvm_print(QvmServer& s, const std::string& line);
// The command buffer run now (localcmd's lines, `exec`'s).
void qvm_run_buffer(QvmServer& s);
// The rules' world from the server: the lists at the map's start (`full`), the rest every call.
void fill_qvm_world(QvmServer& s, RulesWorld& w, bool full);

// The system calls (qvm_calls.cpp): the host function the VM calls.
int32_t qvm_syscall(void* user, ge::progs::Qvm& vm, const int32_t* args);
// Closes every file the module has open (a write's bytes written out).
void qvm_close_files(QvmServer& s);
// A system call's name ("traceline"); "" for an unknown number.
const char* qvm_syscall_name(int number);

// The runner over a server it owns; over one the caller keeps alive; the server behind a runner
// (null for another form).
RulesRunner qvm_runner(std::unique_ptr<QvmServer> s);
RulesRunner qvm_runner_over(QvmServer& s);
QvmServer* qvm_of(const RulesRunner& r);

}  // namespace shooters_edge::server
