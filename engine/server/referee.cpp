// The match's referee, defined: the rules in referee.hpp's header.
//
// ge:summary The referee's password, words and acts: end, time limit, map, kick, ban, mute
// ge:keywords referee admin acommands endmatch changemap kickban mute shuffle referee_command
// ge:keywords referee_words per-game-words fill
// ge:skill net
// ge:test tests/server_referee_test.cpp
#include "server/referee.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

#include "core/num.hpp"
#include "core/num_text.hpp"

namespace ge::server {

namespace {

constexpr double kBanDefault = 60, kBanMost = 720, kMuteDefault = 5, kMuteMost = 60;

// One line or several (a line break between): each said to `to`; an empty line not at all.
void say(Referee& r, int to, std::string_view line) {
  while (!line.empty()) {
    const size_t nl = line.find('\n');
    r.said.emplace_back(line.substr(0, nl));
    r.to.push_back(to);
    if (nl == std::string_view::npos) break;
    line.remove_prefix(nl + 1);
  }
}

// A line of a game's words with its fields filled in.
struct Fill {
  std::string_view name = {};
  std::string_view player = {};
  std::string_view map = {};
  std::string_view reason = {};
  int64_t n = 0;
};
std::string fill(std::string_view line, const Fill& f) {
  std::string out;
  for (size_t i = 0; i < line.size(); i++) {
    if (line[i] != '{') {
      out += line[i];
      continue;
    }
    const size_t close = line.find('}', i);
    const std::string_view key =
        close == std::string_view::npos ? std::string_view{} : line.substr(i + 1, close - i - 1);
    if (key == "name")
      out += f.name;
    else if (key == "player")
      out += f.player;
    else if (key == "map")
      out += f.map;
    else if (key == "reason")
      out += f.reason;
    else if (key == "n")
      out += std::to_string(f.n);
    else if (key == "seconds")
      out += f.n == 1 ? "second" : "seconds";
    else {
      out += line[i];
      continue;
    }
    i = close;
  }
  return out;
}

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

std::string_view trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\n' || s.back() == '\t'))
    s.remove_suffix(1);
  return s;
}

bool digits(std::string_view s) {
  return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
}

const RefereeWords& words_of(const RefereeCaller& who) {
  return who.words != nullptr ? *who.words : referee_words();
}

const std::string& name_of(const RefereeCaller& who, int slot) {
  static const std::string none;
  return who.names != nullptr && slot >= 0 ? (*who.names)[static_cast<size_t>(slot)] : none;
}

int find_player(const RefereeCaller& who, std::string_view word) {
  if (who.names == nullptr || word.empty()) return -1;
  const auto& names = *who.names;
  if (digits(word)) {
    const int n = std::atoi(std::string(word).c_str());
    if (who.ids != nullptr) {  // a user id first
      for (int i = 0; i < kLobbySeats; i++)
        if ((*who.ids)[static_cast<size_t>(i)] == n && !names[static_cast<size_t>(i)].empty())
          return i;
    } else {
      return n >= 0 && n < kLobbySeats && !names[static_cast<size_t>(n)].empty() ? n : -1;
    }
  }
  if (words_of(who).exact_names) {
    for (int i = 0; i < kLobbySeats; i++)
      if (!names[static_cast<size_t>(i)].empty() && names[static_cast<size_t>(i)] == word) return i;
    return -1;
  }
  const std::string w = lower(word);
  int prefix = -1, prefixes = 0;
  for (int i = 0; i < kLobbySeats; i++) {
    const std::string n = lower(names[static_cast<size_t>(i)]);
    if (n.empty()) continue;
    if (n == w) return i;
    if (n.starts_with(w)) {
      prefix = i;
      prefixes++;
    }
  }
  return prefixes == 1 ? prefix : -1;
}

// `<player> [minutes]`: the player (-1 told why) and the minutes (`fallback` when none).
int player_and_minutes(Referee& r, const RefereeCaller& who, std::string_view rest, double fallback,
                       double most, double& minutes) {
  const RefereeWords& w = words_of(who);
  const size_t sp = rest.find(' ');
  const std::string_view name = rest.substr(0, sp);
  const std::string_view more =
      sp == std::string_view::npos ? std::string_view{} : trim(rest.substr(sp + 1));
  minutes = fallback;
  if (!more.empty()) {
    if (!digits(more) || std::atoi(std::string(more).c_str()) <= 0) {
      say(r, who.slot,
          "Give the minutes as a number over 0 (" + ge::core::num::to_string(most) + " at most).");
      return -1;
    }
    minutes = std::min(most, double(std::atoi(std::string(more).c_str())));
  }
  const int slot = find_player(who, name);
  if (slot < 0) {
    say(r, who.slot, fill(w.no_player, {.player = name}));
    return -1;
  }
  if (referee_is(r, slot)) {
    say(r, who.slot,
        fill(w.shielded, {.name = name_of(who, who.slot), .player = name_of(who, slot)}));
    return -1;
  }
  return slot;
}

RefereeAct act(RefereeActKind kind, int slot = -1, double value = 0, std::string text = {}) {
  RefereeAct a;
  a.kind = kind;
  a.slot = slot;
  a.value = value;
  a.text = std::move(text);
  return a;
}

std::string minutes_words(double m) {
  const auto n = static_cast<int64_t>(m);
  return std::to_string(n) + (n == 1 ? " minute" : " minutes");
}

void login(Referee& r, const RefereeCaller& who, std::string_view rest, double now) {
  const RefereeWords& w = words_of(who);
  const auto k = static_cast<size_t>(who.slot);
  const std::string& me = name_of(who, who.slot);
  if (referee_is(r, who.slot)) {
    r.referees &= ~(1u << who.slot);
    say(r, -1, fill(w.left, {.name = me}));
    say(r, who.slot, fill(w.left_me, {.name = me}));
    return;
  }
  if (!who.allowed || (who.password.empty() && !w.no_password_is_wrong)) {
    say(r, who.slot, w.none);
    return;
  }
  if (r.tried[k] > 0 && now - r.tried[k] < w.wait_s) {
    const auto left =
        static_cast<int64_t>(std::max(1.0, ge::core::num::round(w.wait_s - (now - r.tried[k]))));
    say(r, who.slot, fill(w.wait, {.n = left}));
    return;
  }
  const bool right = !who.password.empty() && rest == who.password;
  if (!w.wait_after_wrong || !right) r.tried[k] = now;
  if (!right) {
    say(r, who.slot, fill(w.wrong, {.name = me}));
    say(r, -2, fill(w.wrong_log, {.name = me}));
    return;
  }
  r.referees |= 1u << who.slot;
  say(r, -1, fill(w.became, {.name = me}));
  say(r, who.slot, fill(w.became_me, {.name = me}));
}

// The engine's words, each the book's own.
RefereeWords engine_words() {
  RefereeWords w;
  w.login = {"admin", "referee"};
  for (const char* word : {"endmatch", "tl", "changemap", "kick", "kickban", "ban", "mute",
                           "unmute", "shuffle", "readyall", "unreadyall", "acommands"})
    w.words.emplace_back(word, word);
  w.became = "{name} became a referee.";
  w.left_me = "You are no longer a referee.";
  w.none = "There is no referee on this server.";
  w.wrong = "That is not the referee's password.";
  w.wrong_log = "{name} gave a wrong referee password.";
  w.wait = "Wait a second before trying again.";
  w.not_one = "Only a referee can do that. Type referee and the password to become one.";
  w.list =
      "Referee: endmatch, tl <minutes>, changemap <map>, kick, kickban, ban <player> "
      "[minutes], mute <player> [minutes], unmute, shuffle, readyall, unreadyall.";
  w.ended = "{name} has ended the match.";
  w.no_match = "There is no match to end.";
  w.map_usage = "Name the map: changemap q2dm1.";
  w.new_map = "New map: {map}";
  w.no_player = "There is no player {player}. Name one by number or name.";
  w.shielded = "A referee cannot be kicked, banned or muted.";
  w.kicked = "{player} was kicked by {name}.";
  return w;
}

}  // namespace

