// RULESETS: a competition's rules a player's client keeps, by name. A ruleset is data a game
// offers (its BOOK): the settings it HOLDS at one value or KEEPS inside a range (console locks,
// cvar.hpp § LOCKS) and the commands it REFUSES (console gates, console.hpp), some only while a
// match is under way. The player picks one with the `ruleset` setting; the client holds to it and
// says which it is, so everyone on a server knows the game is played the official way.
//
// - THE BOOK (`set_book`): the loaded game's rulesets, `default` first (no rules). A game swaps
//   its book when another game is loaded; a name the new book lacks falls back to `default`.
// - CHOOSING: `ruleset <name>` (the setting) takes effect at the next `update`; an unknown name is
//   refused with the book's names and the setting goes back. While CONNECTED (`set_connected`) the
//   setting itself is locked: a ruleset changes only between games, as a tournament asks.
// - HOLDING: every lock and gate of the chosen ruleset is set under the owner "ruleset"; `update`
//   locks again when the game registers settings later (a game's own layer loaded), so a lock
//   never misses a setting that arrives after the choice. Leaving a ruleset gives every setting
//   back the player's own value.
// - TELLING OTHERS is the game's: it reads `current()` into its user info and its chat replies
//   (plugins/classic/rulesets.hpp: `*rsn`, the f_ruleset reply).
//
// The `rulesets` command lists the book; `rulesets <name>` says what one holds.
//
// ge:summary Rulesets: a competition's locked settings and refused commands, chosen by name
// ge:keywords ruleset rulesets competition tournament official duel smackdown lock gate book
// ge:keywords set_book update set_connected in_match ruleset-locks refused-commands fair-play
// ge:skill console
// ge:test tests/console_ruleset_test.cpp
#pragma once

#include <functional>
#include <limits>
#include <span>
#include <string>
#include <string_view>

#include "console/console.hpp"

namespace ge::console {

// One setting a ruleset holds: at `value` when it is given, else inside [min, max].
struct RuleLock {
  std::string_view name;
  std::string_view value = {};
  double min = -std::numeric_limits<double>::infinity();
  double max = std::numeric_limits<double>::infinity();
};

// One command a ruleset refuses, always or only while a match is under way.
struct RuleRefusal {
  std::string_view command;
  bool match_only = false;
};

struct Ruleset {
  std::string_view name;   // what a player types ("smackdown"), lower case
  std::string_view title;  // one sentence a player reads: what it is for
  std::span<const RuleLock> locks = {};
  std::span<const RuleRefusal> refused = {};
  // Not empty: a line that expanded a team macro runs only these commands (console.hpp's
  // TeamMacroLimit), so a script cannot act on what the player holds.
  std::span<const std::string_view> team_macro_commands = {};
  // While a match is played no game line or event runs a trigger, and sound triggers never do.
  bool quiet_triggers = false;
  // More than 0: at most that many frames in a row may end on a `wait` (a script's loop).
  int wait_limit = 0;
};

class Rulesets {
 public:
  // Registers the `ruleset` setting (archived) and the `rulesets` command on `console`, in the
  // console's current cvar layer; `match` says whether a match is under way (null: never).
  explicit Rulesets(Console& console, std::function<bool()> match = {});
  Rulesets(const Rulesets&) = delete;
  Rulesets& operator=(const Rulesets&) = delete;

  // The loaded game's rulesets (kept by reference: static data); the chosen name applied again.
  void set_book(std::span<const Ruleset> book);
  std::span<const Ruleset> book() const { return book_; }
  // The setting's new value taken, and locks set again on settings registered since; cheap when
  // nothing moved (call it once a frame).
  void update();
  // Connected to a server: the `ruleset` setting locked until disconnected.
  void set_connected(bool on);
  // The ruleset in force (never null once a book is set).
  const Ruleset* current() const { return current_; }
  // `name` in the book, or null.
  const Ruleset* find(std::string_view name) const;
  // What `r` holds and refuses, one line each, as a player reads it.
  std::string describe(const Ruleset& r) const;

 private:
  void apply(const Ruleset* r);
  void lock_all();
  Console& console_;
  Cvar* cvar_ = nullptr;
  std::function<bool()> match_;
  std::span<const Ruleset> book_;
  const Ruleset* current_ = nullptr;
  uint32_t seen_revision_ = 0;
  size_t seen_size_ = 0;
  bool connected_ = false;
};

}  // namespace ge::console
