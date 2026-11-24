// The match's referee, defined: the rules in referee.hpp's header.
//
// ge:summary The referee's password, words and acts: end, time limit, map, kick, ban, mute
// ge:keywords referee admin acommands endmatch changemap kickban mute shuffle referee_command
// ge:skill net
// ge:test tests/server_referee_test.cpp
#include "server/referee.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

#include "core/num_text.hpp"

namespace ge::server {

namespace {

constexpr double kBanDefault = 60, kBanMost = 720, kMuteDefault = 5, kMuteMost = 60;

void say(Referee& r, int to, std::string line) {
  r.said.push_back(std::move(line));
  r.to.push_back(to);
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

const std::string& name_of(const RefereeCaller& who, int slot) {
  static const std::string none;
  return who.names != nullptr && slot >= 0 ? (*who.names)[static_cast<size_t>(slot)] : none;
}

int find_player(const RefereeCaller& who, std::string_view word) {
  if (who.names == nullptr || word.empty()) return -1;
  const auto& names = *who.names;
  if (digits(word)) {
    const int n = std::atoi(std::string(word).c_str());
    return n >= 0 && n < kLobbySeats && !names[static_cast<size_t>(n)].empty() ? n : -1;
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
    say(r, who.slot, "There is no player " + std::string(name) + ". Name one by number or name.");
    return -1;
  }
  if (referee_is(r, slot)) {
    say(r, who.slot, "A referee cannot be kicked, banned or muted.");
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
  const auto k = static_cast<size_t>(who.slot);
  if (referee_is(r, who.slot)) {
    r.referees &= ~(1u << who.slot);
    say(r, who.slot, "You are no longer a referee.");
    return;
  }
  if (who.password.empty()) {
    say(r, who.slot, "There is no referee on this server.");
    return;
  }
  if (r.tried[k] > 0 && now - r.tried[k] < 1) {
    say(r, who.slot, "Wait a second before trying again.");
    return;
  }
  r.tried[k] = now;
  if (rest != who.password) {
    say(r, who.slot, "That is not the referee's password.");
    say(r, -2, name_of(who, who.slot) + " gave a wrong referee password.");
    return;
  }
  r.referees |= 1u << who.slot;
  say(r, -1, name_of(who, who.slot) + " became a referee.");
}

constexpr const char* kWords[] = {"endmatch", "tl",       "changemap",  "kick",
                                  "kickban",  "ban",      "mute",       "unmute",
                                  "shuffle",  "readyall", "unreadyall", "acommands"};

}  // namespace

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
  line = trim(line);
  const size_t sp = line.find(' ');
  const std::string word = lower(line.substr(0, sp));
  const std::string_view rest =
      sp == std::string_view::npos ? std::string_view{} : trim(line.substr(sp + 1));
  if (word == "admin" || word == "referee") {
    login(r, who, rest, now);
    return true;
  }
  const bool mine = std::find(std::begin(kWords), std::end(kWords), word) != std::end(kWords) ||
                    (word == "break" && who.in_match);
  if (!mine) return false;
  if (!referee_is(r, who.slot)) {
    if (word == "break") return false;  // the lobby's own word for not ready
    say(r, who.slot, "Only a referee can do that. Type referee and the password to become one.");
    return true;
  }
  const std::string& me = name_of(who, who.slot);
  if (word == "acommands") {
    say(r, who.slot,
        "Referee: endmatch, tl <minutes>, changemap <map>, kick, kickban, ban <player> "
        "[minutes], mute <player> [minutes], unmute, shuffle, readyall, unreadyall.");
  } else if (word == "endmatch" || word == "break") {
    if (!who.in_match) {
      say(r, who.slot, "There is no match to end.");
      return true;
    }
    say(r, -1, me + " has ended the match.");
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
    if (rest.empty() || rest.find_first_of("\"; ") != std::string_view::npos) {
      say(r, who.slot, "Name the map: changemap q2dm1.");
      return true;
    }
    say(r, -1, "New map: " + std::string(rest));
    r.acts.push_back(act(RefereeActKind::change_map, -1, 0, std::string(rest)));
  } else if (word == "kick") {
    double unused = 0;
    const int slot = player_and_minutes(r, who, rest, 0, 0, unused);
    if (slot < 0) return true;
    say(r, -1, name_of(who, slot) + " was kicked by " + me + ".");
    r.acts.push_back(act(RefereeActKind::kick, slot));
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