const RefereeWords& referee_words() {
  static const RefereeWords words = engine_words();
  return words;
}

bool referee_is(const Referee& r, int slot) {
  return slot >= 0 && slot < kLobbySeats && ((r.referees >> slot) & 1u) != 0;
}

void referee_left(Referee& r, int slot) {
  if (slot < 0 || slot >= kLobbySeats) return;
  r.referees &= ~(1u << slot);
  r.muted_until[static_cast<size_t>(slot)] = 0;
  r.tried[static_cast<size_t>(slot)] = 0;
}

bool referee_muted(Referee& r, int slot, double now) {
  if (slot < 0 || slot >= kLobbySeats) return false;
  const double until = r.muted_until[static_cast<size_t>(slot)];
  if (until <= now) return false;
  const double left = std::max(1.0, double(int64_t((until - now + 59) / 60)));
  say(r, slot, "You are muted for " + minutes_words(left) + " more.");
  return true;
}

bool referee_command(Referee& r, const RefereeCaller& who, std::string_view line, double now) {
  if (who.slot < 0 || who.slot >= kLobbySeats) return false;
  const RefereeWords& w = words_of(who);
  line = trim(line);
  const size_t sp = line.find(' ');
  const std::string typed = lower(line.substr(0, sp));
  const std::string_view rest =
      sp == std::string_view::npos ? std::string_view{} : trim(line.substr(sp + 1));
  if (std::find(w.login.begin(), w.login.end(), typed) != w.login.end()) {
    login(r, who, rest, now);
    return true;
  }
  std::string word;
  for (const auto& [game, book] : w.words)
    if (game == typed) word = book;
  if (word.empty() && typed == "break" && who.in_match && w.break_ends) word = "break";
  if (word.empty()) return false;
  if (!referee_is(r, who.slot)) {
    if (word == "break") return false;  // the lobby's own word for not ready
    say(r, who.slot, w.not_one);
    return true;
  }
  const std::string& me = name_of(who, who.slot);
  if (word == "acommands") {
    say(r, who.slot, w.list);
  } else if (word == "endmatch" || word == "break") {
    if (!who.in_match) {
      say(r, who.slot, w.no_match);
      return true;
    }
    say(r, -1, fill(w.ended, {.name = me}));
    r.acts.push_back(act(RefereeActKind::end_match));
  } else if (word == "tl") {
    if (!digits(rest)) {
      say(r, who.slot, "Give the time limit in whole minutes: tl 20.");
      return true;
    }
    const double m = std::atoi(std::string(rest).c_str());
    say(r, -1, "Time limit set to " + ge::core::num::to_string(m) + ".");
    r.acts.push_back(act(RefereeActKind::time_limit, -1, m));
  } else if (word == "changemap") {
    if (who.in_match && !w.map_in_match.empty()) {
      say(r, who.slot, w.map_in_match);
      return true;
    }
    if (rest.empty() || rest.find_first_of("\"; ") != std::string_view::npos) {
      say(r, who.slot, w.map_usage);
      return true;
    }
    if (who.map_known != nullptr && !who.map_known(who.map_ctx, rest)) {
      say(r, who.slot, fill(w.map_unknown, {.map = rest}));
      return true;
    }
    say(r, -1, fill(w.new_map, {.name = me, .map = rest}));
    r.acts.push_back(act(RefereeActKind::change_map, -1, 0, std::string(rest)));
  } else if (word == "kick") {
    const size_t at = rest.find(' ');
    const std::string_view reason = w.kick_reason && at != std::string_view::npos
                                        ? trim(rest.substr(at + 1))
                                        : std::string_view{};
    double unused = 0;
    const int slot =
        player_and_minutes(r, who, w.kick_reason ? rest.substr(0, at) : rest, 0, 0, unused);
    if (slot < 0) return true;
    const Fill f{.name = me, .player = name_of(who, slot), .reason = reason};
    say(r, -1, fill(w.kicked, f));
    if (!reason.empty()) say(r, -1, fill(w.kick_said_reason, f));
    say(r, slot, fill(w.kicked_you, f));
    r.acts.push_back(act(RefereeActKind::kick, slot, w.kick_ban_minutes, std::string(reason)));
  } else if (word == "kickban" || word == "ban") {
    double minutes = kBanDefault;
    const std::string_view what = word == "kickban" ? rest.substr(0, rest.find(' ')) : rest;
    const int slot = player_and_minutes(r, who, what, kBanDefault, kBanMost, minutes);
    if (slot < 0) return true;
    say(r, -1,
        name_of(who, slot) + " was banned for " + minutes_words(minutes) + " by " + me + ".");
    r.acts.push_back(act(RefereeActKind::ban, slot, minutes));
  } else if (word == "mute") {
    double minutes = kMuteDefault;
    const int slot = player_and_minutes(r, who, rest, kMuteDefault, kMuteMost, minutes);
    if (slot < 0) return true;
    r.muted_until[static_cast<size_t>(slot)] = now + minutes * 60;
    say(r, -1, name_of(who, slot) + " is muted for " + minutes_words(minutes) + ".");
  } else if (word == "unmute") {
    const int slot = find_player(who, rest);
    if (slot < 0) {
      say(r, who.slot, "There is no player " + std::string(rest) + ".");
      return true;
    }
    r.muted_until[static_cast<size_t>(slot)] = 0;
    say(r, -1, name_of(who, slot) + " may speak again.");
  } else if (word == "shuffle") {
    r.acts.push_back(act(RefereeActKind::shuffle));
  } else if (word == "readyall" || word == "unreadyall") {
    r.acts.push_back(act(RefereeActKind::ready_all, -1, word == "readyall" ? 1.0 : 0.0));
  }
  return true;
}

}  // namespace ge::server
