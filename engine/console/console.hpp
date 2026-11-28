// THE CONSOLE: the one place a game's settings, commands, aliases and binds meet the text a
// player, a server's owner or an agent types or keeps in a cfg file (docs/console.md). A game
// owns one `Console` (a dedicated server one of its own); everything it exposes is registered on
// it at startup, and the language is the engine's, so every game's cfg reads the same.
//
// THE COMMAND BUFFER. Text is queued (`append` at the end, `insert` at the front: `exec` and an
// alias insert, so their commands run before what followed them) and `run` executes it, once a
// frame, until it is empty or a `wait` ends the frame's share; the rest runs on the next `run`.
// A command is looked up as a COMMAND, then an ALIAS, then a CVAR (a cvar's name alone prints
// it, with a value sets it). An alias or a trigger calling itself is stopped at kMaxDepth nested
// calls within one `run`, with a problem naming the alias; a loop that waits is a loop on
// purpose and runs on. Nothing here reads a clock: a frame is one `run`. A host that has a clock
// (a game's window) hands it in with `set_clock` (clock_macros.hpp's `add_clock_macros` does), and
// a recipe's `sleep <seconds>` (add_recipe_commands, only in a window started to run recipes)
// then holds the rest of the buffer until that time has passed; with no clock, it is a `wait`.
// That is what a RECIPE runs on: a cfg an agent writes to drive a window step by step (docs/
// console.md § Recipes).
//
// SOURCES. Every queued command carries who sent it (cvar.hpp's `Source`): a server's stuffed
// text may run only commands flagged kStuffable and set only the server's cvars.
//
// PROBLEMS. Every refusal is printed and kept as a `Problem` with the file and line the command
// came from and the next action (an unknown name gets the nearest known one), so `ge cfg check`
// can list a cfg's mistakes like a compiler's.
//
// FILES. `exec name` reads `name` (".cfg" added when it has no extension) from the first of the
// search directories that holds it; `cfg_save` writes to the first. A host may replace both with
// its own reader and writer (an archive, a test's strings).
//
// The built-in commands are console_builtins.cpp's (variables, aliases, exec, cfg_save),
// script.cpp's (if, set_calc, inc, dec, toggle, cycle, triggers) and console_binds.cpp's (bind,
// unbind, bindlist): docs/console.md lists each with its syntax.
//
// ge:summary The console: the command buffer, commands, aliases, macros, exec, keys, problems
// ge:keywords console command commands buffer append insert run wait alias aliases macro exec
// ge:keywords cfg config autoexec problem problems stuff stuffcmd kstuffable source depth
// ge:keywords set_fallback_reader fallback-reader set_unknown unknown-command host-commands
// ge:keywords complete completion key press release button command-line plus-set
// ge:skill console
// ge:test tests/console_console_test.cpp tests/console_script_test.cpp
#pragma once

#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "console/binds.hpp"
#include "console/cvar.hpp"
#include "console/manual.hpp"

