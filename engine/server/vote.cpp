// Votes on a server's settings, defined: the lists, the words, the weighing and the lines said.
//
// ge:summary Votes on a server's settings, defined: lists, words, weighing, the lines said
// ge:keywords vote vote_command vote_seat vote_tick vote_take vote_map_over vote_pattern_match
// ge:skill net
// ge:test tests/server_vote_test.cpp
#include "server/vote.hpp"

#include <bit>
#include <cmath>

namespace ge::server {

const char* const kVoteDenyDefault =
    "*password* rcon* sv_vote* hostname sv_hostname sv_description sv_motd sv_admin *admin* "
    "maxclients sv_maxclients maxspectators sv_maxspectators net_* sv_rendezvous sv_visibility "
    "sv_mod gamedir sv_progs sv_seed sv_bots sv_lobby sv_timeout sv_floodprot* sv_map "
    "sv_maprotation sv_mintic sv_maxtic sv_maxrate allow_download* sv_downloadchunksperframe "
    "sv_coop* sv_episodes_done log*";

namespace {

bool valid_slot(int slot) { return slot >= 0 && slot < kVoteSeats; }
bool is_voter(const VoteBook& b, int slot) {
  return valid_slot(slot) && (b.voters & (1u << slot)) != 0;
}
std::string who(const VoteBook& b, int slot) {
  return valid_slot(slot) && !b.names[static_cast<size_t>(slot)].empty()
             ? b.names[static_cast<size_t>(slot)]
             : "slot " + std::to_string(slot);
}

void say(VoteBook& b, VoteNoteKind kind, int slot, int to, std::string line,
         VoteWhy why = VoteWhy::ok) {
  b.said.push_back(std::move(line));
  b.notes.push_back({kind, slot, to, why});
}

std::string change_text(const VoteChange& c) { return c.name + " " + c.value; }

// "2 yes, 1 no, 3 needed"
std::string tally(const VoteBook& b) {
  return std::to_string(vote_count(b, Ballot::yes)) + " yes, " +
         std::to_string(vote_count(b, Ballot::no)) + " no, " + std::to_string(vote_needed(b)) +
         " needed";
}

void close_vote(VoteBook& b) {
  b.last_yes = vote_count(b, Ballot::yes);
  b.last_no = vote_count(b, Ballot::no);
  b.last_voters = vote_voter_count(b);
  b.open = false;
  b.caller = -1;
  b.ballots.fill(Ballot::none);
}

void keep(VoteBook& b, const VoteChange& c) {
  for (VoteChange& k : b.kept)
    if (k.key == c.key) {
      k.name = c.name;
      k.value = c.value;  // the first `before` stays: the map's own
      return;
    }
  b.kept.push_back(c);
}

// The open vote weighed: passed, lost, or dropped with nobody left.
void settle(VoteBook& b, double now) {
  if (!b.open) return;
  const int n = vote_voter_count(b);
  const int yes = vote_count(b, Ballot::yes), no = vote_count(b, Ballot::no);
  if (n == 0) {
    say(b, VoteNoteKind::cancelled, -1, -1,
        "Vote dropped: " + change_text(b.asked) + ", nobody left");
    close_vote(b);
    return;
  }
  if (2 * yes > n) {
    say(b, VoteNoteKind::passed, b.caller, -1,
        "Vote passed: " + change_text(b.asked) + " for this map");
    b.due.push_back(b.asked);
    keep(b, b.asked);
    b.passed++;
    close_vote(b);
    return;
  }
  if (2 * no >= n) {
    say(b, VoteNoteKind::failed, b.caller, -1,
        "Vote failed: " + change_text(b.asked) + " (" + std::to_string(yes) + " yes, " +
            std::to_string(no) + " no)");
    if (valid_slot(b.caller)) b.failed_at[static_cast<size_t>(b.caller)] = now;
    b.failed++;
    close_vote(b);
  }
}

bool refuse(VoteBook& b, int slot, VoteWhy why, const std::string& detail = {}) {
  std::string line = "vote: ";
  line += detail.empty() ? vote_why_text(why) : detail;
  say(b, VoteNoteKind::refused, slot, slot, std::move(line), why);
  return true;
}

void tell(VoteBook& b, int slot, std::string line) {
  say(b, VoteNoteKind::told, slot, slot, std::move(line));
}

std::string_view trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
  while (!s.empty() &&
         (s.back() == ' ' || s.back() == '\t' || s.back() == '\n' || s.back() == '\r'))
    s.remove_suffix(1);
  return s;
}

std::string_view unquote(std::string_view s) {
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"') return s.substr(1, s.size() - 2);
  return s;
}

// Every word of a list (spaces or commas between) matching `name`.
bool listed(std::string_view list, std::string_view name) {
  size_t at = 0;
  while (at < list.size()) {
    while (at < list.size() && (list[at] == ' ' || list[at] == ',' || list[at] == '\t')) at++;
    size_t end = at;
    while (end < list.size() && list[end] != ' ' && list[end] != ',' && list[end] != '\t') end++;
    if (end > at && vote_pattern_match(list.substr(at, end - at), name)) return true;
    at = end;
  }
  return false;
}

bool has_words(std::string_view list) {
  for (const char c : list)
    if (c != ' ' && c != ',' && c != '\t') return true;
  return false;
}

std::string rules_text(const VoteBook& b) {
  const VoteSettings& s = b.settings;
  std::string out = "vote: ";
  if (!s.enabled) return out + "votes are off on this server (sv_vote 0)";
  out += has_words(s.allow) ? "these settings may be voted on: " + std::string(trim(s.allow))
                            : std::string("any setting may be voted on");
  if (has_words(s.deny)) out += "; never: " + std::string(trim(s.deny));
  out += "; a vote passes with more than half of the " + std::to_string(vote_voter_count(b)) +
         " players and lasts the map";
  return out;
}

void call(VoteBook& b, const VoteTarget& target, int slot, std::string_view rest, double now) {
  const size_t space = rest.find_first_of(" \t");
  const std::string_view name = rest.substr(0, space);
  const std::string_view value =
      space == std::string_view::npos ? std::string_view() : unquote(trim(rest.substr(space)));
  if (!b.settings.enabled) return (void)refuse(b, slot, VoteWhy::off);
  if (!is_voter(b, slot)) return (void)refuse(b, slot, VoteWhy::not_voter);
  if (b.open)
    return (void)refuse(b, slot, VoteWhy::busy,
                        "a vote is open (" + change_text(b.asked) + ", " +
                            std::to_string(vote_seconds_left(b, now)) +
                            " s left); next: `vote yes` or `vote no`, then call yours");
  const double failed_at = b.failed_at[static_cast<size_t>(slot)];
  if (failed_at > 0 && now - failed_at < b.settings.wait_s) {
    const int left = static_cast<int>(std::ceil(b.settings.wait_s - (now - failed_at) - 1e-9));
    return (void)refuse(b, slot, VoteWhy::waiting,
                        "your last vote failed; next: call another in " + std::to_string(left) +
                            " s (sv_vote_wait)");
  }
  if (value.empty())
    return (void)refuse(b, slot, VoteWhy::no_value,
                        "`vote " + std::string(name) + "` needs a value; next: `vote " +
                            std::string(name) + " <value>`");
  const VoteWhy listed_why = vote_permitted(b.settings, name);
  if (listed_why == VoteWhy::denied)
    return (void)refuse(
        b, slot, listed_why,
        std::string(name) +
            " may not be voted on (sv_vote_deny); next: `vote rules` says what may");
  if (listed_why == VoteWhy::not_allowed)
    return (void)refuse(
        b, slot, listed_why,
        std::string(name) + " is not on sv_vote_allow; next: `vote rules` says what may");
  VoteCheck check;
  if (target.check == nullptr || !target.check(target.ctx, name, value, check))
    return (void)refuse(
        b, slot, VoteWhy::refused,
        check.error.empty() ? std::string(name) + ": this server cannot change it" : check.error);
  b.open = true;
  b.caller = slot;
  b.ends = now + b.settings.time_s;
  b.asked.name = std::string(name);
  b.asked.value = check.value.empty() ? std::string(value) : std::move(check.value);
  b.asked.key = check.key.empty() ? b.asked.name : std::move(check.key);
  b.asked.before = std::move(check.before);
  b.ballots.fill(Ballot::none);
  b.ballots[static_cast<size_t>(slot)] = Ballot::yes;
  b.called++;
  std::string line = who(b, slot) + " calls " + change_text(b.asked);
  if (!b.asked.before.empty()) line += " (now " + b.asked.before + ")";
  line += ": vote yes/no";
  say(b, VoteNoteKind::called, slot, -1, std::move(line));
  settle(b, now);
}

void answer(VoteBook& b, int slot, Ballot ballot, double now) {
  if (!b.open)
    return (void)refuse(b, slot, VoteWhy::none_open,
                        "no vote is open; next: `vote <setting> <value>` calls one");
  if (!is_voter(b, slot)) return (void)refuse(b, slot, VoteWhy::not_voter);
  Ballot& mine = b.ballots[static_cast<size_t>(slot)];
  if (mine == ballot)
    return (void)refuse(b, slot, VoteWhy::same,
                        std::string("you said ") + (ballot == Ballot::yes ? "yes" : "no") +
                            " already: " + tally(b));
  mine = ballot;
  say(b, VoteNoteKind::answered, slot, -1,
      who(b, slot) + " votes " + (ballot == Ballot::yes ? "yes" : "no") + ": " + tally(b));
  settle(b, now);
}

}  // namespace

