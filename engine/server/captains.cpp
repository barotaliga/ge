// The two teams and their captains, defined: the rules in captains.hpp's header.
//
// ge:summary The teams' captains, locks, names, invites, picks and removes, and the shuffle
// ge:keywords captain lockteam teamname invite accept pick kickplayer shuffle teams_command
// ge:skill net
// ge:test tests/server_captains_test.cpp
#include "server/captains.hpp"

#include <algorithm>
#include <cctype>

#include "core/prng.hpp"

namespace ge::server {

namespace {

constexpr size_t kNameMost = 15;

size_t index_of(Team team) { return team == Team::blue ? 1 : 0; }

bool seated(const Lobby& l, int slot) {
  return slot >= 0 && slot < kLobbySeats && l.seats[static_cast<size_t>(slot)].used;
}

Team team_of(const Lobby& l, int slot) {
  return seated(l, slot) ? l.seats[static_cast<size_t>(slot)].team : Team::none;
}

int team_size(const Lobby& l, Team team) {
  int n = 0;
  for (const LobbySeat& s : l.seats)
    if (s.used && s.team == team) n++;
  return n;
}

const std::string& seat_name(const Lobby& l, int slot) {
  return l.seats[static_cast<size_t>(slot)].name;
}

void say(Teams& t, int to, std::string line) {
  t.said.push_back(std::move(line));
  t.to.push_back(to);
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

std::string quoted(const Teams& t, Team team) { return "'" + teams_name(t, team) + "'"; }

// A player named by a word: a slot number, an exact name in any case, or the only name it begins.
int find_player(const TeamsCaller& who, std::string_view word) {
  if (who.names == nullptr || word.empty()) return -1;
  const auto& names = *who.names;
  if (std::all_of(word.begin(), word.end(), [](char c) { return c >= '0' && c <= '9'; })) {
    const int n = std::stoi(std::string(word));
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

const std::string& name_of(const TeamsCaller& who, int slot) {
  static const std::string none;
  return who.names != nullptr ? (*who.names)[static_cast<size_t>(slot)] : none;
}

// The captain of `team` now `slot` (-1: none), said to everyone when it is a new one.
void set_captain(Teams& t, const Lobby& l, Team team, int slot) {
  TeamSide& side = teams_side(t, team);
  if (side.captain == slot) return;
  side.captain = slot;
  if (slot >= 0) say(t, -1, seat_name(l, slot) + " became captain of " + quoted(t, team));
}

// The team the caller acts on: his own, or for an administrator the one his first word names
// (the word taken off `rest`). Team::none when there is none, said why.
Team acted_team(Teams& t, const Lobby& l, const TeamsCaller& who, std::string_view& rest,
                bool may_name) {
  if (who.admin && may_name) {
    const size_t sp = rest.find(' ');
    const Team named = teams_named(t, rest.substr(0, sp));
    if (named != Team::none) {
      rest = sp == std::string_view::npos ? std::string_view{} : trim(rest.substr(sp + 1));
      return named;
    }
  }
  const Team own = team_of(l, who.slot);
  if (own == Team::none) {
    say(t, who.slot,
        who.admin ? "Name the team first: red or blue, or its name."
                  : "You are not on a team. Type team red or team blue to join one.");
    return Team::none;
  }
  if (!who.admin && teams_side(t, own).captain != who.slot) {
    say(t, who.slot, "Only the captain of " + quoted(t, own) + " can do that.");
    return Team::none;
  }
  return own;
}

void join_word(Teams& t, Lobby& l, const TeamsCaller& who, std::string_view rest, double now) {
  if (rest.empty()) {
    const Team own = team_of(l, who.slot);
    say(t, who.slot,
        own == Team::none ? "You are not on a team. Type team red or team blue to join one."
                          : "You are on the " + quoted(t, own) + " team.");
    return;
  }
  Team want = Team::none;
  if (rest == "auto") {
    want = lobby_smaller_team(l, who.slot);
  } else if (rest == "switch") {
    want = team_of(l, who.slot) == Team::red ? Team::blue : Team::red;
  } else {
    want = teams_named(t, rest);
    if (want == Team::none) {
      say(t, who.slot,
          "There is no team " + std::string(rest) + ". The teams are " + quoted(t, Team::red) +
              " and " + quoted(t, Team::blue) + ".");
      return;
    }
  }
  if (seated(l, who.slot) && team_of(l, who.slot) != want && l.phase == LobbyPhase::warmup) {
    if (teams_side(t, want).locked && !who.admin) {
      say(t, who.slot, "Team " + quoted(t, want) + " is locked. Ask its captain for an invite.");
      return;
    }
    if (t.settings.max_per_team > 0 && team_size(l, want) >= t.settings.max_per_team) {
      say(t, who.slot, "Team " + quoted(t, want) + " is full.");
      return;
    }
  }
  const Team was = team_of(l, who.slot);
  if (lobby_team(l, who.slot, want, now) != LobbyWhy::ok) return;  // the lobby said why
  // The lobby said it its way; the teams' names are said here.
  if (!l.said.empty() && !l.notes.empty() && l.notes.back().kind == LobbyNoteKind::moved) {
    l.said.pop_back();
    l.notes.pop_back();
  }
  if (was != Team::none) say(t, -1, seat_name(l, who.slot) + " left team " + quoted(t, was));
  say(t, -1, seat_name(l, who.slot) + " joined team " + quoted(t, want));
  teams_tick(t, l);
}

void captain_word(Teams& t, Lobby& l, const TeamsCaller& who, std::string_view rest) {
  if (rest.empty()) {
    const Team own = team_of(l, who.slot);
    if (own == Team::none) {
      say(t, who.slot, "You are not on a team. Type team red or team blue to join one.");
      return;
    }
    TeamSide& side = teams_side(t, own);
    if (side.captain == who.slot) {
      say(t, who.slot, "You are the captain of team " + quoted(t, own) + ".");
    } else if (side.captain >= 0) {
      say(t, who.slot,
          seat_name(l, side.captain) + " is the captain of team " + quoted(t, own) + ".");
    } else {
      set_captain(t, l, own, who.slot);
    }
    return;
  }
  const Team team = acted_team(t, l, who, rest, true);
  if (team == Team::none) return;
  const int to = find_player(who, rest);
  if (to < 0 || team_of(l, to) != team) {
    say(t, who.slot, "Name a player on team " + quoted(t, team) + ".");
    return;
  }
  if (teams_side(t, team).captain == to) return;
  const int was = teams_side(t, team).captain;
  set_captain(t, l, team, to);
  if (was == who.slot) say(t, to, seat_name(l, who.slot) + " made you the captain.");
}

void lock_word(Teams& t, Lobby& l, const TeamsCaller& who, std::string_view rest, bool lock) {
  const Team team = acted_team(t, l, who, rest, true);
  if (team == Team::none) return;
  if (lock && team_size(l, team) == 0) {
    say(t, who.slot, "An empty team cannot be locked.");
    return;
  }
  teams_side(t, team).locked = lock;
  say(t, who.slot, "Team " + quoted(t, team) + (lock ? " is locked." : " is unlocked."));
}

void name_word(Teams& t, Lobby& l, const TeamsCaller& who, std::string_view rest) {
  if (t.settings.names_locked && !who.admin) {
    say(t, who.slot, "The team names are fixed on this server.");
    return;
  }
  const Team team = acted_team(t, l, who, rest, true);
  if (team == Team::none) return;
  if (l.phase != LobbyPhase::warmup && !who.admin) {
    say(t, who.slot, "A team is renamed only during the warmup.");
    return;
  }
  const bool bad = rest.empty() || std::any_of(rest.begin(), rest.end(), [](char c) {
                     return c == '"' || static_cast<unsigned char>(c) < 32 ||
                            static_cast<unsigned char>(c) > 126;
                   });
  if (bad) {
    say(t, who.slot, "A team's name needs letters, without quotes or colours.");
    return;
  }
  const std::string name(rest.substr(0, kNameMost));
  const std::string was = teams_name(t, team);
  t.settings.names[index_of(team)] = name;
  say(t, -1, "Team '" + was + "' renamed to '" + name + "'.");
}

void invite_word(Teams& t, Lobby& l, const TeamsCaller& who, std::string_view rest, bool pick) {
  const Team team = acted_team(t, l, who, rest, true);
  if (team == Team::none) return;
  const int to = find_player(who, rest);
  if (to < 0) {
    say(t, who.slot, "There is no player " + std::string(rest) + ".");
    return;
  }
  if (to == who.slot) {
    say(t, who.slot, "You cannot invite yourself.");
    return;
  }
  if (seated(l, to)) {
    say(t, who.slot, name_of(who, to) + " is already on a team.");
    return;
  }
  if (pick && t.settings.allow_pick) {
    t.moves.push_back({to, team});
    say(t, -1,
        name_of(who, who.slot) + " picked " + name_of(who, to) + " for team " + quoted(t, team) +
            ".");
    return;
  }
  t.invited[static_cast<size_t>(to)] = static_cast<int8_t>(team);
  say(t, to,
      "You are invited to " + quoted(t, team) + " by " + name_of(who, who.slot) +
          ". Type accept to join it.");
  say(t, who.slot, name_of(who, to) + " was invited to join your team.");
}

void accept_word(Teams& t, const Lobby& l, const TeamsCaller& who) {
  int8_t& invited = t.invited[static_cast<size_t>(who.slot)];
  const Team team = static_cast<Team>(invited);
  if (team == Team::none) {
    say(t, who.slot, "You have no invite to accept.");
    return;
  }
  invited = 0;
  if (seated(l, who.slot)) {
    say(t, who.slot, "You are already on a team.");
    return;
  }
  t.moves.push_back({who.slot, team});
}

void remove_word(Teams& t, Lobby& l, const TeamsCaller& who, std::string_view rest) {
  const Team team = acted_team(t, l, who, rest, true);
  if (team == Team::none) return;
  const int out = find_player(who, rest);
  if (out < 0 || team_of(l, out) != team) {
    say(t, who.slot, "Name a player on team " + quoted(t, team) + ".");
    return;
  }
  if (out == who.slot) {
    say(t, who.slot, "To leave the team yourself, spectate.");
    return;
  }
  t.moves.push_back({out, Team::none});
  say(t, out,
      "You were removed from team " + quoted(t, team) + " by " + name_of(who, who.slot) + ".");
}

}  // namespace

Team teams_named(const Teams& t, std::string_view word) {
  const std::string w = lower(trim(word));
  if (w == "1" || w == "a" || w == "red" || w == "r") return Team::red;
  if (w == "2" || w == "b" || w == "blue") return Team::blue;
  if (!w.empty() && w == lower(t.settings.names[0])) return Team::red;
  if (!w.empty() && w == lower(t.settings.names[1])) return Team::blue;
  return Team::none;
}

const std::string& teams_name(const Teams& t, Team team) {
  return t.settings.names[index_of(team)];
}

TeamSide& teams_side(Teams& t, Team team) { return t.side[index_of(team)]; }
const TeamSide& teams_side(const Teams& t, Team team) { return t.side[index_of(team)]; }

void teams_tick(Teams& t, const Lobby& l) {
  for (const Team team : {Team::red, Team::blue}) {
    TeamSide& side = teams_side(t, team);
    if (side.captain >= 0 && team_of(l, side.captain) != team) side.captain = -1;
    if (team_size(l, team) == 0) {
      side.locked = false;
      continue;
    }
    if (side.captain < 0)
      for (int i = 0; i < kLobbySeats; i++)
        if (team_of(l, i) == team) {
          set_captain(t, l, team, i);
          break;
        }
  }
  for (int i = 0; i < kLobbySeats; i++)  // an invite ends once its player is seated
    if (seated(l, i)) t.invited[static_cast<size_t>(i)] = 0;
}

void teams_seat(Teams& t, Lobby& l, int slot, Team team) {
  if (!seated(l, slot) || team == Team::none) return;
  LobbySeat& s = l.seats[static_cast<size_t>(slot)];
  s.team = team;
  say(t, -1, s.name + " joined team " + quoted(t, team));
  teams_tick(t, l);
}

void teams_match_began(Teams& t) { t.invited.fill(0); }

void teams_match_ended(Teams& t) {
  t.invited.fill(0);
  for (TeamSide& side : t.side) side.locked = false;
}

void teams_shuffle(Teams& t, Lobby& l, uint64_t seed) {
  std::vector<int> order;
  for (int i = 0; i < kLobbySeats; i++)
    if (seated(l, i)) order.push_back(i);
  ge::core::Rng rng = ge::core::make_rng(static_cast<uint32_t>(seed ^ (seed >> 32)));
  for (size_t i = order.size(); i > 1; i--)
    std::swap(order[i - 1], order[ge::core::pick_index(rng, i)]);
  for (TeamSide& side : t.side) side = TeamSide{};
  for (size_t k = 0; k < order.size(); k++) {
    LobbySeat& s = l.seats[static_cast<size_t>(order[k])];
    s.team = k % 2 == 0 ? Team::red : Team::blue;
    if (!s.bot) s.ready = false;
    teams_side(t, s.team).captain = order[k];  // the last dealt to each stays its captain
  }
  say(t, -1, "The teams are shuffled. Everyone, ready up again.");
  for (const Team team : {Team::red, Team::blue}) {
    const int c = teams_side(t, team).captain;
    if (c >= 0) say(t, -1, seat_name(l, c) + " became captain of " + quoted(t, team));
  }
}

bool teams_command(Teams& t, Lobby& l, const TeamsCaller& who, std::string_view line, double now) {
  line = trim(line);
  const size_t sp = line.find(' ');
  const std::string word = lower(line.substr(0, sp));
  const std::string_view rest =
      sp == std::string_view::npos ? std::string_view{} : trim(line.substr(sp + 1));
  if (who.slot < 0 || who.slot >= kLobbySeats) return false;
  if (word == "team" || word == "join") {
    join_word(t, l, who, rest, now);
  } else if (word == "captain") {
    captain_word(t, l, who, rest);
  } else if (word == "captains") {
    for (const Team team : {Team::red, Team::blue}) {
      const int c = teams_side(t, team).captain;
      say(t, who.slot,
          "Team " + quoted(t, team) + " captain: " + (c >= 0 ? seat_name(l, c) : "(none)"));
    }
  } else if (word == "lockteam" || word == "lock") {
    lock_word(t, l, who, rest, true);
  } else if (word == "unlockteam" || word == "unlock") {
    lock_word(t, l, who, rest, false);
  } else if (word == "teamname") {
    name_word(t, l, who, rest);
  } else if (word == "invite") {
    invite_word(t, l, who, rest, false);
  } else if (word == "pickplayer" || word == "pick") {
    invite_word(t, l, who, rest, true);
  } else if (word == "accept") {
    accept_word(t, l, who);
  } else if (word == "kickplayer" || word == "removeplayer" || word == "remove") {
    remove_word(t, l, who, rest);
  } else {
    return false;
  }
  return true;
}

}  // namespace ge::server
