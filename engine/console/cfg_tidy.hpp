// A CFG CHECKED AND TIDIED without running it: what a player's or a server's config sets, read
// against the settings a console registers (docs/console.md § A cfg checked and tidied).
//
// THE CHECK (`check_cfg`) reads each command of the text, never running one, and finds:
//   - a setting this game does not have (`set` of a name nothing reads, or a word that is no
//     setting, command or alias): a warning, with the nearest name when there is one;
//   - a value that fits none of the setting's own values (console/words.hpp's `value_fits`): a
//     warning with what the setting takes;
//   - a setting set twice: a warning on the first, which the later one overrides.
// A part synonym (one word of a setting that holds several) is a setting of its own here, its
// value left to the server, which checks the whole.
// A value with a `$` is left alone (it is expanded when the cfg runs), and so is an alias defined
// in the same text.
//
// THE FORMAT (`format_cfg`) writes the same commands back tidy: one command a line, a setting as
// `name "value"` (`set` and `seta` kept where written), a comment kept with the command it sits
// above or beside. With `comments` each setting gets a `//` line above it: its one sentence of
// help, the values it names with what each does, and its default ("// How the ban list is read.
// 0: Only those on it join, 1: Those on it are refused. (Default: 1)."), written once: a cfg
// formatted again does not gather a second. With `pretty` (which implies
// comments) a setting written by another name (a synonym, console/cvar.hpp) takes its own, and
// the settings are grouped into sections by their name's prefix, in one fixed order and
// by name inside each, each section opened by a blank line and its header ("// SEATS - Who may
// join, how many, and who is refused"), one a line: the values aligned in a column and each
// setting's help beside it in another (`name  "value"  // help`); binds follow by key,
// then aliases, then every other command in the order written (an `exec` still runs last). The
// commands themselves are never changed, so a formatted cfg sets exactly what the original did.
// With `defaults` (pretty only) the settings the text leaves alone are written in their sections
// too, each at its default, which changes nothing either: a setting at its default is set to it.
//
// ge:summary A cfg checked against a console's settings and written back tidy, commented or pretty
// ge:keywords cfg-check cfg-format check_cfg format_cfg config-validator config-formatter tidy
// ge:keywords pretty comments sections align unknown-setting invalid-value cfg-lint
// ge:keywords defaults every-setting
// ge:skill console
// ge:test tests/console_cfg_tidy_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"

namespace ge::console {

// One thing the check found.
struct CfgFinding {
  int line = 0;      // 1-based line of the text
  std::string what;  // what is wrong, naming the setting and its value
  std::string next;  // what to do about it
};

// The findings of `text` read against `c`'s settings, commands and aliases, in line order.
std::vector<CfgFinding> check_cfg(const Console& c, std::string_view text);

struct CfgFormat {
  bool comments = false;  // each setting's help above it
  bool pretty = false;    // sections, a fixed order, values aligned (and comments)
  // With `pretty`: every setting carrying one of these flags (kServer for a server's cfg,
  // kArchive for a player's) that the text does not set is written too, at its default, so the
  // file shows everything it could set, each at its default; an empty default, left to the
  // setting's owner (a game's rule), is written as the one its words name (console/words.hpp's
  // `stated_default`). 0: only what the text sets.
  uint32_t defaults = 0;
};

// `text` written back tidy (above), each setting's words from `c`.
std::string format_cfg(const Console& c, std::string_view text, const CfgFormat& how);

// The section a setting's name belongs to in a pretty cfg ("Server", "Picture", ...).
std::string_view cfg_section(std::string_view name);

}  // namespace ge::console
