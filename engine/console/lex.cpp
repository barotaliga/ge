// The console language's text, defined: the command splitter, the word splitter and expansion.
//
// ge:summary The console lexer's definitions: splitting, words, $ expansion, alias arguments
// ge:keywords console lexer split_commands tokenize expand substitute_args quote
// ge:skill console
// ge:test tests/console_lex_test.cpp
#include "console/lex.hpp"

namespace ge::console {

namespace {

bool blank(char c) { return c == ' ' || c == '\t' || c == '\r'; }

std::string trim(std::string_view s) {
  while (!s.empty() && blank(s.front())) s.remove_prefix(1);
  while (!s.empty() && blank(s.back())) s.remove_suffix(1);
  return std::string(s);
}

bool name_start(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }

bool name_char(char c) {
  return name_start(c) || (c >= '0' && c <= '9') || c == '.' || c == '+' || c == '-';
}

}  // namespace

std::vector<Line> split_commands(std::string_view text) {
  std::vector<Line> out;
  std::string cur;
  int line = 1, start = 1;
  bool quoted = false;
  auto flush = [&] {
    std::string t = trim(cur);
    if (!t.empty()) {
      if (t.size() > kMaxCommand) t.resize(kMaxCommand);
      out.push_back({std::move(t), start});
    }
    cur.clear();
  };
  for (size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    if (c == '\n') {
      flush();
      quoted = false;
      ++line;
      start = line;
      continue;
    }
    if (cur.empty() || trim(cur).empty()) start = line;
    if (c == '"') quoted = !quoted;
    if (!quoted && c == ';') {
      flush();
      continue;
    }
    if (!quoted && c == '/' && i + 1 < text.size() && text[i + 1] == '/') {
      while (i + 1 < text.size() && text[i + 1] != '\n') ++i;
      continue;
    }
    cur += c;
  }
  flush();
  return out;
}

std::vector<std::string> tokenize(std::string_view s) {
  std::vector<std::string> out;
  size_t i = 0;
  while (i < s.size()) {
    while (i < s.size() && blank(s[i])) ++i;
    if (i >= s.size()) break;
    std::string word;
    if (s[i] == '"') {
      ++i;
      while (i < s.size() && s[i] != '"') word += s[i++];
      if (i < s.size()) ++i;
    } else {
      while (i < s.size() && !blank(s[i])) word += s[i++];
    }
    out.push_back(std::move(word));
  }
  return out;
}

std::string rest_after(std::string_view s, size_t skip) {
  size_t i = 0;
  for (size_t w = 0; w < skip; ++w) {
    while (i < s.size() && blank(s[i])) ++i;
    if (i >= s.size()) return {};
    if (s[i] == '"') {
      ++i;
      while (i < s.size() && s[i] != '"') ++i;
      if (i < s.size()) ++i;
    } else {
      while (i < s.size() && !blank(s[i])) ++i;
    }
  }
  return trim(s.substr(i));
}

std::string expand(std::string_view s,
                   const std::function<bool(std::string_view, std::string&)>& lookup) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] != '$' || i + 1 >= s.size()) {
      out += s[i];
      continue;
    }
    if (s[i + 1] == '$') {
      out += '$';
      ++i;
      continue;
    }
    size_t a = i + 1, b = a;
    bool braced = false;
    if (s[a] == '{') {
      braced = true;
      ++a;
      b = a;
    }
    if (b < s.size() && name_start(s[b])) {
      ++b;
      while (b < s.size() && name_char(s[b])) ++b;
    }
    if (b == a || (braced && (b >= s.size() || s[b] != '}'))) {
      out += s[i];
      continue;
    }
    std::string value;
    const std::string_view name = s.substr(a, b - a);
    const size_t end = braced ? b + 1 : b;
    if (lookup(name, value)) {
      out += value;
    } else {
      out += s.substr(i, end - i);
    }
    i = end - 1;
  }
  return out;
}

std::string substitute_args(std::string_view body, const std::vector<std::string>& args) {
  std::string out;
  out.reserve(body.size());
  for (size_t i = 0; i < body.size(); ++i) {
    if (body[i] == '$' && i + 1 < body.size()) {
      const char n = body[i + 1];
      if (n >= '1' && n <= '9') {
        const size_t k = static_cast<size_t>(n - '0');
        if (k < args.size()) out += args[k];
        ++i;
        continue;
      }
      if (n == '*') {
        for (size_t k = 1; k < args.size(); ++k) {
          if (k > 1) out += ' ';
          out += quote(args[k]);
        }
        ++i;
        continue;
      }
      if (n == '$') {  // kept for expansion at run time
        out += "$$";
        ++i;
        continue;
      }
    }
    out += body[i];
  }
  return out;
}

std::string quote(std::string_view w) {
  bool needs = w.empty();
  for (char c : w)
    if (blank(c) || c == ';' || c == '/' || c == '$') needs = true;
  if (!needs) return std::string(w);
  return "\"" + std::string(w) + "\"";
}

}  // namespace ge::console
