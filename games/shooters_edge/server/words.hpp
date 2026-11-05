// A GAME'S OWN WORDS: the texts a loaded classic game prints (its obituaries, its countdown, its
// clock), kept as templates in the game's profile (profiles/words.hpp) and filled in here, so the
// baseline mod and the lobby say a classic game's words while Shooter's Edge's own content keeps
// its own.
//
// THE TEMPLATE is plain ASCII, so it travels as a server line (`localinfo <key> "<template>"`)
// and a cfg can override one; `fill_words` turns it into the bytes a client draws:
//   %1 .. %9    the argument, as it is (a player's name keeps its own bytes)
//   %g1         the argument with its digits the gold figures (bytes 146..155)
//   %r1         the argument in red text (33..127 plus 128; spaces stay plain)
//   %>4g1       any of the above right-aligned to 4 bytes, the spaces before it (width 1..9)
//   ^           red text on or off for the template's own letters (`^is ready^`)
//   %[ %]       the gold brackets (bytes 144 and 145); %. the centred dot (143)
//   %l          a rule 38 glyphs wide (157, 36 of 158, 159)
//   %n          a newline; %% a percent sign; ^^ a caret
// An argument that is not given is empty; a `%` before anything else is kept as it is.
//
// THE LOOKUP: the server's local info under the key (`words_of`), empty when the game names none:
// then the caller says its own words. The mod reads them through the `words` builtin (#640,
// extension GE_QC_WORDS): `words(key, a, b, c, d)` returns the filled text, or "" with no such key.
//
// ge:summary A game's own words: templates with gold digits, red text and padding, filled in
// ge:keywords words templates fill_words gold-digits red-text redtext brackets padding localinfo
// ge:keywords classic-messages obituary countdown words-builtin ge_qc_words
// ge:skill shooter progs
// ge:test tests/shooters_edge_words_test.cpp
#pragma once

#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace shooters_edge::server {

// Red text: every byte 33..127 with 128 added; spaces and bytes already high stay as they are.
std::string red_text(std::string_view s);
// The digits '0'..'9' as the gold figures (146..155); other bytes as they are.
std::string gold_digits(std::string_view s);

// `tmpl` filled with `args` (the rules above).
std::string fill_words(std::string_view tmpl, std::span<const std::string> args);

// The template under `key` in `info` (a server's local info), or "" when there is none.
std::string_view words_of(const std::vector<std::pair<std::string, std::string>>& info,
                          std::string_view key);

// The template under `key` filled with `args`, or "" when `info` has no such key.
std::string say_words(const std::vector<std::pair<std::string, std::string>>& info,
                      std::string_view key, std::span<const std::string> args);

// Why a template cannot travel as a server line (a quote, a backslash, a newline or a byte outside
// printable ASCII), or "" when it can.
std::string words_fault(std::string_view tmpl);

}  // namespace shooters_edge::server
