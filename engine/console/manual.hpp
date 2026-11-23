// THE REFERENCE MANUAL'S SOURCE: every setting, command and command-line flag a program registers,
// read off its live console (never a hand-kept list), so the player's manual (`ge manual`,
// manual_page.hpp) is generated from what the program really answers to and never goes stale.
//
// A program describes itself in PASSES: one per game it can load (Shooter's Edge's window loads
// each game's own layer in turn: the game's own settings, its spellings of ours and its defaults),
// or one pass for a program with one game. A pass is `describe`d into ENTRIES and written as text
// (`manual_text`), and `ge manual` merges every program's passes into one page.
//
// THE FORMAT is text a diff can read: header lines, then one entry a line, fields split by a tab:
//
//   # manual 1
//   # program <id> <name a player reads>
//   # game <id> <name a player reads> [| <id this game's filter shows too>...]
//   <kind> <TAB> <program> <TAB> <games> <TAB> <name> <TAB> <layer> <TAB> <type> <TAB>
//   <default> <TAB> <min> <TAB> <max> <TAB> <flags> <TAB> <target> <TAB> <overlap> <TAB> <help>
//   <TAB> <values>
//
// `kind` is setting, command, flag, setting-synonym or command-synonym; `games` the pass's game
// ids, split by commas ("" a program with no games); `layer` the console layer that registered it
// (cvar.hpp § LAYERS: "engine", "shooter", "classic", "shell", "game:q2"); `type`, `min`, `max`
// and `flags` a setting's as the catalog writes them (catalog.hpp); `target` a synonym's name of
// ours, or a flag's argument ("<demo>"); `overlap` how a game's layer met the name (`same`: the
// game's default and words for one setting; `own`: the game's own meaning) and `home` the layer
// under it, written as "same:engine". A tab or a line break in a field is written as a space.
// Created cvars (made by `set` at run time) are left out.
//
// ge:summary The reference manual's source: a console's settings, commands and flags as entries
// ge:keywords manual reference-manual user-manual describe manual_text read_manual writemanual
// ge:keywords flag-doc command-line-flags document_flags pass per-game layer synonym spelling
// ge:skill console agent-interface
// ge:test tests/console_manual_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace ge::console {

class Console;
class Cvars;

// One command-line flag a program takes, documented: the flag as typed ("-classic",
// "--size=WxH"), its argument ("<id>", "" a switch) and what it does in a player's words.
struct FlagDoc {
  std::string name;
  std::string arg;
  std::string help;
};

enum class ManualKind : uint8_t { setting, command, flag, setting_synonym, command_synonym };

// One line of the manual's source (the format above).
struct ManualEntry {
  ManualKind kind = ManualKind::setting;
  std::string program;
  std::string games;  // the pass's game ids, split by commas; "" none
  std::string name;
  std::string layer;
  std::string type;  // a setting's kind: text, number, integer or boolean
  std::string fallback, min, max, flags;
  std::string target;   // a synonym's name of ours; a flag's argument
  std::string overlap;  // "same:<layer under>" or "own:<layer under>" for a game's; else ""
  std::string help, values;
};

// A program or a game a source names, in a player's words.
struct ManualName {
  std::string id, name;
  std::vector<std::string> takes_in;  // a game's: the other ids its filter shows too
};

// What a program tells its console about itself for the manual (`Console::set_manual`): its id and
// name, the games it plays (with the pass it is in now: ids split by commas) and its flags.
// `more`: the pass's settings that live OUTSIDE the console (a game's rules, made with each map
// and set through the console's unknown-word hook), appended to `out` for the pass of `games` (the
// ids split by commas); `describe` fills their program and games and drops a name the console has.
struct ManualInfo {
  ManualName program;
  std::vector<ManualName> games;
  std::string pass;
  std::vector<FlagDoc> flags;
  std::function<void(std::string_view games, std::vector<ManualEntry>& out)> more = nullptr;
};

// One source: what a program wrote (its passes' entries, its name and its games').
struct ManualSource {
  std::vector<ManualName> programs, games;
  std::vector<ManualEntry> entries;
};

const char* kind_word(ManualKind kind);
bool kind_named(std::string_view word, ManualKind& out);

// Every live setting, synonym, command, command synonym and documented flag of `console`, for
// `program` in the pass of `games` (the ids split by commas, "" none).
std::vector<ManualEntry> describe(const Console& console, std::string_view program,
                                  std::string_view games);
// The settings of `cvars` alone as entries (describe's settings part), each in `layer` ("" its
// own); program and games left empty.
std::vector<ManualEntry> describe_settings(const Cvars& cvars, std::string_view layer = "");
// The source as text (the format above), entries in the order given.
std::string manual_text(const ManualSource& source);
// `text` read and ADDED to `out` (several programs' files read into one); false with `error` (the
// line, what is wrong and the next action) on a line that is none of the format's.
bool read_manual(std::string_view text, ManualSource& out, std::string& error);
// The console's one pass as a source (its ManualInfo's program, games and pass), the `writemanual
// <file>` builtin's.
ManualSource manual_of(const Console& console);
// `source` written to `path` (its folder made); false with `error` when it cannot be.
bool write_manual(const std::string& path, const ManualSource& source, std::string& error);

}  // namespace ge::console
