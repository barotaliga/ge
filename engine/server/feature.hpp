// THE FEATURE SEAM: what a server grows by (docs/servers.md § Open for extension). A FEATURE is
// compiled in and composed by the server's `main` into one ORDERED LIST (a list, not a registry):
// its cvars and console commands added once (`attach`), and its hooks called in the list's order
// as the server's frame meets each thing that happens:
//
//   admit        a peer asks for a seat from an address (false: refused as banned; every feature
//                is asked, the first false stops it)
//   connect      a player took a seat (or came back to it)
//   disconnect   a player's seat freed (left, kicked, timed out)
//   chat         a player's chat line, before it is relayed; true holds it (the feature answered)
//   info         a player's team, readiness or leadership changed
//   command      a player's console line for the server (`status`, `rcon ...`): true answers it
//                and stops it there; a line no feature answers is told it is not a command
//   frame        each frame, after the network
//   phase        the session's phase changed (lobby, playing)
//   server_info  the public status record is written: a feature adds its keys
//
// A feature reaches the server, its console and the answer through `Features`; `feature_answer`
// sends a player lines (or prints them on the console, for the console's own `kNoPlayer`). The
// core holds a pointer to the list (`Server::features`) and knows no feature by name, so a feature
// added to the list needs no edit in the core (the `server-core` lab adds one of its own).
//
// The hooks run on the core's own session (a game with its own protocol has none: there only
// `frame` and `server_info` are called, and the game's endpoint answers its own clients).
//
// ge:summary The server's feature seam: an ordered list of compiled-in features and their hooks
// ge:keywords server feature features hook hooks admit connect disconnect chat command frame phase
// ge:keywords server-info feature_answer features_attach admin-features extension server-mod
// ge:skill net
// ge:test tests/server_feature_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"
#include "net/address.hpp"
#include "net/session.hpp"
#include "net/status.hpp"

namespace ge::server {

struct Server;
struct Features;

// One feature: its name, its own state and its hooks (each may be null).
struct Feature {
  const char* name = "";
  void* self = nullptr;
  void (*attach)(void* self, Features& f) = nullptr;
  bool (*admit)(void* self, Features& f, const net::Address& from) = nullptr;
  void (*connect)(void* self, Features& f, int player) = nullptr;
  void (*disconnect)(void* self, Features& f, int player, net::SessionWhy why) = nullptr;
  bool (*chat)(void* self, Features& f, int player, net::ChatScope scope,
               std::string_view line) = nullptr;
  void (*info)(void* self, Features& f, int player) = nullptr;
  bool (*command)(void* self, Features& f, int player, const std::vector<std::string>& words,
                  std::string_view line) = nullptr;
  void (*frame)(void* self, Features& f) = nullptr;
  void (*phase)(void* self, Features& f, net::SessionPhase phase) = nullptr;
  void (*server_info)(void* self, const Features& f, net::Status& status) = nullptr;
};

struct Features {
  std::vector<Feature> list;
  Server* server = nullptr;             // set by server_open
  console::Console* console = nullptr;  // set by features_attach
  uint64_t commands = 0, unknown = 0;   // player lines taken, and those no feature answered
  uint64_t admitted = 0, refused = 0;   // joins each feature let through, and refused
  uint64_t held = 0;                    // chat lines a feature held
};

// Each feature's `attach`, in order (once, before the server's cfg files are read, so their cvars
// are set like any other).
void features_attach(Features& f, console::Console& con);
// Lines to one player (split at each newline), or to the server's console for kNoPlayer.
void feature_answer(Features& f, int player, std::string_view text);
// The seat a word names: a seat number, else a player's exact name; kNoPlayer when none.
int feature_player(const Features& f, std::string_view word);

// THE CORE'S SIDE (server.cpp): the frame's session events through the hooks, the frame hooks,
// the public record's keys, and the session's admit and chat filter.
void features_frame(Features& f);
void features_server_info(const Features& f, net::Status& status);
bool features_admit(void* features, const net::Address& from);
bool features_chat(void* features, int from, net::ChatScope scope, int to, const char* text,
                   size_t size);

}  // namespace ge::server
