// THE CLASSIC GAMES' COMPETITION RULESETS (docs/porting/settings.md § Rulesets): the books the
// engine's rulesets (console/ruleset.hpp) choose from, one for QuakeWorld and one for Quake II,
// and how a player tells the others which one they keep.
//
// - QUAKEWORLD'S BOOK: `default` and the five rulesets its leagues ask for by name: `smackdown`
//   (duels and team games), `smackdrive`, `qcon`, `thunderdome` and `mtfl` (team fortress). Each
//   holds the settings and refuses the commands those leagues forbid while a match is played.
// - QUAKE II'S BOOK: `default` and `duel`, the game as its duel players know it: nothing the
//   classic client calls a cheat (lightmaps alone, the world full bright, the triangles shown,
//   the third person) and the frame rate inside the game's own 10 to 125.
// - EVERY RULESET BUT `default` turns off what this engine adds to the classic games: classic
//   enhance (`ce_enabled 0`), the remix (`cr_enabled 0`), the easy jump (`cl_speedjump 0`) and
//   the rocket jump helpers (`+rjump`, `+rrjump`), so a ruleset's game is the original one.
// - TELLING OTHERS: the ruleset rides in the user info (`*rsn` on QuakeWorld, which every server
//   keeps and shows in `user <name>` and its demos; `rsn` on Quake II), and the client answers a
//   chat line `f_ruleset` (said by anyone, or by `f_ruleset check`) with its server, its version
//   and its ruleset, at most once every 20 seconds (`FairReplies`). The answer's ending says
//   which aids are on and off: `+` then the letters on, `-` then the letters off, in the order
//   m (the game's models or sounds changed: classic enhance or the remix on), s (movement
//   scripts), f (one skin forced on every enemy), i (the side-step aid).
//
// ge:summary The classic games' competition rulesets, the ruleset in user info, f_ruleset replies
// ge:keywords ruleset rulesets smackdown smackdrive qcon thunderdome mtfl duel competition
// ge:keywords tournament official rsn f_ruleset fair-play fairness-reply q1_rulesets q2_rulesets
// ge:keywords fair_query ruleset_reply fairreplies ce_enabled cr_enabled cl_speedjump rjump
// ge:skill classic console
// ge:test tests/classic_rulesets_test.cpp
#pragma once

#include <span>
#include <string>
#include <string_view>

#include "console/ruleset.hpp"

namespace ge::classic {

// QuakeWorld's book (Quake played on its servers too), `default` first.
std::span<const console::Ruleset> qw_rulesets();
// Quake II's book, `default` first.
std::span<const console::Ruleset> q2_rulesets();

// The user info key that carries the ruleset: "*rsn" on QuakeWorld, "rsn" on Quake II.
std::string_view ruleset_info_key(bool q2);

// Whether a chat line asks the fairness question `word` ("f_ruleset"): the text after a
// speaker's "name: " is the word alone, or the word and digits (a check's fresh number), then
// only spaces. The charset's coloured letters count as plain.
bool fair_query(std::string_view chat_line, std::string_view word);

// What the client knows to answer with.
struct FairFacts {
  std::string server;   // the server's address as joined ("host:port")
  std::string version;  // the client's name and version
  std::string ruleset;  // the ruleset in force
  bool modified = false, scripts = false, forced_skins = false, side_step = false;
};
// The answer said in chat: "<server> <version> <ruleset>+<on>-<off>".
std::string ruleset_reply(const FairFacts& f);
// The answer to `f_ruleset` typed alone: what it is for and how to read the answers.
std::string fair_help();

// At most one answer of each kind every kFairEveryS seconds; a question inside it is ignored.
inline constexpr double kFairEveryS = 20;
struct FairReplies {
  double ruleset_at_s = -1e9;
  // Whether an answer may be said now (and, if so, that it was).
  bool may_answer(double now_s);
};

}  // namespace ge::classic