namespace ge::console {

// A message trigger's levels: the game's print levels, and the team's own chat.
inline constexpr int kTriggerLow = 0, kTriggerMedium = 1, kTriggerHigh = 2, kTriggerChat = 3,
                     kTriggerTeam = 4;
inline constexpr int kMaxDepth = 64;       // nested alias, exec and trigger calls in one run
inline constexpr int kMaxPerRun = 100000;  // commands one run executes before it stops
inline constexpr int kMaxFrames = 1000;    // runs `execute` and startup take before they return

class Console;

// Where a command came from: a file and its line, or "" and 0 for typed or code-sent text.
struct Origin {
  std::string file;
  int line = 0;
};

struct Args {
  std::vector<std::string> words;  // words[0] the command's name
  std::string raw;                 // the whole command as run (after expansion)
  Source source = Source::local;
  Origin where;
  size_t size() const { return words.size(); }
  // Word i, or "" past the end.
  const std::string& operator[](size_t i) const;
  // The text after word `i` as written (quotes kept).
  std::string rest(size_t i) const;
};

using CommandFn = std::function<void(Console&, const Args&)>;

enum CommandFlag : uint8_t {
  kNoExpand = 1u << 0,   // its words are not $-expanded before it runs (alias, bind, if)
  kStuffable = 1u << 1,  // a server may stuff it into a client
};

struct Problem {
  enum class Severity : uint8_t { error, warning };
  Severity severity = Severity::error;
  Origin where;
  std::string what;  // the line printed
  std::string next;  // the next action
};

// A `+name` / `-name` pair a held key drives: down while any key holding it is down.
struct Button {
  int held = 0;          // keys holding it
  bool pressed = false;  // went down since the game last cleared it
  bool down() const { return held > 0; }
};

// Which part of the config `save_text` writes (cfg.hpp § SAVING ONE PART): a series' shared config
// and each game's own file are two parts of one console's settings.
struct SaveScope {
  bool binds = true;  // `unbindall`, the binds and the aliases
  // With `binds`: only the player's changes to the binds (binds.hpp § THE DEFAULTS), each key
  // bound or unbound, and no `unbindall`, so the file lays them over a game's own defaults.
  bool bind_changes = false;
  // The layers whose settings are written; empty: every layer.
  std::function<bool(std::string_view layer)> layers = {};
  // The game scopes whose own values are written (`<scope>:<name>`, cvar.hpp § SCOPES); empty:
  // every scope.
  std::function<bool(std::string_view scope)> scopes = {};
};

// A COMMAND GATE (console/ruleset.hpp): a command refused while `when` holds (always when
// empty), `why` printed in its place; `owner` names who set it, for `ungate`. What a server
// stuffs is never gated: a gate is about what the player does.
struct CommandGate {
  std::string owner;
  std::string why;
  std::function<bool()> when = {};
};

// A `$` macro's kind: a TEAM macro tells what a player holds or where they are (health, armour,
// weapons, powerups, places), the words a strict ruleset keeps scripts from acting on.
enum class MacroKind : uint8_t { plain, team };

// A TEAM MACRO LIMIT (console/ruleset.hpp): while `when` holds (always when empty), a command line
// that expanded a team macro may run only the `allowed` commands; any other command or setting
// prints its name followed by `why`. `owner` names who set it, for `ungate`.
struct TeamMacroLimit {
  std::string owner;
  std::string why;
  std::vector<std::string> allowed;
  std::function<bool()> when = {};
};

// QUIET TRIGGERS (console/ruleset.hpp): while `when` holds, no game line fires a trigger
// (`msg_trigger`, `re_trigger`) and a game's event triggers ask `triggers_quiet` before they run;
// `sounds`: a game's sound triggers (a team line's sound word) keep quiet at all times.
struct TriggerQuiet {
  std::string owner;
  std::function<bool()> when = {};
  bool sounds = false;
};

class Console {
 public:
  // `is_server`: the console of a dedicated server or a listen host (cvar.hpp's tiers).
  explicit Console(bool is_server = true);
  ~Console();
  Console(const Console&) = delete;
  Console& operator=(const Console&) = delete;

  Cvars& cvars() { return cvars_; }
  const Cvars& cvars() const { return cvars_; }
  Binds& binds() { return binds_; }
  const Binds& binds() const { return binds_; }

