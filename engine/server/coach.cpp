// A team's coach, defined: the rules in coach.hpp's header.
//
// ge:summary A team's coach: the election, the yes, the steps down, the camera kept on his team
// ge:keywords coach uncoach coaches coach-election coach_command coach_tick coach_next_follow
// ge:skill net
// ge:test tests/server_coach_test.cpp
#include "server/coach.hpp"

#include <algorithm>
#include <cctype>

namespace ge::server {

namespace {

size_t index_of(Team team) { return team == Team::blue ? 1 : 0; }

void say(Coaches& c, int to, std::string line) {
  c.said.push_back(std::move(line));
  c.to.push_back(to);
}

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& ch : out) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return out;
}

std::string_view trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\n' || s.back() == '\t'))
    s.remove_suffix(1);
  return s;
}

bool seated(const Lobby& l, int slot) {
  return slot >= 0 && slot < kLobbySeats && l.seats[static_cast<size_t>(slot)].used;
}

const std::string& name_at(const std::array<std::string, kLobbySeats>* names, int slot) {
  static const std::string none;
  return names != nullptr && slot >= 0 && slot < kLobbySeats ? (*names)[static_cast<size_t>(slot)]
                                                             : none;
}

std::string quoted(const Teams& t, Team team) { return "'" + teams_name(t, team) + "'"; }

// A player named by a word: a slot number, an exact name in any case, or the only name it begins.
int find_slot(const std::array<std::string, kLobbySeats>* names, std::string_view word) {
  if (names == nullptr || word.empty()) return -1;
  if (std::all_of(word.begin(), word.end(), [](char ch) { return ch >= '0' && ch <= '9'; })) {
    const int n = std::stoi(std::string(word));
    return n >= 0 && n < kLobbySeats && !(*names)[static_cast<size_t>(n)].empty() ? n : -1;
  }
  const std::string w = lower(word);
  int prefix = -1, prefixes = 0;
  for (int i = 0; i < kLobbySeats; i++) {
    const std::string n = lower((*names)[static_cast<size_t>(i)]);
    if (n.empty()) continue;
    if (n == w) return i;
    if (n.starts_with(w)) {
      prefix = i;
      prefixes++;
    }
  }
  return prefixes == 1 ? prefix : -1;
}

void close_election(Coaches& c, double now, bool passed) {
  if (!passed && c.candidate >= 0)
    c.wait_until[static_cast<size_t>(c.candidate)] = now + c.settings.wait_s;
  c.candidate = -1;
  c.candidate_team = Team::none;
  c.yes = 0;
}

void stop(Coaches& c, Team team, const std::string& line) {
  c.coach[index_of(team)] = -1;
  say(c, -1, line);
}

// The election passes when its yes reach the share.
void weigh(Coaches& c, const Lobby& l, const Teams& t,
           const std::array<std::string, kLobbySeats>* names, double now) {
  if (c.candidate < 0) return;
  int yes = 0;
  for (int i = 0; i < kLobbySeats; i++)
    if (((c.yes >> i) & 1u) != 0 && seated(l, i)) yes++;
  if (yes < coach_needed(c, l)) return;
  const int who = c.candidate;
  const Team team = c.candidate_team;
  close_election(c, now, true);
  c.coach[index_of(team)] = who;
  say(c, -1, name_at(names, who) + " now coaches " + quoted(t, team) + ".");
}

bool phase_open(const Lobby& l) { return l.phase == LobbyPhase::warmup; }

