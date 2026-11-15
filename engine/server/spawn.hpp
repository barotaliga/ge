// LAUNCHING A DEDICATED SERVER from a game (docs/servers.md § The shape: both ways can start the
// other's work): the game's HOST page, its player CLI (`<game>_cli host --dedicated`) and `ge
// server` start the game's dedicated binary as its own process, hand it a cfg, find it again
// later (after the game or the machine's session restarted) and stop it. The same for every game
// with a server; desktop only (module.cmake leaves it out of a phone's or a browser's build).
//
// THE RECORD: each server started is kept as `hosted-<port>.txt` in a STATE folder the caller
// names (the game's own state folder), so any later run of any of the game's programs finds it:
// its process, port, kind, log, start time and the command that started it (a password shown as
// ***). A port holds one record; one whose process is gone is said so, and `forget` drops it.
//
// THE CFG: `SpawnAsk::cfg` (cfg lines, the game's own settings) is written beside the record as
// `server-<port>.cfg` and run on the server's command line (`+exec <path>`), so `--explain` names
// that file and line for every value it set, over the server.cfg layers.
//
// ITS CONSOLE: the server is started with `--commands=<state>/console-<port>.txt` and runs each line
// added there (main.hpp), so a later program sends it console lines by the record alone:
// `hosted_command(state, port, "mod arena")` switches its mods and restarts its match.
//
// FINDING IT AGAIN: a record names a port on this machine; `server_answers` pings the session
// there (the LAN advert, net/discovery.hpp) and reads back its name, seats and phase. A record
// whose process lives but does not answer is a server still starting, or one wedged.
//
//   spawn_server(ask, out, error)       the binary started detached, the cfg handed, recorded
//   hosted_list(state)                  every record, by port
//   hosted_alive(h)                     its process still runs
//   hosted_command(state, port, line)   a console line sent to the server started on the port
//   server_answers(port, protocol, ...) the server on this machine's port answers a ping
//   stop_hosted(state, h, error)        stopped (asked, then made to) and its record dropped
//
// ge:summary Launching a dedicated server: spawn with a cfg, the record, find by ping, stop
// ge:keywords spawn launch dedicated-server host-page hosted record state-folder find-again stop
// ge:keywords spawn_server hosted_list hosted_read hosted_write hosted_forget server_answers
// ge:keywords stop_hosted exec cfg-handed ping advert local-address hosted_command mod-switch
// ge:skill net
// ge:test tests/server_spawn_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "net/discovery.hpp"

namespace ge::server {

// A server this machine started, as its record keeps it.
struct Hosted {
  long pid = 0;
  int port = 0;
  std::string kind;  // "dedicated", or "listen" for a game's window hosting one
  std::string log;
  long long started = 0;  // seconds since the epoch
  std::string command;    // as started, a password shown as ***
};

// The record's path for `port` in the folder `state`.
std::string hosted_path(const std::string& state, int port);
bool hosted_read(const std::string& path, Hosted& out);
// Written into `state` (made when missing); false when it cannot be.
bool hosted_write(const std::string& state, const Hosted& h);
// Every record in `state`, by port.
std::vector<Hosted> hosted_list(const std::string& state);
// The record for `port` dropped (with the cfg handed to it and its console file).
void hosted_forget(const std::string& state, int port);
bool hosted_alive(const Hosted& h);

// What to start.
struct SpawnAsk {
  std::string program;  // the dedicated binary's path (process.hpp's program_path)
  std::string root;     // its --root, and the folder it runs in ("": the caller's)
  std::string state;    // where the record and the cfg handed to it go
  std::string logs;     // where its log goes ("": `state`)
  int port = 0;
  std::string map, name, password;
  int maxclients = 0;              // 0: the server's own
  int bots = -1;                   // -1: the server's own
  std::vector<std::string> mods;   // --mod, lowest first
  std::string cfg;                 // cfg lines handed to it (`server-<port>.cfg`), "" none
  double seconds = 0;              // --for (0: until stopped)
  std::vector<std::string> extra;  // more flags or +commands, as given
};

// The arguments `ask` starts the binary with (argv[0] the program); the password as given.
std::vector<std::string> spawn_argv(const SpawnAsk& ask);
// The command line as one string to show, quoted where a word needs it, the password as ***.
std::string shown_command(const std::vector<std::string>& argv, const std::string& password);

// `ask` started detached and recorded into `out`. Refused, with the reason and the next step in
// `error`, when the port is outside 1..65535, the program is not there, or a server this machine
// started still runs on the port.
bool spawn_server(const SpawnAsk& ask, Hosted& out, std::string& error);

// `line` sent to the console of the server this machine started on `port` (its record in
// `state`), run within a quarter of a second. False with `error` when no such server runs.
bool hosted_command(const std::string& state, int port, const std::string& line,
                    std::string& error);

// Whether a server on this machine's `port` answers a ping of `protocol` within `wait_ms` (its
// advert into `advert` when it does).
bool server_answers(int port, uint32_t protocol, int wait_ms, net::Advert* advert = nullptr);

// `h` stopped (asked to end, then made to after `grace_ms`) and its record dropped.
bool stop_hosted(const std::string& state, const Hosted& h, std::string& error,
                 int grace_ms = 3000);

}  // namespace ge::server