  // A command; a second registration of a name replaces the first.
  void add_command(std::string_view name, std::string_view help, CommandFn fn, uint32_t flags = 0);
  bool has_command(std::string_view name) const;
  // COMMAND GATES (above): `name` refused while the gate holds; a second gate replaces the first.
  void gate_command(std::string_view name, CommandGate gate);
  // Every gate `owner` set taken away; returns how many.
  int ungate(std::string_view owner);
  // The gate refusing `name` now, or null.
  const CommandGate* gate_of(std::string_view name) const;
  // The command taken away (a game's own that only some content has); false when there was none.
  bool remove_command(std::string_view name);
  // `name` runs our command `target`: a game's own spelling of it, in the cvars' current layer
  // (cvar.hpp § LAYERS), so it goes with that layer (`drop_layer`).
  void add_command_synonym(std::string_view name, std::string_view target);
  // ONE NAME, ONE MEANING for a command: while the cvars' current layer is loaded, `name` runs our
  // command `target` even where `name` is a command of ours too (the game's own meaning of a
  // shared word, ours set aside: Quake's `toggle a b c` flips each name, ours cycles `a`). It goes
  // with its layer (`drop_layer`), and ours is back.
  void add_command_own(std::string_view name, std::string_view target);
  // THE MANUAL (manual.hpp): what the program says about itself (its name, games and flags) for
  // `writemanual` and the generated reference manual.
  void set_manual(ManualInfo info) { manual_ = std::move(info); }
  ManualInfo& manual() { return manual_; }
  const ManualInfo& manual() const { return manual_; }
  // Every command synonym as (its name, the command it runs), sorted by name.
  std::vector<std::pair<std::string, std::string>> command_synonyms() const;
  // A layer taken away: its cvars and setting synonyms (`Cvars::drop_layer`) and its command
  // synonyms. Returns how many moved.
  int drop_layer(std::string_view layer);
  // `$name` expands to `fn()` at run time (a team message's $health), ahead of a cvar's value.
  void add_macro(std::string_view name, std::function<std::string()> fn,
                 MacroKind kind = MacroKind::plain);
  // Whether `$name` is a team macro (`MacroKind::team`).
  bool is_team_macro(std::string_view name) const;
  // TEAM MACRO LIMIT (above): a second replaces the first; `ungate(owner)` takes it away.
  void limit_team_macros(TeamMacroLimit limit);
  // The limit in force now, or null.
  const TeamMacroLimit* team_macro_limit() const;
  // QUIET TRIGGERS (above): a second replaces the first; `ungate(owner)` takes it away.
  void quiet_triggers(TriggerQuiet quiet);
  // Whether triggers keep quiet now; whether sound triggers do.
  bool triggers_quiet() const;
  bool sound_triggers_quiet() const { return trigger_quiet_ && trigger_quiet_->sounds; }
  // At most `n` frames in a row may end on a `wait` with commands still waiting (0: no limit);
  // past it the buffer is emptied and `why` printed. `ungate(owner)` takes it away.
  void limit_waits(std::string owner, int n, std::string why);
  // Every `$` macro's name, sorted.
  std::vector<std::string> macro_names() const;
  // Fixed widths for `$` words (a team line's columns): with `length_prefix` set ("tp_length_"), a
  // `$word` whose cvar `<length_prefix><word>` the player made is padded or cut to that many
  // letters (1..40; `fit_width`), set left, or right or centred by `<align_prefix><word>`
  // starting with r or c. Empty prefixes (the default) leave every value as it is.
  void set_macro_widths(std::string length_prefix, std::string align_prefix) {
    width_prefix_ = std::move(length_prefix);
    align_prefix_ = std::move(align_prefix);
  }
  // A settings block bound cvars write into, kept alive as long as the console (a game's
  // registration hook that has nowhere else to keep its struct).
  void keep(std::shared_ptr<void> block) { kept_.push_back(std::move(block)); }
  // `+name` and `-name`; the button stays at its address.
  Button& add_button(std::string_view name, std::string_view help);

  // THE BUFFER.
  void append(std::string_view text, Source source = Source::local, const Origin& where = {});
  void insert(std::string_view text, Source source = Source::local, const Origin& where = {});
  // Execute until the buffer is empty or a `wait`; returns the commands executed.
  int run();
  // `text` appended and run to the end, `wait`s included (a tool's or a test's way in). While a
  // `sleep` holds the buffer, `text` runs at once and the held commands wait on behind it.
  int execute(std::string_view text, Source source = Source::local);
  bool pending() const { return !buffer_.empty(); }

