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

std::string fit_width(std::string_view s, int width, char align) {
  const auto hex = [](char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
  };
  // Each letter's place and the room it takes: a code or brace none.
  std::string out;
  int letters = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '&' && i + 4 < s.size() && s[i + 1] == 'c' && hex(s[i + 2]) && hex(s[i + 3]) &&
        hex(s[i + 4])) {
      out.append(s.substr(i, 5));
      i += 4;
      continue;
    }
    if (s[i] == '&' && i + 1 < s.size() && s[i + 1] == 'r') {
      out.append("&r");
      ++i;
      continue;
    }
    if (s[i] == '{' || s[i] == '}') {
      out += s[i];
      continue;
    }
    if (letters == width) continue;  // cut: the codes after still kept
    out += s[i];
    ++letters;
  }
  const int pad = width - letters;
  if (pad <= 0) return out;
  const int before = align == 'r' ? pad : align == 'c' ? pad / 2 : 0;
  return std::string(static_cast<size_t>(before), ' ') + out +
         std::string(static_cast<size_t>(pad - before), ' ');
}

std::string substitute_args(std::string_view body, const std::vector<std::string>& args) {
  // Arguments `from`..`to` (inclusive, held to those given) joined by single blanks.
  const auto joined = [&args](size_t from, size_t to) {
    std::string s;
    for (size_t k = from; k <= to && k < args.size(); ++k) {
      if (k > from) s += ' ';
      s += args[k];
    }
    return s;
  };
  const auto digits = [&body](size_t& i, size_t& n) {
    const size_t from = i;
    n = 0;
    while (i < body.size() && body[i] >= '0' && body[i] <= '9' && i - from < 3)
      n = n * 10 + static_cast<size_t>(body[i++] - '0');
    return i > from;
  };
  // A body with a `%` in it takes `%0`..`%9` too, and `%%` is a `%` (an old config's form).
  const bool percents = body.find('%') != std::string_view::npos;
  std::string out;
  out.reserve(body.size());
  for (size_t i = 0; i < body.size(); ++i) {
    if (percents && body[i] == '%' && i + 1 < body.size()) {
      const char n = body[i + 1];
      if (n >= '0' && n <= '9') {
        out += n == '0' ? joined(1, args.size())
                        : (static_cast<size_t>(n - '0') < args.size()
                               ? args[static_cast<size_t>(n - '0')]
                               : std::string());
        ++i;
        continue;
      }
      if (n == '%') {
        out += '%';
        ++i;
        continue;
      }
    }
    if (body[i] == '$' && i + 1 < body.size()) {
      const char n = body[i + 1];
      if (n >= '0' && n <= '9') {
        const auto k = static_cast<size_t>(n - '0');
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
      if (n == '@') {
        out += joined(1, args.size());
        ++i;
        continue;
      }
      if (n == '{') {  // `${N-M}` and `${N-}`: a run of the arguments
        size_t at = i + 2, from = 0, to = 0;
        if (digits(at, from) && at < body.size() && body[at] == '-') {
          ++at;
          const bool upper = digits(at, to);
          if (at < body.size() && body[at] == '}') {
            out += joined(from, upper ? to : args.size());
            i = at;
            continue;
          }
        }
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
