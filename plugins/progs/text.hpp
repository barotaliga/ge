// QC'S TEXT RULES: how the standard builtins turn numbers into text and back, and split a line into
// words. Pure functions over plain buffers (no VM), so the builtins (stdlib.hpp), the game's
// command routing and the tests share one statement of each rule.
//
// - `ftos`: a whole number (equal to its truncation) as an integer ("-0" prints "0"); anything else
//   with one decimal right-aligned in five characters ("  1.5"), the exact decimal rounded half
//   away from zero; NaN and the infinities as "nan", "inf", "-inf" in the same width; **ours**: at
//   or past 1e21 the shortest exponent form ("3.4028234663852886e+38"), so the text is the same on
//   every platform (core/num_text.hpp's to_fixed).
// - `vtos`: "'x y z'", each component as the five-wide one-decimal form.
// - `stof`: leading characters at or below a space skipped; an optional sign; "0x" starts
//   hexadecimal digits; a `'` takes the next character's code; otherwise decimal digits with one
//   optional point, accumulated in double with the fractional digits divided out by repeated
//   division by 10; rounded to float. Reading stops at the first other character.
// - `tokenize` (the WORDS): spaces, tabs and carriage returns skipped; a newline or the end stops;
//   `//` runs to the end of the line and is skipped; a `"` starts a quoted word that runs to the
//   next `"` or the end (the quotes dropped); otherwise a word runs to the next space, tab,
//   newline or carriage return. At most 80 words and 1024 bytes of words (each word's bytes and
//   its terminator counted), a word at most 1023 bytes; the text from where the second word
//   starts to the end of the line is the ARGUMENTS line hooks are handed.
//
// ge:summary QC's text rules: ftos, vtos, stof, and the words of tokenize with the arguments line
// ge:keywords qc progs text ftos vtos stof tokenize words argc argv arguments-line number-text
// ge:keywords format_ftos format_vtos parse_stof tokenize_words
// ge:skill progs
// ge:test tests/progs_text_test.cpp
#pragma once

#include <cstdint>

namespace ge::progs {

constexpr int kMaxWords = 80;
constexpr int kWordBytes = 1024;     // all the words, a terminator each
constexpr int kMaxWordBytes = 1023;  // one word
constexpr int kArgsBytes = 2048;     // the arguments line

struct Words {
  int count = 0;
  int at[kMaxWords] = {};      // each word's start in `text`
  char text[kWordBytes] = {};  // the words, NUL-terminated one after another
  char args[kArgsBytes] = {};  // from the second word's start to the end of the line
};

// `out` must hold kFtosBytes for ftos, kVtosBytes for vtos (room for the largest float, 39 digits).
constexpr int kFtosBytes = 64;
constexpr int kVtosBytes = 3 * kFtosBytes + 8;
void format_ftos(float v, char* out);
void format_vtos(float x, float y, float z, char* out);
float parse_stof(const char* s);

// Splits `line` into `w` (replacing what it held); returns the count.
int tokenize_words(const char* line, Words& w);
inline const char* word(const Words& w, int i) {
  return i >= 0 && i < w.count ? w.text + w.at[i] : "";
}

}  // namespace ge::progs
