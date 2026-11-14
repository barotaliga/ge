// A SETTING'S WORDS: what every cvar says about itself, so `help`, a commented cfg and a cfg check
// all read one source (docs/console.md § A setting's words).
//
// THE HELP is ONE SENTENCE: it starts with a capital letter (or a digit) and ends with a full stop,
// with no other sentence inside it, and says what the setting does with its unit: "The most
// players the server seats at once."
//
// THE VALUES say what each value does, entries separated by "; ", each "<pattern>: <meaning>":
//   0: off; 1: on                     a literal value, then what it does
//   0: no limit; 1..32: that many     a number range, both ends included; N for no bound that
//                                     way ("1..N": one and up)
//   lan: ...; invite: ...; listed: .. literal words
//   "": none                          the empty value
//   N: ...                            any number
//   TEXT: ...                         any text
//   N S T: ...                        several words, each a pattern (a number each here)
// A value FITS (`value_fits`) when one entry's pattern matches it, word for word; a setting with
// no values fits anything, and one whose default is empty (unset: its owner's own default
// applies, as a game's rules have) also fits the empty value. A meaning marks the default with
// "(the default)". The meanings are fragments, read after the pattern ("0: off").
//
// `words_fault` names what is wrong with a cvar's words (an empty help, two sentences, values
// that do not parse), or "" when they are right; a game's test runs it over every cvar it has.
//
// ge:summary A setting's words: one-sentence help, what each value does, and whether a value fits
// ge:keywords help values cvar-help explanation value-pattern value_fits words_fault sentence
// ge:keywords setting-words cfg-check cfg-comments stated_default
// ge:skill console
// ge:test tests/console_words_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "console/cvar.hpp"

namespace ge::console {

// One entry of a values text.
struct ValueEntry {
  std::string pattern;  // as written: "0", "1..32", "lan", "\"\"", "N", "TEXT", "N S T"
  std::string meaning;
};

// `values` split into its entries; false when one has no ": " or an empty side.
bool parse_values(std::string_view values, std::vector<ValueEntry>& out);
// Whether `value` matches `pattern` (above).
bool pattern_fits(std::string_view pattern, std::string_view value);
// Whether `value` is one the cvar's values allow (and within its kind's range); a cvar with no
// values text checks its range alone.
bool value_fits(const Cvar& cvar, std::string_view value);
// What each entry means for the cvar's value now ("" when none matches).
std::string value_meaning(const Cvar& cvar, std::string_view value);
// What is wrong with a cvar's words, or "" when nothing is.
std::string words_fault(const Cvar& cvar);
// The default the values text names: a literal entry whose meaning says "(the default)" ("1"
// from "1: normal (the default)"), or the value inside "(<value> the default)" ("10" from
// "0..1000: that rate (10 the default)"); "" when it names none.
std::string stated_default(std::string_view values);

}  // namespace ge::console