  // `exec`: the file's commands inserted at the front. False, with a problem, when not found.
  bool exec_file(std::string_view name, Source source = Source::local, const Origin& from = {});
  // The command line's commands, appended in order: a `+` starts each (`+exec my.cfg +set r_x 1`),
  // and the first needs none (`coopgame 2 expert +map e1m2` is two). Returns how many it appended.
  int append_command_line(const std::vector<std::string>& args);

  // FILES: the search directories, the first also where cfg_save writes.
  void set_search_path(std::vector<std::filesystem::path> dirs);
  const std::vector<std::filesystem::path>& search_path() const { return search_; }
  // A host's own reader and writer in place of the directories.
  using Reader = std::function<bool(std::string_view name, std::string& text)>;
  using Writer = std::function<bool(std::string_view name, std::string_view text)>;
  void set_files(Reader read, Writer write);
  // A host's reader tried after the directories find nothing (a game's packs, say); the
  // directories still come first and writes still go to the first of them.
  void set_fallback_reader(Reader read);
  // A SESSION'S OWN CONFIG (a game's `-config <file>`): `config.cfg` is read from and written to
  // this file in place of the search directories' (startup, `cfg_save`, `cfg_load`, a game's
  // save of its settings), and the startup skips `autoexec.cfg`, so a session (a measurement, a
  // test) neither reads nor touches the player's own. A file not there yet starts from the
  // defaults and is made by the first save. Empty: the player's, as always.
  void set_config_file(std::filesystem::path file) { config_ = std::move(file); }
  const std::filesystem::path& config_file() const { return config_; }
  // A host's own command set past the console's: a line whose first word is no command, cvar or
  // alias goes to `run` (the whole line, as typed) and is not refused when it returns true (a
  // window driving bots takes `skill expert` and `bot1 skill expert`). Null: refused as unknown.
  void set_unknown(std::function<bool(const Args&)> run) { unknown_ = std::move(run); }
  const std::function<bool(const Args&)>& unknown() const { return unknown_; }
  bool read_file(std::string_view name, std::string& text) const;
  bool write_file(std::string_view name, std::string_view text) const;

  // OUTPUT: every printed line goes to the sink (default: kept in `output()`).
  void set_sink(std::function<void(std::string_view)> sink);
  const std::function<void(std::string_view)>& sink() const { return sink_; }
  // A TAP sees every printed line too, before the sink (a log of a stretch of play); one at a
  // time, cleared by an empty function.
  void set_tap(std::function<void(std::string_view)> tap) { tap_ = std::move(tap); }
  void print(std::string_view line);
  // A line from the game (a chat line, the server's print): printed, then every message trigger
  // it matches queues its alias.
  void message(std::string_view line);
  // Every message trigger `line` matches queues its alias (what `message` does after printing),
  // whatever its level.
  void fire_triggers(std::string_view line);
  // The same for a line of the game's print `level` (kTriggerLow .. kTriggerChat; `team`: a chat
  // line from the player's own team): a trigger fires on lines of its own level, a `t` trigger
  // on the team's chat alone. A firing sets `$triggermatch` to the line (cut at 255 bytes).
  void fire_triggers(std::string_view line, int level, bool team);
  const std::string& trigger_match() const { return trigger_match_; }
  void clear_trigger_match() { trigger_match_.clear(); }
  const std::vector<std::string>& output() const { return output_; }
  void clear_output() { output_.clear(); }

  // PROBLEMS since the last clear.
  const std::vector<Problem>& problems() const { return problems_; }
  void clear_problems() { problems_.clear(); }
  // Printed as "file:line: what (next)" and kept; `where` is the running command's when empty.
  void error(std::string_view what, std::string_view next,
             Problem::Severity severity = Problem::Severity::error);
  // Kept as a problem like `error`, but never printed: logged on the `console` channel at debug.
  // For what the player can do nothing about (a server's stuffed line the game refuses).
  void quiet_error(std::string_view what, std::string_view next);