const char* vote_why_text(VoteWhy w) {
  switch (w) {
    case VoteWhy::ok:
      return "ok";
    case VoteWhy::off:
      return "votes are off on this server (sv_vote 0)";
    case VoteWhy::not_voter:
      return "only the people on the server vote; join, then vote";
    case VoteWhy::busy:
      return "a vote is open; next: answer it, then call yours";
    case VoteWhy::none_open:
      return "no vote is open; next: `vote <setting> <value>` calls one";
    case VoteWhy::waiting:
      return "your last vote failed; next: wait sv_vote_wait seconds";
    case VoteWhy::denied:
      return "that setting may not be voted on; next: `vote rules`";
    case VoteWhy::not_allowed:
      return "that setting is not on sv_vote_allow; next: `vote rules`";
    case VoteWhy::no_value:
      return "a vote needs a value; next: `vote <setting> <value>`";
    case VoteWhy::refused:
      return "the server cannot make that change";
    case VoteWhy::not_caller:
      return "only the player who called the vote may cancel it";
    case VoteWhy::same:
      return "you gave that answer already";
  }
  return "?";
}

bool vote_pattern_match(std::string_view p, std::string_view s) {
  // Greedy with one backtrack point: the last `*` seen.
  size_t pi = 0, si = 0, star = std::string_view::npos, mark = 0;
  while (si < s.size()) {
    if (pi < p.size() && p[pi] == '*') {
      star = pi++;
      mark = si;
    } else if (pi < p.size() && p[pi] == s[si]) {
      pi++;
      si++;
    } else if (star != std::string_view::npos) {
      pi = star + 1;
      si = ++mark;
    } else {
      return false;
    }
  }
  while (pi < p.size() && p[pi] == '*') pi++;
  return pi == p.size();
}

