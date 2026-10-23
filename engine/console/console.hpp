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
// purpose and runs on. Nothing here reads a clock: a frame is one `run`.
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
#include <string>
#include <string_view>
#include <vector>

#include "console/binds.hpp"
#include "console/cvar.hpp"

namespace ge::console {

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
  // `$name` expands to `fn()` at run time (a team message's $health), ahead of a cvar's value.
  void add_macro(std::string_view name, std::function<std::string()> fn);
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
  // `text` appended and run to the end, `wait`s included (a tool's or a test's way in).
  int execute(std::string_view text, Source source = Source::local);
  bool pending() const { return !buffer_.empty(); }

  // `exec`: the file's commands inserted at the front. False, with a problem, when not found.
  bool exec_file(std::string_view name, Source source = Source::local, const Origin& from = {});
  // The command line's `+cmd args …` runs (`+exec my.cfg +set r_x 1`), appended in order;
  // words before the first `+` are ignored. Returns how many commands it appended.
  int append_command_line(const std::vector<std::string>& args);

  // FILES: the search directories, the first also where cfg_save writes.
  void set_search_path(std::vector<std::filesystem::path> dirs);
  const std::vector<std::filesystem::path>& search_path() const { return search_; }
  // A host's own reader and writer in place of the directories.
  using Reader = std::function<bool(std::string_view name, std::string& text)>;
  using Writer = std::function<bool(std::string_view name, std::string_view text)>;
  void set_files(Reader read, Writer write);
  bool read_file(std::string_view name, std::string& text) const;
  bool write_file(std::string_view name, std::string_view text) const;

  // OUTPUT: every printed line goes to the sink (default: kept in `output()`).
  void set_sink(std::function<void(std::string_view)> sink);
  void print(std::string_view line);
  // A line from the game (a chat line, the server's print): printed, then every message trigger
  // it matches queues its alias.
  void message(std::string_view line);
  // Every message trigger `line` matches queues its alias (what `message` does after printing).
  void fire_triggers(std::string_view line);
  const std::vector<std::string>& output() const { return output_; }
  void clear_output() { output_.clear(); }

  // PROBLEMS since the last clear.
  const std::vector<Problem>& problems() const { return problems_; }
  void clear_problems() { problems_.clear(); }
  // Printed as "file:line: what (next)" and kept; `where` is the running command's when empty.
  void error(std::string_view what, std::string_view next,
             Problem::Severity severity = Problem::Severity::error);

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

  // COMPLETION: every command, alias and cvar starting with `prefix`, sorted.
  std::vector<std::string> complete(std::string_view prefix) const;
  // The nearest name to `name` (edit distance at most 2), or "".
  std::string suggest(std::string_view name) const;

  // The whole player config cfg_save writes (cfg.hpp's format).
  std::string save_text() const;

  struct Command {
    std::string help;
    CommandFn fn;
    uint32_t flags = 0;
  };
  const std::map<std::string, Command>& commands() const { return commands_; }

  // MESSAGE TRIGGERS (script.cpp): a line containing `match` queues `alias`.
  struct Trigger {
    std::string alias;
    std::string match;
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
    std::string via;  // the alias or file it came from, for a depth problem
  };
  void dispatch(Entry& e);
  void queue_front(std::string_view text, Source source, const Origin& where, int depth,
                   const std::string& via, bool keep_lines);
  void register_builtins();
  friend void register_script(Console&);
  friend void register_bind_commands(Console&);
  friend void register_builtin_commands(Console&);

  Cvars cvars_;
  Binds binds_;
  std::map<std::string, Command> commands_;
  std::map<std::string, Alias> aliases_;
  std::map<std::string, std::function<std::string()>> macros_;
  std::map<std::string, std::unique_ptr<Button>> buttons_;
  std::map<std::string, std::string> held_;  // key → the + command it went down with
  std::vector<Trigger> triggers_;
  std::vector<std::shared_ptr<void>> kept_;
  std::map<std::string, Origin> created_;
  std::deque<Entry> buffer_;
  std::vector<std::filesystem::path> search_;
  Reader reader_;
  Writer writer_;
  std::function<void(std::string_view)> sink_;
  std::vector<std::string> output_;
  std::vector<Problem> problems_;
  const Args* current_ = nullptr;
  int current_depth_ = 0;
  std::string current_via_;
  bool waiting_ = false;
};

// The built-in command sets, each registered by the Console's constructor.
void register_builtin_commands(Console& c);  // console_builtins.cpp
void register_script(Console& c);            // script.cpp
void register_bind_commands(Console& c);     // console_binds.cpp

}  // namespace ge::console