  // KEYS: `key` (a name, normalize_key's form or any spelling of it) going down or up in `mode`.
  // A `+` bind runs on both edges. Unknown keys do nothing.
  void key(std::string_view key, bool down, BindMode mode = BindMode::game);

  // ALIASES.
  struct Alias {
    std::string body;
    bool temp = false;  // tempalias: not written by cfg_save
  };
  const std::map<std::string, Alias>& aliases() const { return aliases_; }
  void set_alias(std::string_view name, std::string_view body, bool temp);
  bool remove_alias(std::string_view name);
  void clear_aliases() { aliases_.clear(); }

  // `wait`: the running `run` stops after this command.
  void wait() { waiting_ = true; }
  // The host's clock in seconds (a window's wall time). Until one is set, `sleep` waits one frame.
  void set_clock(std::function<double()> now_s) { clock_ = std::move(now_s); }
  // `sleep <seconds>`: the running `run` stops after this command, and the runs after it do
  // nothing until the host's time is `seconds` later.
  void sleep(double seconds);
  // Whether a `sleep` holds the buffer now.
  bool sleeping() const { return clock_ && clock_() < wake_s_; }
  // `work` (a host's own commands: a config it runs at startup) run with the held commands set
  // aside, then put back behind whatever `work` left; the sleep holds on as it was.
  void past_sleep(const std::function<void()>& work);

  // COMPLETION: every command, alias and cvar starting with `prefix`, sorted.
  std::vector<std::string> complete(std::string_view prefix) const;
  // The nearest name to `name` (edit distance at most 2), or "".
  std::string suggest(std::string_view name) const;
  // A word's argument values (Quake II's `use`, `drop` and `give` complete the item names the
  // server listed), asked when a line `<word> <part>` is completed; the word need not be a
  // command here (a server's own words are forwarded by `set_unknown`). Null removes it.
  using ArgValues = std::function<std::vector<std::string>()>;
  void set_arg_values(std::string_view word, ArgValues values);
  // The values `word`'s argument completes to, in their own order; empty when none. A command
  // synonym with no values of its own completes as the command it runs.
  std::vector<std::string> arg_values(std::string_view word) const;

  // The whole player config cfg_save writes (cfg.hpp's format).
  std::string save_text() const;
  // One part of it (cfg.hpp § SAVING ONE PART).
  std::string save_text(const SaveScope& scope) const;

  struct Command {
    std::string help;
    CommandFn fn;
    uint32_t flags = 0;
    std::string layer;  // the cvars' layer when it was added (cvar.hpp § LAYERS)
  };
  const std::map<std::string, Command>& commands() const { return commands_; }

  // MESSAGE TRIGGERS (script.cpp): a line containing `match` queues `alias`.
  struct Trigger {
    std::string alias;
    std::string match;
    int level = kTriggerHigh;  // the print level it fires on (kTriggerTeam: the team's chat)
  };
  std::vector<Trigger>& triggers() { return triggers_; }

  // Where a name no code reads was first set (`set`, `seta`, `setinfo`), for `ge cfg check`.
  const Origin* created_at(std::string_view name) const;
  void note_created(std::string_view name, const Origin& where);

  // The command running now (its origin and source), for a command's own problems.
  const Args* current() const { return current_; }
  // Set the command problems are reported against (a tool reporting outside a run); null: none.
  void set_current(const Args* a) { current_ = a; }
  // Expansion of `$name` against macros and cvars (what run does before a command).
  std::string expand_text(std::string_view text) const;
  // Run one command now, bypassing the buffer (if's branches use the buffer; this is for tests).
  void run_now(std::string_view command, Source source = Source::local);

