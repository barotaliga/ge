// THE CVAR CATALOG: every setting a program registers, written down as data so a tool that does
// not link the program can still describe, check and set its settings. A game writes it beside
// the player's config each time it saves (`kCatalogFile`, `cvars.txt`); its player CLI
// (engine/gamecli) reads it to list every setting with its default, range, tier and help, and
// registers the entries on a console of its own so a value is clamped and refused exactly as the
// game would.
//
// THE FORMAT is text a diff can read: a comment line, then one cvar a line, fields split by a
// tab, in name order:
//
//   name <TAB> kind <TAB> flags <TAB> default <TAB> min <TAB> max <TAB> help <TAB> values
//
// `kind` is text, number, integer or boolean; `flags` cfg.hpp's `flags_text` (eight places, a
// letter or a dot: `A.......` a player's archived setting); `min` and `max` the range as written by
// the shortest round trip (`-` for none); `values` what each value does (console/words.hpp), which
// a catalog written before it lacks (seven fields, read as none). A tab or a line break inside a
// default or a help is written as a space. Created cvars (made by `set` at run time, no code reads
// them) are left out.
//
// ge:summary The cvar catalog: every registered cvar as text, read back to register them again
// ge:keywords cvar catalog cvars-txt catalog_text read_catalog add_catalog settings describe
// ge:keywords player-cli gamecli kind flags default range help
// ge:skill console agent-interface
// ge:test tests/gamecli_settings_test.cpp
#pragma once

#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "console/cvar.hpp"

namespace ge::console {

inline constexpr std::string_view kCatalogFile = "cvars.txt";

// One cvar as the catalog keeps it (owning its text, unlike CvarSpec).
struct CatalogEntry {
  std::string name, fallback, help, values;
  uint32_t flags = 0;
  CvarKind kind = CvarKind::text;
  double min = -std::numeric_limits<double>::infinity();
  double max = std::numeric_limits<double>::infinity();
};

// Every cvar of `cvars` but the created ones, in name order.
std::string catalog_text(const Cvars& cvars);
// `text` read into `out`; false with `error` (the line and what is wrong with it, and the next
// action) on a line that is not a catalog entry.
bool read_catalog(std::string_view text, std::vector<CatalogEntry>& out, std::string& error);
// Each entry registered on `cvars`, owning its value at its default (a value `set` already
// created is kept, as `Cvars::add` keeps it).
void add_catalog(Cvars& cvars, const std::vector<CatalogEntry>& entries);
// The kind's word in the catalog, and back (false for a word that is none).
const char* kind_name(CvarKind kind);
bool kind_of(std::string_view word, CvarKind& out);
// cfg.hpp's flag letters read back.
uint32_t flags_of(std::string_view letters);

}  // namespace ge::console
