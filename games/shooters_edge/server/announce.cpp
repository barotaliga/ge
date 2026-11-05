// The lobby's words on the wire, defined: the rules in announce.hpp's header.
//
// ge:summary The lobby's lines and the countdown's centre print as server events, in a game's words
// ge:keywords announce announce_flush match_look countdown centerprint lobby-words
// ge:skill shooter net
// ge:test tests/shooters_edge_words_test.cpp
#include "shooters_edge/server/announce.hpp"

#include <cstdlib>
#include <string_view>

#include "shooters_edge/server/words.hpp"

namespace shooters_edge::server {

namespace {

using ge::server::LobbyNote;
using ge::server::LobbyNoteKind;

constexpr uint32_t kEveryone = 0xffffffffu;
constexpr int kHigh = 2;  // the print level the lobby speaks at

uint32_t one(int slot) { return slot >= 0 && slot < 32 ? 1u << slot : 0u; }

void print(Announcer& a, uint32_t to, std::string text) {
  if (text.empty() || to == 0) return;
  NetEvent ev;
  ev.kind = EventKind::print;
  ev.to = to;
  ev.reliable = true;
  ev.level = kHigh;
  ev.text = std::move(text);
  a.pending.push_back(std::move(ev));
}

void centre(Announcer& a, std::string text) {
  NetEvent ev;
  ev.kind = EventKind::centerprint;
  ev.to = kEveryone;
  ev.reliable = true;
  ev.text = std::move(text);
  a.pending.push_back(std::move(ev));
}

// A sample everyone hears as it is, wherever they are (no attenuation).
void sound(Announcer& a, std::string_view sample) {
  if (sample.empty()) return;
  NetEvent ev;
  ev.kind = EventKind::sound;
  ev.to = kEveryone;
  ev.reliable = true;
  ev.values[0] = 1;  // volume
  ev.values[1] = 0;  // attenuation: none
  ev.text = std::string(sample);
  a.pending.push_back(std::move(ev));
}

std::string words(const Info& info, std::string_view key, std::vector<std::string> args = {}) {
  return say_words(info, key, args);
}

const std::string& name_of(const Lobby& l, int slot) {
  static const std::string kNone;
  return slot >= 0 && slot < kLobbySeats ? l.seats[static_cast<size_t>(slot)].name : kNone;
}

// The countdown's centre print for `seconds` left.
std::string count_text(const Info& info, const Lobby& l, const MatchLook& m, int seconds) {
  const std::string mode_key = m.teamplay != 0        ? "mode_team"
                               : lobby_seated(l) == 2 ? "mode_duel"
                                                      : "mode_ffa";
  std::string t = words(info, "count_head", {std::to_string(seconds)});
  t += words(info, "count_dm", {std::to_string(m.deathmatch)});
  t += words(info, "count_mode", {std::string(words_of(info, mode_key))});
  if (m.antilag != 0) t += words(info, "count_antilag", {std::to_string(m.antilag)});
  if (m.teamplay != 0) t += words(info, "count_teamplay", {std::to_string(m.teamplay)});
  if (m.timelimit != 0) t += words(info, "count_timelimit", {std::to_string(m.timelimit)});
  if (m.fraglimit != 0) t += words(info, "count_fraglimit", {std::to_string(m.fraglimit)});
  if (m.powerups) t += words(info, "count_powerups", {"on"});
  t += words(info, "count_tail");
  return t;
}

// One of the lobby's lines in the game's words (`next` the note after it, or null).
void classic_line(Announcer& a, const Lobby& l, const Info& info, const MatchLook& m,
                  const LobbyNote& n, const LobbyNote* next, const std::string& line) {
  const std::string& who = name_of(l, n.slot);
  switch (n.kind) {
    case LobbyNoteKind::ready:
      if (m.teamplay != 0)
        print(
            a, kEveryone,
            words(info, "ready_team", {who, team_name(l.seats[static_cast<size_t>(n.slot)].team)}));
      else
        print(a, kEveryone, words(info, "ready", {who}));
      break;
    case LobbyNoteKind::unready:
      // A break that stopped the countdown says so instead (the next note).
      if (next == nullptr || next->kind != LobbyNoteKind::stopped || next->slot != n.slot)
        print(a, kEveryone, words(info, "unready", {who}));
      break;
    case LobbyNoteKind::stopped:
      if (n.slot >= 0)
        print(a, kEveryone, words(info, "stops", {who}));
      else
        print(a, kEveryone, words(info, "aborting"));
      centre(a, "");
      a.shown = 0;
      break;
    case LobbyNoteKind::countdown:
      if (n.slot < 0) print(a, kEveryone, words(info, "all_ready"));
      print(a, kEveryone, words(info, "timer"));
      sound(a, words_of(info, "sound_timer"));
      a.shown = 0;
      break;
    case LobbyNoteKind::begins:
      centre(a, "");
      print(a, kEveryone, words(info, "begun"));
      a.shown = 0;
      break;
    case LobbyNoteKind::refused:
      if (n.why == static_cast<uint8_t>(LobbyWhy::same) && n.slot >= 0 &&
          l.seats[static_cast<size_t>(n.slot)].ready && line.rfind("ready", 0) == 0)
        print(a, one(n.slot), words(info, "ready_again"));
      else
        print(a, one(n.slot), line + "\n");
      break;
    case LobbyNoteKind::told:
      print(a, one(n.slot), line + "\n");
      break;
    case LobbyNoteKind::joined:
    case LobbyNoteKind::left:
    case LobbyNoteKind::moved:
    case LobbyNoteKind::warmup:
    case LobbyNoteKind::warm:
    case LobbyNoteKind::sat_out:
      break;
  }
}

}  // namespace

MatchLook match_look(MatchLook look, const std::vector<std::string>& lines) {
  for (const std::string& line : lines) {
    const size_t sp = line.find(' ');
    if (sp == std::string::npos) continue;
    const std::string_view key = std::string_view(line).substr(0, sp);
    std::string value = line.substr(sp + 1);
    if (!value.empty() && value.front() == '"') value.erase(0, 1);
    if (!value.empty() && value.back() == '"') value.pop_back();
    const int v = std::atoi(value.c_str());
    if (key == "deathmatch") look.deathmatch = v;
    if (key == "teamplay") look.teamplay = v;
    if (key == "timelimit") look.timelimit = v;
    if (key == "fraglimit") look.fraglimit = v;
    if (key == "sv_antilag") look.antilag = v;
  }
  return look;
}

void announce(Announcer& a, Lobby& l, const Info& info, const MatchLook& look, double now) {
  const bool classic = !words_of(info, "count_head").empty();
  const size_t n = l.said.size();
  for (size_t i = 0; i < n; i++) {
    if (!classic || i >= l.notes.size()) {
      print(a, kEveryone, l.said[i] + "\n");
      continue;
    }
    const LobbyNote* next = i + 1 < l.notes.size() ? &l.notes[i + 1] : nullptr;
    classic_line(a, l, info, look, l.notes[i], next, l.said[i]);
  }
  l.said.clear();
  l.notes.clear();
  if (!classic) return;
  if (l.phase != LobbyPhase::countdown) {
    a.shown = 0;
    return;
  }
  const int left = lobby_seconds_left(l, now);
  if (left <= 0 || left == a.shown) return;
  a.shown = left;
  centre(a, count_text(info, l, look, left));
  if (left <= 5) {
    sound(a, words_of(info, "sound_tick"));
    uint32_t unready = 0;
    for (int i = 0; i < kLobbySeats; i++) {
      const auto& s = l.seats[static_cast<size_t>(i)];
      if (s.used && !s.bot && !s.ready) unready |= one(i);
    }
    print(a, unready, words(info, "get_ready"));
  }
}

void announce_phase(Info& info, LobbyPhase phase) {
  const char* name = phase_name(phase);
  for (auto& [k, v] : info)
    if (k == "ge_phase") {
      v = name;
      return;
    }
  info.emplace_back("ge_phase", name);
}

void announce_flush(Announcer& a, MessageSink& out, double now) {
  for (NetEvent& ev : a.pending) {
    ev.time = now;
    out.events.push_back(std::move(ev));
    out.events_total++;
  }
  a.pending.clear();
}

}  // namespace shooters_edge::server
