// The definitions behind diagnostic.hpp: the prompt rule and two small lexers (C++, CMake) that
// find the diagnostic calls and gather their string literals.
//
// ge:summary The prompt rule's definition and the C++ and CMake lexers that find diagnostic calls
// ge:keywords diagnostics lexer string-literal comment cmake message fatal-error prompt_problem
// ge:skill agent-interface
// ge:test tests/cli_diagnostics_test.cpp
#include "cli/diagnostic.hpp"

#include <cctype>

namespace ge::cli {

namespace {

bool ident_char(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

int count_words(std::string_view s) {
  int n = 0;
  bool in = false;
  for (const char c : s) {
    const bool space = std::isspace(static_cast<unsigned char>(c)) != 0;
    if (!space && !in) n++;
    in = !space;
  }
  return n;
}

// Whether a qualified C++ name is one of the diagnostic helpers.
bool is_cpp_diagnostic(std::string_view name) {
  for (const std::string_view call : {"output::error", "output::warn", "out::error", "out::warn"}) {
    if (name == call) return true;
    if (name.size() > call.size() && name.ends_with(call) &&
        name.substr(name.size() - call.size() - 2, 2) == "::") {
      return true;
    }
  }
  return name == "GE_ASSERT";
}

// A C++ cursor that steps over comments and literals. `pos` always sits in code.
struct CppLexer {
  std::string_view s;
  size_t pos = 0;
  int line = 1;

  bool done() const { return pos >= s.size(); }
  void advance(size_t n) {
    for (size_t k = 0; k < n && pos < s.size(); k++) line += s[pos++] == '\n' ? 1 : 0;
  }
  // Skips one comment or literal at `pos` (appending a string literal's contents to `out` when
  // given); false when `pos` starts neither.
  bool skip_non_code(std::string* out) {
    if (s.substr(pos, 2) == "//") {
      while (!done() && s[pos] != '\n') advance(1);
      return true;
    }
    if (s.substr(pos, 2) == "/*") {
      advance(2);
      while (!done() && s.substr(pos, 2) != "*/") advance(1);
      advance(2);
      return true;
    }
    const bool raw = s.substr(pos, 2) == "R\"" && (pos == 0 || !ident_char(s[pos - 1]));
    if (raw) {
      const size_t open = s.find('(', pos);
      if (open == std::string_view::npos) return false;
      const std::string close = ")" + std::string(s.substr(pos + 2, open - pos - 2)) + "\"";
      const size_t end = s.find(close, open);
      const size_t stop = end == std::string_view::npos ? s.size() : end;
      if (out) out->append(s.substr(open + 1, stop - open - 1));
      advance(stop + close.size() - pos);
      return true;
    }
    if (s[pos] == '"' || s[pos] == '\'') {
      const char quote = s[pos];
      advance(1);
      while (!done() && s[pos] != quote && s[pos] != '\n') {
        if (s[pos] == '\\' && pos + 1 < s.size()) {
          if (out && quote == '"') {
            const char e = s[pos + 1];
            out->push_back(e == 'n' ? '\n' : e == 't' ? '\t' : e);
          }
          advance(2);
          continue;
        }
        if (out && quote == '"') out->push_back(s[pos]);
        advance(1);
      }
      advance(1);
      return true;
    }
    return false;
  }
};

void cpp_sites(std::string_view s, std::vector<DiagnosticSite>& sites) {
  CppLexer lx{s};
  char prev = ' ';  // the last non-space code character before an identifier
  while (!lx.done()) {
    if (lx.skip_non_code(nullptr)) continue;
    const char c = s[lx.pos];
    if (!ident_char(c) || std::isdigit(static_cast<unsigned char>(c))) {
      if (!std::isspace(static_cast<unsigned char>(c))) prev = c;
      lx.advance(1);
      continue;
    }
    // A qualified name: ident(::ident)*.
    const size_t start = lx.pos;
    const int line = lx.line;
    size_t end = start;
    while (end < s.size()) {
      while (end < s.size() && ident_char(s[end])) end++;
      if (s.substr(end, 2) == "::" && end + 2 < s.size() && ident_char(s[end + 2])) {
        end += 2;
        continue;
      }
      break;
    }
    const std::string_view name = s.substr(start, end - start);
    const bool member_fail = name == "fail" && (prev == '.' || prev == '>');
    lx.advance(end - start);
    prev = 'a';
    if (!is_cpp_diagnostic(name) && !member_fail) continue;
    size_t k = lx.pos;
    while (k < s.size() && std::isspace(static_cast<unsigned char>(s[k]))) k++;
    if (k >= s.size() || s[k] != '(') continue;
    lx.advance(k + 1 - lx.pos);
    // The arguments, to the matching ')', with their string literals gathered.
    std::string text;
    int depth = 1;
    while (!lx.done() && depth > 0) {
      if (lx.skip_non_code(&text)) continue;
      depth += s[lx.pos] == '(' ? 1 : s[lx.pos] == ')' ? -1 : 0;
      lx.advance(1);
    }
    if (!text.empty()) {
      sites.push_back({line, member_fail ? std::string(".fail") : std::string(name), text});
    }
  }
}

void cmake_sites(std::string_view s, std::vector<DiagnosticSite>& sites) {
  size_t pos = 0;
  int line = 1;
  auto advance = [&](size_t n) {
    for (size_t k = 0; k < n && pos < s.size(); k++) line += s[pos++] == '\n' ? 1 : 0;
  };
  // Skips a comment, a quoted argument (gathered into `out`) or a bracket argument.
  auto skip_non_code = [&](std::string* out) {
    if (s[pos] == '#') {
      while (pos < s.size() && s[pos] != '\n') advance(1);
      return true;
    }
    if (s.substr(pos, 2) == "[[") {
      const size_t end = s.find("]]", pos);
      advance((end == std::string_view::npos ? s.size() : end + 2) - pos);
      return true;
    }
    if (s[pos] == '"') {
      advance(1);
      while (pos < s.size() && s[pos] != '"') {
        if (s[pos] == '\\' && pos + 1 < s.size()) {
          if (out) out->push_back(s[pos + 1] == 'n' ? '\n' : s[pos + 1]);
          advance(2);
          continue;
        }
        if (out) out->push_back(s[pos]);
        advance(1);
      }
      advance(1);
      return true;
    }
    return false;
  };
  while (pos < s.size()) {
    if (skip_non_code(nullptr)) continue;
    if (!ident_char(s[pos]) || (pos > 0 && ident_char(s[pos - 1]))) {
      advance(1);
      continue;
    }
    size_t end = pos;
    while (end < s.size() && ident_char(s[end])) end++;
    const std::string name = lower(s.substr(pos, end - pos));
    const int start_line = line;
    advance(end - pos);
    if (name != "message") continue;
    size_t k = pos;
    while (k < s.size() && std::isspace(static_cast<unsigned char>(s[k]))) k++;
    if (k >= s.size() || s[k] != '(') continue;
    advance(k + 1 - pos);
    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) advance(1);
    size_t mode_end = pos;
    while (mode_end < s.size() && ident_char(s[mode_end])) mode_end++;
    const std::string_view mode = s.substr(pos, mode_end - pos);
    const bool diagnostic = mode == "FATAL_ERROR" || mode == "SEND_ERROR" || mode == "WARNING" ||
                            mode == "AUTHOR_WARNING";
    std::string text;
    int depth = 1;
    while (pos < s.size() && depth > 0) {
      if (skip_non_code(&text)) continue;
      depth += s[pos] == '(' ? 1 : s[pos] == ')' ? -1 : 0;
      advance(1);
    }
    if (diagnostic && !text.empty()) {
      sites.push_back({start_line, "message(" + std::string(mode), text});
    }
  }
}

}  // namespace

std::string prompt_problem(std::string_view message) {
  if (count_words(message) < 3) {
    return "says what failed in fewer than three words: \"" + std::string(message) + "\"";
  }
  const std::string text = lower(message);
  for (const std::string_view marker : kNextMarkers) {
    if (text.find(marker) != std::string::npos) return "";
  }
  return "names no next action: end it with \"next: <what to do>\", \"skill: <name>\", a "
         "command (make ..., run ...) or a doc (see ..., docs/...)";
}

std::vector<DiagnosticSite> diagnostic_sites(std::string_view source, SourceKind kind) {
  std::vector<DiagnosticSite> sites;
  if (kind == SourceKind::cpp) {
    cpp_sites(source, sites);
  } else {
    cmake_sites(source, sites);
  }
  return sites;
}

}  // namespace ge::cli
