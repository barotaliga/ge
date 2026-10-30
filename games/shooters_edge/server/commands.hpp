// WHAT CLIENTS AND THE CONSOLE TELL THE MOD SERVER, and the hooks that answer: client commands,
// chat, info changes, console lines and pausing.
//
// A CLIENT COMMAND (`client_command`): the line's words (the words of tokenize, plugins/progs/
// text.hpp) are put in the program's word list (so argc and argv read them) and the first is
// matched against the server's own commands. One the server marks as not overridable (new,
// modellist, soundlist, prespawn, spawn, begin, drop, pings, setinfo, serverinfo, download,
// nextdl, ptrack, snap, stopdownload, upload) runs at once. Otherwise the mod gets it first:
// GE_ClientCommand (p0 the word, p1 the arguments line) when the progs has it, else UserCmd (p0
// the word), self the client; a non-zero result means handled. Else the server's own overridable
// command runs (rate, kill, pause, say, say_team, minping, airstep, maps, ban, banip, banrem, join,
// observe, qtvusers, god, give, noclip, fly), or "Bad user command: <word>" is printed to the
// client. `say` and `say_team` offer ChatMessage first (p0 the text with its surrounding quotes
// removed, p1 1 for team): non-zero and the mod handled it, else everyone gets "<name>: <text>"
// at the chat level. `kill` runs ClientKill for a player.
//
// INFO: `set_user_info` changes one key of a client's user info and, when the value moved, calls
// UserInfo_Changed (self the client; p0 key, p1 old value, p2 new value; a new `name` is the
// entity's netname before the hook runs); `set_local_info` the
// same for the server's local info with localinfoChanged (self the world).
//
// THE CONSOLE (`server_command`): `set <name> <value>` (made when unknown), `serverinfo` and
// `localinfo <key> <value>`, `map <name>` (the change the game makes: `pending_map`), a cvar's
// name (with a value: set), and anything else to ConsoleCmd (self the world, the console's words
// in the program's word list): zero, and "Unknown command" is printed.
//
// PAUSING (`request_pause`): a client asks (a spectator cannot); GE_ShouldPause (self the client,
// p0 the new state) may refuse with 0; the pause changes and every client is told. The server
// asks the same with self the world before it unpauses because no players are left.
//
// ge:summary Client commands, chat, info changes, the console and pausing, to the mod's hooks
// ge:keywords client-command usercmd ge_clientcommand chatmessage say say_team kill userinfo
// ge:keywords userinfo_changed localinfochanged consolecmd console pause ge_shouldpause
// ge:keywords overridable bad-user-command client_command server_command request_pause call_hook
// ge:skill progs shooter
// ge:test tests/shooters_edge_qc_hooks_test.cpp
#pragma once

#include <initializer_list>
#include <string>

#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::server {

// One argument a hook is handed: a number, or text (in the ring of 8 the engine hands hooks).
struct HookArg {
  bool is_text = false;
  float number = 0;
  std::string text;
  HookArg(float f) : number(f) {}  // NOLINT: an argument list reads best
  HookArg(std::string t) : is_text(true), text(std::move(t)) {}  // NOLINT
  HookArg(const char* t) : is_text(true), text(t) {}             // NOLINT
};

// Runs hook function `fn` with self `self`, other the world, time the server's and the arguments;
// returns its float result (0 after a fault, and `ok` false).
float call_hook(QcServer& s, HookId h, int32_t fn, int self, std::initializer_list<HookArg> args,
                bool* ok = nullptr);

enum class CommandResult : uint8_t { mod, server, bad };
CommandResult client_command(QcServer& s, int slot, const std::string& line);

void set_user_info(QcServer& s, int slot, const std::string& key, const std::string& value);
void set_local_info(QcServer& s, const std::string& key, const std::string& value);

// One console line; false when nothing took it ("Unknown command").
bool server_command(QcServer& s, const std::string& line);

// True when the pause changed.
bool request_pause(QcServer& s, int slot, bool pause);

}  // namespace shooters_edge::server
