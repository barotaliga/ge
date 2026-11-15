// Timeouts in a match (timeout.hpp).
//
// ge:summary Timeouts in a match, defined
// ge:keywords timeout timein timeout_command timeout_tick
// ge:skill net
// ge:test tests/server_timeout_test.cpp
#include "server/timeout.hpp"

namespace ge::server {

namespace {

// Whole seconds from `now` to `at`, rounded up (0 once it is past).
int seconds_to(double at, double now) {
  const double d = at - now;
  if (d <= 0) return 0;
  const int whole = static_cast<int>(d);
  return whole + (d > whole ? 1 : 0);
}

std::string seconds_words(int s) { return std::to_string(s) + (s == 1 ? " second" : " seconds"); }

void say(TimeoutBook& b, int to, std::string line) {
  b.said.push_back(std::move(line));
  b.to.push_back(to);
}

int& used_by(TimeoutBook& b, std::string_view side) {
  for (auto& [name, n] : b.used)
    if (name == side) return n;
  b.used.emplace_back(std::string(side), 0);
  return b.used.back().second;
}

void resume(TimeoutBook& b, double now) {
  b.phase = TimeoutPhase::resuming;
  b.ends = now + b.settings.resume_s;
  b.told = seconds_to(b.ends, now);
  if (b.told > 0) say(b, -1, "The timeout is over. Play resumes in " + seconds_words(b.told) + ".");
}

}  // namespace

bool timeout_command(TimeoutBook& b, const TimeoutCaller& who, bool in_match, std::string_view line,
                     double now) {
  while (!line.empty() && line.back() == ' ') line.remove_suffix(1);
  const bool call = line == "timeout";
  if (!call && line != "timein") return false;
  if (b.phase == TimeoutPhase::resuming) {
    say(b, who.slot, "Play resumes in " + seconds_words(seconds_to(b.ends, now)) + ".");
    return true;
  }
  if (b.phase == TimeoutPhase::out) {
    if (call) {
      say(b, who.slot,
          b.caller_name + "'s timeout: " + seconds_words(seconds_to(b.ends, now)) +
              " left. Type timein to resume sooner if it is yours.");
    } else if (!who.player || who.side != b.side) {
      say(b, who.slot, "Only " + b.caller_name + "'s side can end this timeout early.");
    } else {
      say(b, -1, std::string(who.name) + " calls time in.");
      resume(b, now);
    }
    return true;
  }
  if (!call) {
    say(b, who.slot, "There is no timeout to end.");
    return true;
  }
  if (b.settings.per_side <= 0) {
    say(b, who.slot, "Timeouts are off on this server.");
    return true;
  }
  if (!who.player) {
    say(b, who.slot, "Only a player in the match can call a timeout.");
    return true;
  }
  if (!in_match) {
    say(b, who.slot, "A timeout can be called only while the match is on.");
    return true;
  }
  int& used = used_by(b, who.side);
  if (used >= b.settings.per_side) {
    say(b, who.slot,
        b.settings.per_side == 1 ? "Your side has used its timeout for this map."
                                 : "Your side has used all " + std::to_string(b.settings.per_side) +
                                       " of its timeouts for this map.");
    return true;
  }
  used++;
  b.called++;
  b.phase = TimeoutPhase::out;
  b.caller = who.slot;
  b.caller_name = who.name;
  b.side = who.side;
  b.ends = now + b.settings.length_s;
  b.told = seconds_to(b.ends, now);
  say(b, -1,
      std::string(who.name) + " calls a timeout. The game stops for " + seconds_words(b.told) +
          ".");
  return true;
}

void timeout_tick(TimeoutBook& b, double now) {
  if (b.phase == TimeoutPhase::none) return;
  const int left = seconds_to(b.ends, now);
  if (b.phase == TimeoutPhase::out) {
    if (left == 0) {
      resume(b, now);
    } else if (left != b.told) {
      b.told = left;
      if (left == 30 || left == 10) say(b, -1, seconds_words(left) + " left in the timeout.");
      return;
    }
    if (b.phase == TimeoutPhase::out) return;
  }
  const int count = seconds_to(b.ends, now);
  if (count == 0) {
    b.phase = TimeoutPhase::none;
    b.caller = -1;
    say(b, -1, "Play!");
    return;
  }
  if (count != b.told) {
    b.told = count;
    if (count <= 3) say(b, -1, std::to_string(count) + "...");
  }
}

void timeout_map_over(TimeoutBook& b) {
  b.phase = TimeoutPhase::none;
  b.caller = -1;
  b.used.clear();
}

int timeout_left(const TimeoutBook& b, std::string_view side) {
  for (const auto& [name, n] : b.used)
    if (name == side) return b.settings.per_side - n > 0 ? b.settings.per_side - n : 0;
  return b.settings.per_side > 0 ? b.settings.per_side : 0;
}

}  // namespace ge::server
