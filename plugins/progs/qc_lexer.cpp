// The lexer's scanner (lexer.hpp).
//
// ge:summary The QC lexer's scanner: comments, literals, punctuation, the $ lines
// ge:keywords qc lexer scanner frame-macro strtof
// ge:skill progs
// ge:test tests/progs_qc_compile_test.cpp
#include "progs/qc_lexer.hpp"

#include <cstdlib>
#include <map>

namespace ge::progs {

namespace {

bool is_name_start(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
bool is_digit(char c) { return c >= '0' && c <= '9'; }
bool is_name_char(char c) { return is_name_start(c) || is_digit(c); }

struct Scanner {
  std::string_view s;
  size_t i = 0;
  int line = 1;
  bool line_start = true;  // only whitespace since the last newline
  std::map<std::string, float, std::less<>> frames;
  float next_frame = 0;
  Lexed out;

  char at(size_t k = 0) const { return i + k < s.size() ? s[i + k] : '\0'; }

  void error(std::string m) { out.errors.push_back({line, std::move(m)}); }

  void skip_space() {
    for (;;) {
      const char c = at();
      if (c == '\n') {
        ++line;
        ++i;
        line_start = true;
      } else if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') {
        ++i;
      } else if (c == '/' && at(1) == '/') {
        while (i < s.size() && at() != '\n') ++i;
      } else if (c == '/' && at(1) == '*') {
        const int opened = line;
        i += 2;
        while (i < s.size() && !(at() == '*' && at(1) == '/')) {
          if (at() == '\n') ++line;
          ++i;
        }
        if (i >= s.size()) {
          error("the comment opened on line " + std::to_string(opened) +
                " is never closed: add */");
          return;
        }
        i += 2;
      } else {
        return;
      }
    }
  }

  std::string word() {
    const size_t b = i;
    while (is_name_char(at())) ++i;
    return std::string(s.substr(b, i - b));
  }

  // A float literal at i: digits, an optional point and digits, an optional exponent.
  bool number(float& v) {
    const size_t b = i;
    if (at() == '-') ++i;
    while (is_digit(at())) ++i;
    if (at() == '.') {
      ++i;
      while (is_digit(at())) ++i;
    }
    if ((at() == 'e' || at() == 'E') &&
        (is_digit(at(1)) || ((at(1) == '-' || at(1) == '+') && is_digit(at(2))))) {
      i += 2;
      while (is_digit(at())) ++i;
    }
    const std::string text(s.substr(b, i - b));
    if (text.empty() || text == "-" || text == "." || text == "-.") return false;
    v = std::strtof(text.c_str(), nullptr);
    return true;
  }

  // A `$` line: `$frame a b c` names frames, `$framevalue N` restarts the count, the rest of the
  // model-description lines are read and ignored.
  void directive() {
    ++i;
    const std::string what = word();
    auto rest_words = [&] {
      std::vector<std::string> words;
      for (;;) {
        while (at() == ' ' || at() == '\t' || at() == '\r') ++i;
        if (at() == '\n' || at() == '\0' || (at() == '/' && at(1) == '/')) break;
        const size_t b = i;
        while (at() != ' ' && at() != '\t' && at() != '\r' && at() != '\n' && at() != '\0') ++i;
        words.emplace_back(s.substr(b, i - b));
      }
      while (at() != '\n' && at() != '\0') ++i;
      return words;
    };
    const std::vector<std::string> words = rest_words();
    if (what == "frame") {
      for (const std::string& w : words) frames[w] = next_frame++;
    } else if (what == "framevalue") {
      if (words.empty()) {
        error("$framevalue needs a number: $framevalue 10");
      } else {
        next_frame = std::strtof(words[0].c_str(), nullptr);
      }
    } else if (what != "cd" && what != "origin" && what != "base" && what != "skin" &&
               what != "flags" && what != "scale" && what != "modelname" && what != "spritename" &&
               what != "type" && what != "load" && what != "flush") {
      error("unknown model-description line $" + what +
            ": the known ones are $frame, $framevalue, $cd, $origin, $base, $skin, $flags, "
            "$scale and $modelname");
    }
  }

  void string_literal(Token& t) {
    for (;;) {
      ++i;  // the opening quote
      for (;;) {
        const char c = at();
        if (c == '\0' || c == '\n') {
          error("a string is not closed on its line: add the closing \"");
          return;
        }
        ++i;
        if (c == '"') break;
        if (c == '\\') {
          const char e = at();
          ++i;
          t.text += e == 'n' ? '\n' : e;
        } else {
          t.text += c;
        }
      }
      // Adjacent literals are one string.
      const size_t save_i = i;
      const int save_line = line;
      skip_space();
      if (at() != '"') {
        i = save_i;
        line = save_line;
        return;
      }
    }
  }

  void vector_literal(Token& t) {
    ++i;
    for (int k = 0; k < 3; ++k) {
      while (at() == ' ' || at() == '\t') ++i;
      if (!number(t.vec[size_t(k)])) {
        error("a vector literal is three numbers in single quotes: '0 0 1'");
        return;
      }
    }
    while (at() == ' ' || at() == '\t') ++i;
    if (at() != '\'') {
      error("a vector literal is three numbers in single quotes: '0 0 1'");
      return;
    }
    ++i;
  }

  void run() {
    static constexpr std::string_view kThree[] = {"..."};
    static constexpr std::string_view kTwo[] = {"==", "!=", "<=", ">=", "&&",
                                                "||", "+=", "-=", "*=", "/="};
    for (;;) {
      skip_space();
      const bool first_on_line = line_start;
      line_start = false;
      Token t;
      t.line = line;
      const char c = at();
      if (c == '\0') {
        out.tokens.push_back(t);
        return;
      }
      if (c == '$' && first_on_line) {
        directive();
        line_start = false;
        continue;
      }
      if (c == '$') {
        ++i;
        const std::string name = word();
        const auto it = frames.find(name);
        if (it == frames.end()) {
          std::string what = "unknown frame $" + name;
          what += ": declare it with $frame ";
          what += name;
          error(what);
        }
        t.kind = Tok::number;
        t.number = it == frames.end() ? 0 : it->second;
      } else if (is_name_start(c)) {
        t.kind = Tok::name;
        t.text = word();
      } else if (is_digit(c) || (c == '.' && is_digit(at(1)))) {
        t.kind = Tok::number;
        number(t.number);
      } else if (c == '"') {
        t.kind = Tok::string;
        string_literal(t);
      } else if (c == '\'') {
        t.kind = Tok::vector;
        vector_literal(t);
      } else {
        t.kind = Tok::punct;
        for (std::string_view p : kThree) {
          if (s.substr(i, p.size()) == p) t.text = p;
        }
        if (t.text.empty()) {
          for (std::string_view p : kTwo) {
            if (s.substr(i, p.size()) == p) t.text = p;
          }
        }
        if (t.text.empty()) {
          static constexpr std::string_view kOne = "{}()[];,.=+-*/<>!&|#:";
          if (kOne.find(c) == std::string_view::npos) {
            error(std::string("unexpected character '") + c + "'");
            ++i;
            continue;
          }
          t.text = std::string(1, c);
        }
        i += t.text.size();
      }
      out.tokens.push_back(std::move(t));
    }
  }
};

}  // namespace

Lexed lex_file(std::string_view text) {
  Scanner sc;
  sc.s = text;
  sc.run();
  return std::move(sc.out);
}

}  // namespace ge::progs
