// A CFG CHECKED AND TIDIED without running it: what a player's or a server's config sets, read
// against the settings a console registers (docs/console.md § A cfg checked and tidied).
//
// THE CHECK (`check_cfg`) reads each command of the text, never running one, and finds:
//   - a setting this game does not have (`set` of a name nothing reads, or a word that is no
//     setting, command or alias): a warning, with the nearest name when there is one;
//   - a value that fits none of the setting's own values (console/words.hpp's `value_fits`): a
//     warning with what the setting takes;
//   - a setting set twice: a warning on the first, which the later one overrides.
// A value with a `$` is left alone (it is expanded when the cfg runs), and so is an alias defined
// in the same text.
//
// THE FORMAT (`format_cfg`) writes the same commands back tidy: one command a line, a setting as
// `name "value"` (`set` and `seta` kept where written), a comment kept with the command it sits
// above or beside. With `comments` each setting gets its one sentence of help as a `//` line above
// it (written once: a cfg formatted again does not gather a second). With `pretty` (which implies
// comments) the settings are grouped into sections by their name's prefix, in one fixed order and
// by name inside each, under a banner, their values aligned in a column; binds follow by key,
// then aliases, then every other command in the order written (an `exec` still runs last). The
// commands themselves are never changed, so a formatted cfg sets exactly what the original did.
//
// ge:summary A cfg checked against a console's settings and written back tidy, commented or pretty
// ge:keywords cfg-check cfg-format check_cfg format_cfg config-validator config-formatter tidy
// ge:keywords pretty comments sections align unknown-setting invalid-value cfg-lint
// ge:skill console
// ge:test tests/console_cfg_tidy_test.cpp
#pragma once

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
};

// `text` written back tidy (above), each setting's words from `c`.
std::string format_cfg(const Console& c, std::string_view text, const CfgFormat& how);

// The section a setting's name belongs to in a pretty cfg ("Server", "Picture", ...).
std::string_view cfg_section(std::string_view name);

}  // namespace ge::console
