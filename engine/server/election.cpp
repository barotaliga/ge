// An election for a role, defined: the rules in election.hpp's header.
//
// ge:summary One election for a role at a time: open, yes and no, the count, timeout, the block
// ge:keywords election elect election_open election_vote election_tick votes-needed
// ge:skill net
// ge:test tests/server_election_test.cpp
#include "server/election.hpp"

#include <algorithm>
#include <bit>

namespace ge::server {

namespace {

std::string fill(std::string line, const std::string& key, const std::string& value) {
  for (size_t at = line.find(key); at != std::string::npos; at = line.find(key, at + value.size()))
    line.replace(at, key.size(), value);
  return line;
}

void say(Election& e, const std::string& text, const std::string& name, int n, int to) {
  if (text.empty()) return;
  std::string line = fill(text, "{name}", name);
  line = fill(line, "{role}", e.open.name);
  line = fill(line, "{n}", std::to_string(n));
  line = fill(line, "{votes}", n == 1 ? "vote" : "votes");
  line = fill(line, "{seconds}", n == 1 ? "second" : "seconds");
  size_t start = 0;
  while (true) {
    const size_t nl = line.find('\n', start);
    const std::string part = line.substr(start, nl == std::string::npos ? nl : nl - start);
    if (!part.empty()) {
      e.said.push_back(part);
      e.to.push_back(to);
    }
    if (nl == std::string::npos) break;
    start = nl + 1;
  }
}

bool in_range(int slot) { return slot >= 0 && slot < kElectionSeats; }

void end(Election& e, double now) {
  if (in_range(e.open.slot))
    e.blocked_until[static_cast<size_t>(e.open.slot)] = now + e.settings.block_s;
  e.open = {};
  e.yes = 0;
}

// The yes counted among the voters: passed (the role handed over), or "more" said when `tell`.
void count(Election& e, const ElectionWords& w, uint32_t voters, double now, bool tell = true) {
  const int have = std::popcount(e.yes & voters);
  const int need = election_needed(e.settings, voters);
  if (have >= need) {
    e.passed = e.open;
    end(e, now);
    return;
  }
  if (tell) say(e, w.more, "", need - have, -1);
}

}  // namespace

int election_needed(const ElectionSettings& s, uint32_t voters) {
  const int percent = std::clamp(s.percent, 51, 100);
  const int n = std::popcount(voters);
  return std::max(s.min_votes, (percent * n + 99) / 100);
}

bool election_running(const Election& e) { return e.open.slot >= 0; }

bool election_open(Election& e, const ElectionWords& w, const ElectionRole& role,
                   const std::string& name, uint32_t voters, double now) {
  if (!in_range(role.slot)) return false;
  const size_t at = static_cast<size_t>(role.slot);
  if (election_running(e)) {
    say(e, w.busy, name, 0, role.slot);
    return false;
  }
  if (now < e.blocked_until[at]) {
    // Whole seconds left, rounded up.
    const double left = e.blocked_until[at] - now;
    int n = static_cast<int>(left);
    if (n < left) n++;
    say(e, w.wait, name, n, role.slot);
    return false;
  }
  e.open = role;
  e.ends = now + e.settings.time_s;
  e.yes = (voters >> role.slot) & 1u ? 1u << role.slot : 0;
  say(e, w.requested, name, 0, -1);
  for (int s = 0; s < kElectionSeats; s++)
    if (s != role.slot && ((voters >> s) & 1u)) say(e, w.approve, name, 0, s);
  say(e, w.abort_hint, name, 0, role.slot);
  count(e, w, voters, now, false);
  return true;
}

bool election_vote(Election& e, const ElectionWords& w, int slot, const std::string& name, bool yes,
                   uint32_t voters, double now) {
  if (!election_running(e) || !in_range(slot)) return false;
  if (!((voters >> slot) & 1u)) return true;  // not a voter: the word is still the election's
  const uint32_t bit = 1u << slot;
  if (yes) {
    if (slot == e.open.slot) {
      say(e, w.not_self, name, 0, slot);
      return true;
    }
    if (e.yes & bit) {
      say(e, w.still_good, name, 0, slot);
      return true;
    }
    e.yes |= bit;
    say(e, w.gives, name, 0, -1);
    count(e, w, voters, now);
    return true;
  }
  if (!(e.yes & bit) || slot == e.open.slot) return true;
  e.yes &= ~bit;
  say(e, w.withdraws, name, 0, -1);
  count(e, w, voters, now);
  return true;
}

bool election_abort(Election& e, const ElectionWords& w, int slot, const std::string& name,
                    double now) {
  if (!election_running(e) || slot != e.open.slot) return false;
  say(e, w.aborted, name, 0, -1);
  end(e, now);
  return true;
}

void election_drop(Election& e, double now) {
  if (election_running(e)) end(e, now);
}

void election_leave(Election& e, const ElectionWords& w, int slot, uint32_t voters, double now) {
  if (!election_running(e) || !in_range(slot)) return;
  if (slot == e.open.slot) {
    election_drop(e, now);
    return;
  }
  const uint32_t bit = 1u << slot;
  if (!(e.yes & bit)) return;
  e.yes &= ~bit;
  count(e, w, voters & ~bit, now);
}

void election_tick(Election& e, const ElectionWords& w, double now) {
  if (!election_running(e) || now < e.ends) return;
  say(e, w.timed_out, "", 0, -1);
  end(e, now);
}

}  // namespace ge::server
