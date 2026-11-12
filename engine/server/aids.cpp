// Player aids, defined: finding an aid, a grant's value, the order and the vote's words, the
// players list and the join line.
//
// ge:summary Player aids defined: grants' values, orders, votes' words, the players list
// ge:keywords player-aids users_text aids_join_line aid_parse aid_value aid_share_text
// ge:skill net
// ge:test tests/server_aids_test.cpp
#include "server/aids.hpp"

#include <cstdlib>

#include "core/num.hpp"
#include "core/num_text.hpp"

namespace ge::server {

namespace {

std::string pad_left(const std::string& s, size_t width) {
  return s.size() < width ? std::string(width - s.size(), ' ') + s : s;
}

// The words of `line`, split on spaces.
std::vector<std::string> words_of(std::string_view line) {
  std::vector<std::string> w;
  size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && line[i] == ' ') i++;
    const size_t start = i;
    while (i < line.size() && line[i] != ' ') i++;
    if (i > start) w.emplace_back(line.substr(start, i - start));
  }
  return w;
}

}  // namespace

const AidKind* aid_find(std::span<const AidKind> kinds, std::string_view name) {
  for (const AidKind& k : kinds)
    if (name == k.name) return &k;
  return nullptr;
}

std::string aid_share_text(float share) {
  std::string t = core::num::to_fixed(static_cast<double>(share), 2);
  while (!t.empty() && t.back() == '0') t.pop_back();
  if (!t.empty() && t.back() == '.') t.pop_back();
  return t;
}

bool aid_parse_share(std::string_view text, float& out) {
  if (text.empty()) return false;
  const std::string s(text);
  char* end = nullptr;
  const double v = std::strtod(s.c_str(), &end);
  if (end == s.c_str() || *end != '\0' || !(v >= 0 && v <= 1)) return false;
  out = static_cast<float>(core::num::round(v * 100)) / 100.0f;
  return true;
}

bool aid_value(const AidKind& kind, std::string_view word, std::string& value, std::string& error) {
  if (word == "on") {
    value = kind.on;
    return true;
  }
  if (word == "off") {
    value.clear();
    return true;
  }
  float share = 0;
  if (kind.share && aid_parse_share(word, share)) {
    value = share > 0 ? aid_share_text(share) : std::string();
    return true;
  }
  error = std::string(kind.words) + " is turned \"on\" or \"off\"" +
          (kind.share ? " or given a share from 0 to 1" : "") + ", not \"" + std::string(word) +
          "\".";
  return false;
}

bool aid_parse(std::span<const AidKind> kinds, std::string_view line, AidOrder& out,
               std::string& error) {
  const std::vector<std::string> w = words_of(line);
  if (w.empty()) return false;
  out = AidOrder{};
  out.kind = aid_find(kinds, w[0]);
  if (out.kind == nullptr) return false;
  if (w.size() < 2) return true;
  out.who = w[1];
  if (w.size() > 3) {
    error = "Type \"" + w[0] + " <player> on\" or \"" + w[0] + " <player> off\".";
    return true;
  }
  aid_value(*out.kind, w.size() == 3 ? std::string_view(w[2]) : std::string_view("on"), out.value,
            error);
  return true;
}

std::string aid_vote_key(const AidKind& kind, int userid) {
  return std::string(kind.name) + ":" + std::to_string(userid);
}

bool aid_vote_split(const AidKind& kind, std::string_view text, std::string& who,
                    std::string& value, std::string& error) {
  const size_t space = text.find_last_of(' ');
  if (space == std::string_view::npos) {
    error = "Type \"vote " + std::string(kind.name) + " <player> on\" or \"vote " +
            std::string(kind.name) + " <player> off\".";
    return false;
  }
  const size_t end = text.find_last_not_of(' ', space);
  who = std::string(text.substr(0, end == std::string_view::npos ? 0 : end + 1));
  return aid_value(kind, text.substr(space + 1), value, error);
}

std::string aid_vote_word(const AidKind& kind, std::string_view value) {
  if (value.empty()) return "off";
  if (value == kind.on) return "on";
  return std::string(value);
}

std::string aids_column(const UserRow& row) {
  if (row.aids.empty()) return "-";
  std::string t;
  for (const AidSeat& a : row.aids) {
    if (!t.empty()) t += ", ";
    t += a.kind->words;
    std::string how = a.allowed.empty() ? "not allowed" : a.allowed;
    how += a.used.empty() ? ", not used" : ", " + a.used;
    t += " (" + how + ")";
  }
  return t;
}

std::string users_text(std::span<const UserRow> rows) {
  std::string t = "userid frags name             aids\n------ ----- ----             ----\n";
  for (const UserRow& r : rows) {
    std::string name = r.name;
    if (r.spectator) name += " (spec)";
    if (r.bot) name += " (bot)";
    if (name.size() < 16) name.append(16 - name.size(), ' ');
    t += pad_left(std::to_string(r.userid), 6) + " " + pad_left(std::to_string(r.frags), 5) + " " +
         name + " " + aids_column(r) + "\n";
  }
  return t + std::to_string(rows.size()) + " total users\n";
}

std::string aids_join_line(std::span<const UserRow> rows,
                           std::string (*detail)(const AidKind& kind, const void* ctx),
                           const void* ctx) {
  std::string t;
  for (const UserRow& r : rows) {
    std::string mine;
    for (const AidSeat& a : r.aids) {
      if (!a.granted) continue;
      if (!mine.empty()) mine += " and ";
      mine += a.kind->words;
      const std::string more = detail != nullptr ? detail(*a.kind, ctx) : std::string();
      if (!more.empty()) mine += " (" + more + ")";
    }
    if (mine.empty()) continue;
    t += (t.empty() ? "" : "; ") + r.name + " has " + mine;
  }
  if (t.empty()) return t;
  return "Player aids on this server: " + t + ".\n";
}

}  // namespace ge::server
