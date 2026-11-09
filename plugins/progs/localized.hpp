// A PROGRAM'S WORDS BY KEY: the localized-text table some programs print through. Such a program
// prints a KEY instead of the words (`$qc_got_item`, `$map_skill_normal`: a `$` and then lower-case
// letters, digits and underscores) and the server puts the words in before the text leaves it, so
// a client never sees a key. The table is the release's text file, read once.
//
// THE FILE (UTF-8, an optional byte-order mark skipped; lines split on LF):
// - leading spaces and tabs skipped; a line starting `/` is a comment; a line with no `=` (a blank
//   one too) is ignored;
// - the key is everything before the first `=`, trailing white space (a CR too) removed; it is
//   written without the `$`; keys match exactly, case and all; the first of two equal keys wins;
// - the value starts after the `=` and its white space: quoted (`"..."`, ending at the next
//   unescaped quote, the rest of the line dropped) or bare (to the line's end, trailing spaces and
//   tabs removed); escapes `\n` `\t` `\v` `\b` `\f` `\"` `\'`, any other `\x` gives `x`;
// - the text is turned into the classic 8-bit set as it is read: ASCII as it is, the typographic
//   quotes, dashes, the ellipsis and the no-break space as their ASCII forms, accented Latin
//   letters as their plain letter, anything else (or broken UTF-8) the font's box (11).
//
// A STRING IS A KEY only when the whole of it is: it starts with `$` and the rest names a row. A
// key the table lacks, a `$` inside a longer string and an empty string pass through unchanged.
//
// A PRINT OF A KEY WITH ARGUMENTS (the print builtins' first text argument, then the rest, each
// resolved as a key in its own right): when the first's words hold a placeholder, `{}` or `{N}`
// (N decimal digits) puts in argument N after the first (`{}` is argument 0), an index past the
// arguments puts in nothing and anything else in braces is copied as it is; with no placeholder
// (or no table) the words and every argument are joined in order, as a classic print joins them.
// The result is cut at the buffer's size.
//
// ge:summary A program's words by key: the localized-text file read, `$key` strings and their
// ge:summary placeholders resolved into the words before a print leaves the server
// ge:keywords localization localized loc-file loc_english words-by-key dollar-key placeholder
// ge:keywords print-key qc_ map_ rerelease read_localized localize_args utf8 classic-charset
// ge:skill progs
// ge:test tests/progs_localized_test.cpp
#pragma once

#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

namespace ge::progs {

struct Vm;

struct LocalizedText {
  std::unordered_map<std::string, std::string> words;  // by key (no `$`), in the classic set
};

// The rows of a text file in the format above added to `out` (rows it already has kept). Returns
// how many rows were added.
int read_localized(std::string_view file, LocalizedText& out);

// The words for `s` when the whole of it is a key the table has; else nullptr.
const std::string* localized(const LocalizedText& table, std::string_view s);
// `s` resolved: the words when it is a key the table has, else `s` itself.
std::string_view resolve_key(const LocalizedText* table, std::string_view s);

// The print rule above over `args` (the first the text, the rest its arguments) into `out`, at
// most `cap - 1` bytes and a NUL. A null table joins them as they are.
void localize_args(const LocalizedText* table, std::span<const std::string_view> args, char* out,
                   int cap);
// The same over the running builtin's arguments from `first` on (bprint, sprint, centerprint).
void localize_join_args(Vm& vm, const LocalizedText* table, int first, char* out, int cap);

}  // namespace ge::progs