bool ask(Coaches& c, const Lobby& l, const Teams& t, const CoachCaller& who, std::string_view arg,
         bool teams_on, double now) {
  const int me = who.slot;
  const std::string& name = name_at(who.names, me);
  if (!teams_on) {
    say(c, me, "Coaches are for team matches only.");
    return true;
  }
  // An administrator: `coach <team> <player>` names one at once.
  const size_t sp = arg.find(' ');
  const Team team = teams_named(t, arg.substr(0, sp));
  if (who.admin && team != Team::none && sp != std::string_view::npos) {
    const int pick = find_slot(who.names, trim(arg.substr(sp + 1)));
    if (pick < 0 || seated(l, pick)) {
      say(c, me, "Name a spectator to coach " + quoted(t, team) + ".");
      return true;
    }
    if (c.coach[index_of(Team::red)] == pick) c.coach[index_of(Team::red)] = -1;
    if (c.coach[index_of(Team::blue)] == pick) c.coach[index_of(Team::blue)] = -1;
    if (c.candidate == pick) close_election(c, now, true);
    c.coach[index_of(team)] = pick;
    say(c, -1, name_at(who.names, pick) + " now coaches " + quoted(t, team) + ".");
    return true;
  }
  if (!who.spectator) {
    say(c, me, "Only a spectator can coach a team.");
    return true;
  }
  if (team == Team::none) {
    say(c, me,
        "Say which team you want to coach: coach " + teams_name(t, Team::red) + " or coach " +
            teams_name(t, Team::blue) + ".");
    return true;
  }
  if (!phase_open(l)) {
    say(c, me, "A coach is chosen in the warmup, before the match starts.");
    return true;
  }
  const int has = c.coach[index_of(team)];
  if (has >= 0) {
    say(c, me, quoted(t, team) + " already has a coach, " + name_at(who.names, has) + ".");
    return true;
  }
  if (coach_voters(l) < c.settings.min_players) {
    say(c, me,
        "A coach needs at least " + std::to_string(c.settings.min_players) +
            " players on the server to elect him.");
    return true;
  }
  if (c.candidate >= 0 || who.vote_open) {
    say(c, me, "Another vote is under way. Wait until it ends.");
    return true;
  }
  const double wait = c.wait_until[static_cast<size_t>(me)] - now;
  if (wait > 0) {
    const int whole = static_cast<int>(wait);
    const int s = whole + (static_cast<double>(whole) < wait ? 1 : 0);
    say(c, me, "Wait " + std::to_string(s) + (s == 1 ? " second" : " seconds") + ".");
    return true;
  }
  c.candidate = me;
  c.candidate_team = team;
  c.yes = 0;
  c.opened = now;
  say(c, -1,
      name + " asks to coach " + quoted(t, team) + ". Players, type yes to agree (" +
          std::to_string(coach_needed(c, l)) + " needed).");
  say(c, me, "Type coach again to withdraw.");
  return true;
}

}  // namespace

Team coach_team(const Coaches& c, int slot) {
  if (slot < 0) return Team::none;
  if (c.coach[0] == slot) return Team::red;
  if (c.coach[1] == slot) return Team::blue;
  return Team::none;
}

int coach_voters(const Lobby& l) {
  int n = 0;
  for (const LobbySeat& s : l.seats)
    if (s.used && !s.bot) n++;
  return n;
}

int coach_needed(const Coaches& c, const Lobby& l) {
  const int share = std::clamp(c.settings.share, 51, 100);
  const int n = (coach_voters(l) * share + 99) / 100;
  return std::max(2, n);
}

void coach_tick(Coaches& c, const Lobby& l, const Teams& t,
                const std::array<std::string, kLobbySeats>& names, bool teams_on, double now) {
  for (Team team : {Team::red, Team::blue}) {
    const int s = c.coach[index_of(team)];
    if (s < 0) continue;
    if (!teams_on) {
      c.coach[index_of(team)] = -1;
    } else if (names[static_cast<size_t>(s)].empty()) {
      stop(c, team, "The coach of " + quoted(t, team) + " left.");
    } else if (seated(l, s)) {
      stop(c, team, names[static_cast<size_t>(s)] + " plays now and no longer coaches.");
    }
  }
  if (c.candidate < 0) return;
  const int s = c.candidate;
  if (!teams_on || names[static_cast<size_t>(s)].empty() || seated(l, s)) {
    say(c, -1, "The coach election ended: its candidate is gone.");
    close_election(c, now, true);  // gone: no wait to keep
  } else if (now - c.opened >= c.settings.election_s) {
    say(c, -1, "The coach election for " + names[static_cast<size_t>(s)] + " timed out.");
    close_election(c, now, false);
  }
}

