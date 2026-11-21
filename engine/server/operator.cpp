// The operator's words any server can mount (operator.hpp).
//
// ge:summary The operator's words for any server: picking a player, a line's raw rest, stuff lists
// ge:keywords operator pick-player stuff-lists line_rest
// ge:skill net
// ge:test tests/server_operator_test.cpp
#include "server/operator.hpp"

#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace ge::server {

namespace {

char lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

std::string lowered(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = lower(c);
  return out;
}

bool digits(std::string_view s) {
  if (s.empty()) return false;
  for (char c : s)
    if (c < '0' || c > '9') return false;
  return true;
}

bool space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

// The words of `line` as the console splits them: by spaces, a quoted run one word.
struct Word {
  size_t at, end;
};

std::vector<Word> words_at(std::string_view line) {
  std::vector<Word> out;
  size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && space(line[i])) i++;
    if (i >= line.size()) break;
    const size_t at = i;
    if (line[i] == '"') {
      i++;
      while (i < line.size() && line[i] != '"') i++;
      if (i < line.size()) i++;
    } else {
      while (i < line.size() && !space(line[i])) i++;
    }
    out.push_back({at, i});
  }
  return out;
}

std::string unquoted(std::string_view w) {
  if (w.size() >= 2 && w.front() == '"' && w.back() == '"')
    return std::string(w.substr(1, w.size() - 2));
  return std::string(w);
}

}  // namespace

int pick_player(const std::vector<PickSlot>& slots, std::string_view word, bool partial,
                std::string& error) {
  error.clear();
  if (word.empty()) return -1;
  if (digits(word)) {
    const long n = std::stol(std::string(word.substr(0, 9)));
    if (word.size() > 9 || n >= static_cast<long>(slots.size())) {
      error = "Bad client slot number: " + std::string(word) + "\n";
      return -1;
    }
    if (!slots[static_cast<size_t>(n)].held) {
      error = "Client slot " + std::to_string(n) + " is not active.\n";
      return -1;
    }
    return static_cast<int>(n);
  }
  for (size_t i = 0; i < slots.size(); i++)
    if (slots[i].held && slots[i].name == word) return static_cast<int>(i);
  if (!partial) {
    error = "Userid '" + std::string(word) + "' is not on the server.\n";
    return -1;
  }
  const std::string want = lowered(word);
  for (size_t i = 0; i < slots.size(); i++)
    if (slots[i].held && lowered(slots[i].name) == want) return static_cast<int>(i);
  int found = -1, count = 0;
  for (size_t i = 0; i < slots.size(); i++)
    if (slots[i].held && lowered(slots[i].name).find(want) != std::string::npos) {
      found = static_cast<int>(i);
      count++;
    }
  if (count == 1) return found;
  error = count == 0 ? "No clients matching '" + std::string(word) + "' found.\n"
                     : "'" + std::string(word) + "' matches multiple clients.\n";
  return -1;
}

std::string line_rest(std::string_view line, size_t n) {
  const std::vector<Word> w = words_at(line);
  if (n >= w.size()) return "";
  std::string_view rest = line.substr(w[n].at, w.back().end - w[n].at);
  if (rest.size() >= 2 && rest.front() == '"' && rest.back() == '"')
    rest = rest.substr(1, rest.size() - 2);
  return std::string(rest);
}

bool stuff_list_command(StuffLists& lists, std::string_view line, std::string& out) {
  const std::vector<Word> at = words_at(line);
  if (at.empty()) return false;
  std::vector<std::string> w;
  for (const Word& x : at) w.push_back(unquoted(line.substr(x.at, x.end - x.at)));
  const std::string& cmd = w[0];
  const bool add = cmd == "addstuffcmd", del = cmd == "delstuffcmd", list = cmd == "liststuffcmds";
  if (!add && !del && !list) return false;
  out.clear();
  if (add && w.size() < 3) {
    out = "Usage: addstuffcmd <list> <command>\n";
    return true;
  }
  if (del && w.size() < 3) {
    out = "Usage: delstuffcmd <list> <id|cmd|all>\n";
    return true;
  }
  if (list && w.size() != 2) {
    out = "Usage: liststuffcmds <list>\n";
    return true;
  }
  std::vector<std::string>* l = w[1] == "connect" ? &lists.connect
                                : w[1] == "begin" ? &lists.begin
                                                  : nullptr;
  if (l == nullptr) {
    out = "Unknown stuffcmd list: " + w[1] + "\n";
    return true;
  }
  if (add) {
    const std::string text = line_rest(line, 2);
    for (const std::string& e : *l)
      if (e == text) {
        out = "Stuffcmd already exists: " + text + "\n";
        return true;
      }
    l->push_back(text);
    return true;
  }
  if (l->empty()) {
    out = "No stuffcmds registered.\n";
    return true;
  }
  if (list) {
    out = "id command\n-- -------\n";
    char row[16];
    for (size_t i = 0; i < l->size(); i++) {
      std::snprintf(row, sizeof row, "%-2d ", static_cast<int>(i + 1));
      out += row + (*l)[i] + "\n";
    }
    return true;
  }
  const std::string arg = line_rest(line, 2);
  if (arg == "all") {
    l->clear();
    return true;
  }
  if (digits(arg)) {
    const long n = arg.size() > 9 ? 0 : std::stol(arg);
    if (n < 1 || n > static_cast<long>(l->size())) {
      out = "No such stuffcmd index: " + arg + "\n";
      return true;
    }
    l->erase(l->begin() + (n - 1));
    return true;
  }
  for (size_t i = 0; i < l->size(); i++)
    if ((*l)[i] == arg) {
      l->erase(l->begin() + static_cast<std::ptrdiff_t>(i));
      return true;
    }
  out = "No such stuffcmd string: " + arg + "\n";
  return true;
}

}  // namespace ge::server