 private:
  struct Entry {
    std::string text;
    Source source;
    Origin where;
    int depth;
    std::string via;    // the alias or file it came from, for a depth problem
    bool team = false;  // queued by a line under a team macro limit: held to it too
  };
  void dispatch(Entry& e);
  // A command or an alias of that (lower-case) name.
  bool known_name(const std::string& name) const;
  void queue_front(std::string_view text, Source source, const Origin& where, int depth,
                   const std::string& via, bool keep_lines);
  void register_builtins();
  friend void register_script(Console&);
  friend void register_bind_commands(Console&);
  friend void register_builtin_commands(Console&);

  Cvars cvars_;
  Binds binds_;
  std::map<std::string, Command> commands_;
  std::map<std::string, CommandGate> gates_;
  struct CommandSynonym {
    std::string target, layer;
    bool own = false;  // runs in place of our command of the same name (`add_command_own`)
  };
  std::map<std::string, CommandSynonym> command_synonyms_;
  ManualInfo manual_;
  std::map<std::string, Alias> aliases_;
  std::map<std::string, std::function<std::string()>> macros_;
  std::string width_prefix_, align_prefix_;  // set_macro_widths
  std::set<std::string> team_macros_;        // MacroKind::team
  std::optional<TeamMacroLimit> macro_limit_;
  std::optional<TriggerQuiet> trigger_quiet_;
  std::string wait_owner_, wait_why_;  // limit_waits
  std::map<std::string, std::unique_ptr<Button>> buttons_;
  std::map<std::string, std::string> held_;  // key → the + command it went down with
  std::vector<Trigger> triggers_;
  std::string trigger_match_;  // $triggermatch: the line the last trigger fired on
  std::vector<std::shared_ptr<void>> kept_;
  std::map<std::string, Origin> created_;
  std::deque<Entry> buffer_;
  std::vector<std::filesystem::path> search_;
  std::filesystem::path config_;  // the session's config.cfg, or empty
  Reader reader_;
  Reader fallback_;
  std::function<bool(const Args&)> unknown_;
  std::map<std::string, ArgValues, std::less<>> arg_values_;
  Writer writer_;
  std::function<void(std::string_view)> sink_;
  std::function<void(std::string_view)> tap_;
  std::vector<std::string> output_;
  std::vector<Problem> problems_;
  const Args* current_ = nullptr;
  std::string current_via_;
  // Whether what the running command queues is held by the team macro limit.
  bool held_by_team() const;
  int current_depth_ = 0;
  int wait_limit_ = 0, waits_in_row_ = 0;  // limit_waits
  bool waiting_ = false;
  std::function<double()> clock_;  // set_clock: the host's clock, when it has one
  double wake_s_ = 0;              // a `sleep` holds the buffer until the host's time reaches it
  mutable bool expanded_team_ = false;  // the last expansion used a team macro
  bool current_team_ = false;           // the running line is held by a team macro limit
};

// The game `scope`'s own file beside a cfg file: "empezar.cfg" under "baseq2" is
// "empezar.baseq2.cfg". Empty with no scope or when `file` is already one (cfg_save and exec's
// pair).
std::string game_file(std::string_view file, std::string_view scope);

// The built-in command sets, each registered by the Console's constructor.
void register_builtin_commands(Console& c);  // console_builtins.cpp
// A RECIPE's commands (`sleep`; docs/console.md § Recipes), registered only by a host started to
// run recipes (a window's `-recipes`), never in a player's console: a cfg, a bind or a server
// cannot reach them.
void add_recipe_commands(Console& c);      // console_builtins.cpp
void register_script(Console& c);          // script.cpp
void register_script_strings(Console& c);  // script_strings.cpp
// `hash`'s number: each byte with bit 5 cleared (letters in either case alike), h = c + 64h +
// 65536h - h, wrapping at 2^32.
uint32_t text_hash(std::string_view text);
void register_bind_commands(Console& c);     // console_binds.cpp
void register_pattern_commands(Console& c);  // console_patterns.cpp
void register_more_commands(Console& c);     // console_more.cpp

}  // namespace ge::console