void coach_match_began(Coaches& c, const Teams& t) {
  (void)t;
  if (c.candidate < 0) return;
  say(c, -1, "The coach election ended: the match has started.");
  close_election(c, 0, true);
}

bool coach_command(Coaches& c, const Lobby& l, const Teams& t, const CoachCaller& who,
                   std::string_view line, bool teams_on, double now) {
  line = trim(line);
  const size_t sp = line.find(' ');
  const std::string word = lower(line.substr(0, sp));
  const std::string_view arg =
      sp == std::string_view::npos ? std::string_view{} : trim(line.substr(sp + 1));
  const int me = who.slot;
  if (me < 0 || me >= kLobbySeats) return false;
  const std::string& name = name_at(who.names, me);
  if (word == "coaches") {
    for (Team team : {Team::red, Team::blue}) {
      const int s = c.coach[index_of(team)];
      say(c, me,
          quoted(t, team) + ": " + (s >= 0 ? name_at(who.names, s) : std::string("no coach")));
    }
    if (c.candidate >= 0)
      say(c, me,
          name_at(who.names, c.candidate) + " asks to coach " + quoted(t, c.candidate_team) + ".");
    return true;
  }
  if (word == "uncoach") {
    if (!who.admin) {
      say(c, me, "Only the referee can take a coach off. Ask the coach to type coach.");
      return true;
    }
    const Team team = teams_named(t, arg);
    const int s = team != Team::none ? c.coach[index_of(team)] : -1;
    if (s < 0) {
      say(c, me, "Name a team that has a coach: uncoach red or uncoach blue.");
      return true;
    }
    stop(c, team, name_at(who.names, s) + " no longer coaches " + quoted(t, team) + ".");
    return true;
  }
  if (word == "coach") {
    const Team mine = coach_team(c, me);
    if (mine != Team::none && arg.empty()) {
      stop(c, mine, name + " no longer coaches " + quoted(t, mine) + ".");
      return true;
    }
    if (c.candidate == me && arg.empty()) {
      say(c, -1, name + " withdrew from coaching " + quoted(t, c.candidate_team) + ".");
      close_election(c, now, false);
      return true;
    }
    return ask(c, l, t, who, arg, teams_on, now);
  }
  if (c.candidate < 0 || (word != "yes" && word != "no") || !arg.empty()) return false;
  if (!seated(l, me)) {
    say(c, me, "Only the players vote for a coach.");
    return true;
  }
  const uint32_t bit = 1u << me;
  if (word == "no") {
    if ((c.yes & bit) != 0) {
      c.yes &= ~bit;
      say(c, -1, name + " takes back the yes.");
    }
    return true;
  }
  if ((c.yes & bit) != 0) {
    say(c, me, "Your yes is already counted.");
    return true;
  }
  c.yes |= bit;
  int yes = 0;
  for (int i = 0; i < kLobbySeats; i++)
    if (((c.yes >> i) & 1u) != 0 && seated(l, i)) yes++;
  say(c, -1,
      name + " agrees (" + std::to_string(yes) + " of " + std::to_string(coach_needed(c, l)) +
          ").");
  weigh(c, l, t, who.names, now);
  return true;
}

bool coach_may_follow(const Coaches& c, const Lobby& l, int slot, int target) {
  const Team team = coach_team(c, slot);
  if (team == Team::none) return true;
  return seated(l, target) && l.seats[static_cast<size_t>(target)].team == team;
}

int coach_next_follow(const Coaches& c, const Lobby& l, int slot, int from) {
  const Team team = coach_team(c, slot);
  if (team == Team::none) return from;
  const int start = from >= 0 && from < kLobbySeats ? from : kLobbySeats - 1;
  for (int k = 1; k <= kLobbySeats; k++) {
    const int i = (start + k) % kLobbySeats;
    if (seated(l, i) && l.seats[static_cast<size_t>(i)].team == team) return i;
  }
  return -1;
}

}  // namespace ge::server
