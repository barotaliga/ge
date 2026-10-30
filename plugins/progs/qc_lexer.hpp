// THE QC LEXER: one source file's text as tokens (names, numbers, strings, vector literals,
// punctuation), with its model-description lines (`$frame a b c`) read as it goes, so `$a` in an
// expression arrives as the frame's number. Comments and whitespace are skipped; every token
// carries its line for the compiler's diagnostics.
//
// The language is the `progs` skill's (§ The QC language). Numbers are read as 32-bit floats in one
// correctly rounded step (`std::strtof`, the C locale), so a literal is the same float on every
// platform.
//
// ge:summary The QC lexer: tokens, frame macros, literals, each with its line
// ge:keywords qc lexer tokenizer token frame-macro dollar-frame string-literal vector-literal
// ge:keywords number-literal comment lex_file
// ge:skill progs
// ge:test tests/progs_qc_compile_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ge::progs {

enum class Tok : uint8_t {
  end,     // the end of the file
  name,    // an identifier or a keyword
  number,  // a float literal or a frame macro's value
  string,  // a string literal, escapes resolved, adjacent literals joined
  vector,  // 'x y z'
  punct,   // an operator or punctuation, in `text`
};

struct Token {
  Tok kind = Tok::end;
  std::string text;            // the name, the punctuation, or the string's contents
  float number = 0;            // Tok::number
  std::array<float, 3> vec{};  // Tok::vector
  int line = 0;
};

struct LexError {
  int line = 0;
  std::string message;
};

// Every token of `text` (the last one Tok::end), or the errors that stopped it.
struct Lexed {
  std::vector<Token> tokens;
  std::vector<LexError> errors;
};
Lexed lex_file(std::string_view text);

}  // namespace ge::progs