VoteWhy vote_permitted(const VoteSettings& s, std::string_view name) {
  if (listed(s.deny, name)) return VoteWhy::denied;
  if (has_words(s.allow) && !listed(s.allow, name)) return VoteWhy::not_allowed;
  return VoteWhy::ok;
}

void vote_seat(VoteBook& b, int slot, std::string_view name, double now) {
  if (!valid_slot(slot)) return;
  const auto at = static_cast<size_t>(slot);
  const uint32_t bit = 1u << slot;
  if (!name.empty()) {
    b.voters |= bit;
    if (b.names[at] != name) b.names[at] = std::string(name);
    return;
  }
  if ((b.voters & bit) == 0) return;
  b.voters &= ~bit;
  b.names[at].clear();
  b.failed_at[at] = 0;
  if (!b.open) return;
  b.ballots[at] = Ballot::none;
  settle(b, now);
}

void vote_tick(VoteBook& b, double now) {
  if (!b.open || now < b.ends) return;
  say(b, VoteNoteKind::expired, b.caller, -1,
      "Vote ran out: " + change_text(b.asked) + " (" + std::to_string(vote_count(b, Ballot::yes)) +
          " yes, " + std::to_string(vote_needed(b)) + " needed)");
  if (valid_slot(b.caller)) b.failed_at[static_cast<size_t>(b.caller)] = now;
  b.failed++;
  close_vote(b);
}

