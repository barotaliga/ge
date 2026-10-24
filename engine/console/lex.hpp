// THE CONSOLE LANGUAGE'S TEXT: how a cfg file or a typed line becomes commands and words.
//
//   - A COMMAND ends at a newline or at a `;` outside double quotes. `//` outside quotes starts a
//     comment to the end of the line. A command is at most kMaxCommand characters.
//   - WORDS are split on blanks; a double-quoted run is one word with its quotes removed (no
//     escapes: a word cannot hold a `"`). An unclosed quote runs to the end of the command.
//   - EXPANSION, done when a command runs (never when an alias or a bind is defined, whose bodies
//     are quoted and kept as written): `$name` or `${name}` is replaced by a macro's or a cvar's
//     value (a name of a-z 0-9 _ . + -, letters first); `$$` is a literal `$`; an unknown name is
//     left as written. Inside an alias's body `$1` … `$9` are its arguments and `$*` all of them,
//     substituted when the alias is called.
//
// ge:summary The console language's text: commands split on ; and lines, words, $ expansion
// ge:keywords console lexer tokenize split_commands words quotes comment semicolon expand
// ge:keywords dollar cvar-expansion macro alias-arguments substitute_args
// ge:skill console
// ge:test tests/console_lex_test.cpp
#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace ge::console {

inline constexpr size_t kMaxCommand = 4096;

struct Line {
  std::string text;  // the command, trimmed, comments removed
  int line = 0;      // 1-based line of the text it starts on
};

// `text` split into commands (empty commands dropped).
std::vector<Line> split_commands(std::string_view text);

// One command split into words.
std::vector<std::string> tokenize(std::string_view command);

// The text after the first `skip` words, as written (quotes kept), trimmed.
std::string rest_after(std::string_view command, size_t skip);

// `$name` and `${name}` replaced by `lookup(name, value)` where it returns true; `$$` → `$`.
std::string expand(std::string_view text,
                   const std::function<bool(std::string_view, std::string&)>& lookup);

// An alias body with `$1` … `$9` and `$*` replaced by `args` (args[0] is the alias's name).
std::string substitute_args(std::string_view body, const std::vector<std::string>& args);

// `word` quoted for writing back into a command: as is when it needs no quotes.
std::string quote(std::string_view word);

}  // namespace ge::console
