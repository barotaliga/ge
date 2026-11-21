// THE OPERATOR'S WORDS any game's server can mount on its console, in the words a modern classic
// server answers with (docs/servers.md § The operator's words):
//
// - PICKING A PLAYER (`pick_player`): a word of digits is a slot ("Bad client slot number: <n>"
//   past the slots, "Client slot <n> is not active." on a free one); else the first exact,
//   case-sensitive name. With `partial` (a server's `sv_enhanced_setplayer`, or always for a
//   player's `lag`), a name equal but for case is taken at once, else a unique part of a name in
//   any case ("No clients matching '<id>' found.", "'<id>' matches multiple clients."); without it,
//   "Userid '<id>' is not on the server.". An empty word picks nobody and says nothing.
// - THE RAW REST OF A LINE (`line_rest`): the line from its `n`th word to the end of its last, its
//   inner spacing kept, then one pair of double quotes taken off when it starts and ends with one.
//   What `stuff`, `stuffall`, `printall` and the stuff lists send.
// - THE STUFF LISTS (`StuffLists`, `stuff_list_command`): two lists of lines, `connect` (run by a
//   client at the first `new` of its connection) and `begin` (at each entering of the game). The
//   words `addstuffcmd <list> <command>`, `delstuffcmd <list> <id|cmd|all>` (ids from 1) and
//   `liststuffcmds <list>`, each list named exactly; a line already there, an index or a text not
//   there, an empty list and a list's unknown name are answered in the server's words.
//
// ge:summary The operator's words for any server: picking a player, a line's raw rest, stuff lists
// ge:keywords operator pick-player sv_enhanced_setplayer partial-name stuff stuffall printall
// ge:keywords addstuffcmd delstuffcmd liststuffcmds stuff-lists connect-list begin-list line_rest
// ge:keywords pick_player stuff_list_command
// ge:skill net
// ge:test tests/server_operator_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace ge::server {

// One slot as `pick_player` sees it: its name, and whether a client holds it (a free slot and a
// dropped one waiting to be freed do not).
struct PickSlot {
  std::string name;
  bool held = false;
};

// The slot `word` names among `slots` (above), or -1 with `error` the answer ("" when the word is
// empty). Each error ends in a newline.
int pick_player(const std::vector<PickSlot>& slots, std::string_view word, bool partial,
                std::string& error);

// The raw rest of `line` from its word `n` (0 the command), above; "" when it has fewer words.
std::string line_rest(std::string_view line, size_t n);

struct StuffLists {
  std::vector<std::string> connect, begin;
};

// `line` when it is one of the stuff lists' words: true, with the answer in `out` (each line ending
// in a newline; "" for a quiet success).
bool stuff_list_command(StuffLists& lists, std::string_view line, std::string& out);

}  // namespace ge::server
