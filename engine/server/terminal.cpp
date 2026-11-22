// The definitions behind terminal.hpp.
//
// ge:summary The terminal line editor's definitions
// ge:keywords terminal line-editor history tab
// ge:skill net console
// ge:test tests/server_terminal_test.cpp
#include "server/terminal.hpp"

#include <algorithm>
#include <fstream>

#include "console/tab_list.hpp"

namespace ge::server {

namespace {

void remember(TermEdit& t, const std::string& line) {
  if (line.empty()) return;
  t.history.push_back(line);
  if (t.history.size() > static_cast<size_t>(kHistoryKept))
    t.history.erase(t.history.begin(), t.history.end() - static_cast<std::ptrdiff_t>(kHistoryKept));
}

void walk(TermEdit& t, int step) {
  if (t.history.empty()) return;
  const int last = static_cast<int>(t.history.size()) - 1;
  int to = 0;
  if (step < 0) {
    if (t.at < 0) t.typed = t.line;
    to = t.at < 0 ? last : std::max(0, t.at - 1);
  } else {
    if (t.at < 0) return;
    to = t.at + 1 > last ? -1 : t.at + 1;
  }
  t.at = to;
  t.line = to < 0 ? t.typed : t.history[static_cast<size_t>(to)];
  t.cursor = t.line.size();
}

// The command word the cursor is in ("" past it), its slash off.
std::string head(const TermEdit& t) {
  const bool slash = !t.line.empty() && (t.line[0] == '/' || t.line[0] == '\\');
  std::string h = t.line.substr(slash ? 1 : 0);
  if (h.empty() || h.find_first_of(" \t;") != std::string::npos) return "";
  return h;
}

void key(TermEdit& t, const console::Console& con, const std::string& k, std::string& out) {
  if (k == "left") {
    if (t.cursor > 0) t.cursor--;
  } else if (k == "right") {
    if (t.cursor < t.line.size()) t.cursor++;
  } else if (k == "home") {
    t.cursor = 0;
  } else if (k == "end") {
    t.cursor = t.line.size();
  } else if (k == "up") {
    walk(t, -1);
  } else if (k == "down") {
    walk(t, 1);
  } else if (k == "delete") {
    if (t.cursor < t.line.size()) t.line.erase(t.cursor, 1);
  } else if (k == "backspace") {
    if (t.cursor > 0) t.line.erase(--t.cursor, 1);
  } else if (k == "cut-start") {
    t.line.erase(0, t.cursor);
    t.cursor = 0;
  } else if (k == "cut-end") {
    t.line.erase(t.cursor);
  } else if (k == "tab") {
    const console::TabList l =
        console::tab_list(con, t.line, head(t), t.mode, t.threshold, t.asked);
    t.line = l.input;
    t.cursor = t.line.size();
    if (!l.echo.empty()) {
      out += term_hide() + l.echo + "\r\n";
      for (const std::string& s : l.lines) out += s + "\r\n";
    }
  }
  out += term_redraw(t);
}

}  // namespace

std::string term_hide() { return "\r\x1b[K"; }

std::string term_redraw(const TermEdit& t) {
  std::string out = term_hide() + "]" + t.line;
  if (t.cursor < t.line.size()) out += "\x1b[" + std::to_string(t.line.size() - t.cursor) + "D";
  return out;
}

void term_feed(TermEdit& t, const console::Console& con, char c, std::string& out,
               std::vector<std::string>& done) {
  if (!t.escape.empty() || c == '\x1b') {  // ESC [ X, ESC [ n ~, ESC O X
    t.escape.push_back(c);
    const std::string& e = t.escape;
    if (e.size() < 3) {
      if (e.size() == 2 && e[1] != '[' && e[1] != 'O') t.escape.clear();
      return;
    }
    const char last = e.back();
    if (last >= '0' && last <= '9') {
      if (e.size() > 6) t.escape.clear();
      return;
    }
    const std::string body = e.substr(2);
    t.escape.clear();
    static const std::pair<const char*, const char*> kKeys[] = {
        {"A", "up"},   {"B", "down"}, {"C", "right"},  {"D", "left"},
        {"H", "home"}, {"F", "end"},  {"1~", "home"},  {"7~", "home"},
        {"4~", "end"}, {"8~", "end"}, {"3~", "delete"}};
    for (const auto& [seq, name] : kKeys)
      if (body == seq) return key(t, con, name, out);
    return;
  }
  switch (c) {
    case '\r':
    case '\n': {
      out += "\r\n";
      done.push_back(t.line);
      remember(t, t.line);
      t.line.clear();
      t.cursor = 0;
      t.at = -1;
      t.typed.clear();
      t.asked.clear();
      return;
    }
    case '\t':
      return key(t, con, "tab", out);
    case 127:
    case 8:
      return key(t, con, "backspace", out);
    case 1:
      return key(t, con, "home", out);
    case 5:
      return key(t, con, "end", out);
    case 21:
      return key(t, con, "cut-start", out);
    case 11:
      return key(t, con, "cut-end", out);
    default:
      break;
  }
  if (static_cast<unsigned char>(c) < 32) return;
  t.line.insert(t.cursor++, 1, c);
  out += term_redraw(t);
}

void term_history_load(TermEdit& t, const std::string& path, int keep) {
  if (keep <= 0) return;
  std::ifstream f(path);
  std::vector<std::string> lines;
  for (std::string line; std::getline(f, line);) {
    while (!line.empty() && line.back() == '\r') line.pop_back();
    if (!line.empty()) lines.push_back(line);
  }
  const size_t n = std::min(lines.size(), static_cast<size_t>(std::min(keep, kHistoryKept)));
  t.history.assign(lines.end() - static_cast<std::ptrdiff_t>(n), lines.end());
}

bool term_history_save(const TermEdit& t, const std::string& path, int keep) {
  if (keep <= 0) return true;
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  const size_t n = std::min(t.history.size(), static_cast<size_t>(std::min(keep, kHistoryKept)));
  for (size_t i = t.history.size() - n; i < t.history.size(); i++) f << t.history[i] << '\n';
  return static_cast<bool>(f);
}

}  // namespace ge::server