bool vote_command(VoteBook& b, const VoteTarget& target, int slot, std::string_view line,
                  double now) {
  line = trim(line);
  const size_t space = line.find_first_of(" \t");
  const std::string_view word = line.substr(0, space);
  const std::string_view rest =
      space == std::string_view::npos ? std::string_view() : trim(line.substr(space));
  if (word == "yes" || word == "no") {
    if (!b.open || !rest.empty()) return false;  // the mod's own `yes` when no vote is open
    answer(b, slot, word == "yes" ? Ballot::yes : Ballot::no, now);
    return true;
  }
  if (word != "vote") return false;
  vote_tick(b, now);
  if (rest.empty()) {
    if (!b.open) {
      tell(b, slot,
           "vote: no vote is open; next: `vote <setting> <value>` calls one, "
           "`vote rules` says which may");
      return true;
    }
    tell(b, slot,
         "vote: " + change_text(b.asked) + " called by " + who(b, b.caller) + ": " + tally(b) +
             ", " + std::to_string(vote_seconds_left(b, now)) + " s left");
    return true;
  }
  if (rest == "yes" || rest == "no") {
    answer(b, slot, rest == "yes" ? Ballot::yes : Ballot::no, now);
    return true;
  }
  if (rest == "rules") {
    tell(b, slot, rules_text(b));
    return true;
  }
  if (rest == "cancel") {
    if (!b.open) return refuse(b, slot, VoteWhy::none_open);
    if (slot != b.caller) return refuse(b, slot, VoteWhy::not_caller);
    say(b, VoteNoteKind::cancelled, slot, -1, "Vote cancelled: " + change_text(b.asked));
    close_vote(b);
    return true;
  }
  call(b, target, slot, rest, now);
  return true;
}

std::vector<VoteChange> vote_take(VoteBook& b) {
  std::vector<VoteChange> out;
  out.swap(b.due);
  return out;
}

std::vector<VoteChange> vote_map_over(VoteBook& b) {
  if (b.open) {
    say(b, VoteNoteKind::cancelled, b.caller, -1,
        "Vote dropped: " + change_text(b.asked) + ", new map");
    close_vote(b);
  }
  b.due.clear();
  std::vector<VoteChange> out;
  out.reserve(b.kept.size());
  for (size_t i = b.kept.size(); i-- > 0;) {
    VoteChange c = std::move(b.kept[i]);
    c.value = c.before;
    say(b, VoteNoteKind::reverted, -1, -1,
        c.name + " back to " + (c.before.empty() ? std::string("default") : c.before) +
            " for the new map");
    out.push_back(std::move(c));
  }
  b.kept.clear();
  return out;
}

int vote_voter_count(const VoteBook& b) { return std::popcount(b.voters); }

int vote_count(const VoteBook& b, Ballot ballot) {
  int n = 0;
  for (int i = 0; i < kVoteSeats; i++)
    if (is_voter(b, i) && b.ballots[static_cast<size_t>(i)] == ballot) n++;
  return n;
}

int vote_needed(const VoteBook& b) { return vote_voter_count(b) / 2 + 1; }

int vote_seconds_left(const VoteBook& b, double now) {
  if (!b.open) return 0;
  const double left = b.ends - now;
  return left <= 0 ? 0 : static_cast<int>(std::ceil(left - 1e-9));
}

}  // namespace ge::server
