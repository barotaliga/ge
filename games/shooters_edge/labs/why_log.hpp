// THE BOTS' DECISION LOG: what each bot decided through a match and when it changed its mind, on
// the match recording's clock, so a moment of the recording (or of its video) is answered by the
// log ("red2 stopped shooting at 4:12: why?"). The fighters already record every decision on an
// `ai::Why` (engine/ai/why.hpp: the fight mode, the trigger, the aim, the pilot's link, the role's
// goal, sight, hearing...); a match keeps them only when handed one (`TeamMatchOptions::why`).
// A 10-minute 4on4 makes millions of decisions, so the log keeps a decision only when its choice
// differs from the bot's last one of that name (with what settled it, the margin and its facts),
// and counts every one (how often each is made and how often it changes: a bot that flips its
// mind ten times a second is seen at once). Beside the decisions, EVENTS: a sudden loss of speed
// (a wall or a player met: 120 u/s or more in one command) with the pilot's mode and link, and
// each death. Held in memory, written once at the match's end (`write_why_log`).
//
// THE FILE: a TSV, a row an entry: t (the server's seconds: the recording's clock to a frame,
// 0.01 s measured), bot (its name), kind (decision, event, count), name, chosen, because,
// margin, facts ("name=value ..."); the count rows at the end (t -1): a decision's name, how
// many times made (chosen) and changed (because), over every bot. Read back by
// `read_why_log`; the `se-why` lab answers a bot and a moment out of it.
//
// ge:summary The bots' decision log: decisions kept when they change, events, on the demo's clock
// ge:keywords why-log decision-log bots why debug flips changes mind demo-clock mvd moment events
// ge:keywords bump speed-loss death se-why se-team
// ge:skill bots labs
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "ai/why.hpp"
#include "core/num_text.hpp"

namespace shooters_edge::labs {

struct WhyEntry {
  double t = 0;
  std::string bot, kind, name, chosen, because;
  double margin = 0;  // NaN: a decision with no options
  std::string facts;
};

struct WhyLog {
  ge::ai::Why why;  // every bot's (`follow` by slot), read and cleared after each command
  struct Bot {
    std::map<std::string, std::string> last;  // decision -> its last choice
    float speed = 0;
    bool seen = false;
  };
  std::array<Bot, 64> bots{};
  std::map<std::string, std::pair<int64_t, int64_t>> counts;  // made, changed
  std::vector<WhyEntry> entries;
  std::array<std::string, 64> names{};  // a bot's name by its index `k`, set by the caller

  // After bot `k`'s command at `t`: its decisions read (kept when changed) and cleared; a speed
  // lost since its last command noted as an event (`mode`, `link`: the pilot's then).
  void take(int k, double t, float speed, const char* mode, const char* link, float x, float y,
            float z) {
    if (k < 0 || k >= 64) return;
    Bot& b = bots[static_cast<size_t>(k)];
    for (const ge::ai::Decision& d : why.decisions()) {
      auto& c = counts[d.name];
      c.first++;
      std::string& last = b.last[d.name];
      if (last == d.chosen) continue;
      if (!last.empty()) c.second++;
      last = d.chosen;
      std::string facts;
      for (const ge::ai::Fact& f : why.facts(d))
        facts += (facts.empty() ? "" : " ") + std::string(f.name) + "=" +
                 ge::core::num::to_fixed(f.value, 2);
      entries.push_back({t, std::to_string(k), "decision", d.name, d.chosen, d.because, d.margin,
                         std::move(facts)});
    }
    why.clear();
    if (b.seen && b.speed - speed >= 120)
      entries.push_back({t, std::to_string(k), "event", "speed_lost", mode, link, b.speed - speed,
                         "from=" + ge::core::num::to_fixed(b.speed, 0) +
                             " to=" + ge::core::num::to_fixed(speed, 0) +
                             " at=" + ge::core::num::to_fixed(x, 0) + "," +
                             ge::core::num::to_fixed(y, 0) + "," + ge::core::num::to_fixed(z, 0)});
    b.speed = speed;
    b.seen = true;
  }
};

// The log to `path`, entries in time order (a bot's index replaced by its name), then the counts;
// `extra`: more rows already in the TSV form (a match's deaths). False when it does not write.
inline bool write_why_log(const std::string& path, const WhyLog& log,
                          const std::vector<WhyEntry>& extra) {
  std::ofstream out(path);
  if (!out) return false;
  std::vector<const WhyEntry*> all;
  all.reserve(log.entries.size() + extra.size());
  for (const WhyEntry& e : log.entries) all.push_back(&e);
  for (const WhyEntry& e : extra) all.push_back(&e);
  std::stable_sort(all.begin(), all.end(),
                   [](const WhyEntry* a, const WhyEntry* b) { return a->t < b->t; });
  out << "t\tbot\tkind\tname\tchosen\tbecause\tmargin\tfacts\n";
  for (const WhyEntry* e : all) {
    std::string bot = e->bot;
    if (!bot.empty() && bot.find_first_not_of("0123456789") == std::string::npos) {
      const size_t k = std::stoul(bot);
      if (k < log.names.size() && !log.names[k].empty()) bot = log.names[k];
    }
    out << ge::core::num::to_fixed(e->t, 2) << '\t' << bot << '\t' << e->kind << '\t' << e->name
        << '\t' << e->chosen << '\t' << e->because << '\t'
        << (std::isnan(e->margin) ? std::string("-") : ge::core::num::to_fixed(e->margin, 2))
        << '\t' << e->facts << '\n';
  }
  for (const auto& [name, c] : log.counts)
    out << "-1\t*\tcount\t" << name << '\t' << c.first << '\t' << c.second << "\t-\t\n";
  return static_cast<bool>(out);
}

// A log written by `write_why_log` (or any TSV of its columns), every row. False with the reason.
inline bool read_why_log(const std::string& text, std::vector<WhyEntry>& out, std::string& error) {
  std::istringstream in(text);
  std::string line;
  if (!std::getline(in, line) || line.rfind("t\tbot\tkind", 0) != 0) {
    error = "not a bots' decision log (its first line is not the t, bot, kind header)";
    return false;
  }
  while (std::getline(in, line)) {
    std::vector<std::string> f;
    size_t at = 0;
    while (f.size() < 7) {
      const size_t tab = line.find('\t', at);
      if (tab == std::string::npos) break;
      f.push_back(line.substr(at, tab - at));
      at = tab + 1;
    }
    f.push_back(line.substr(at));
    if (f.size() < 8) continue;
    WhyEntry e;
    e.t = std::strtod(f[0].c_str(), nullptr);
    e.bot = f[1];
    e.kind = f[2];
    e.name = f[3];
    e.chosen = f[4];
    e.because = f[5];
    e.margin = f[6] == "-" ? std::nan("") : std::strtod(f[6].c_str(), nullptr);
    e.facts = f[7];
    out.push_back(std::move(e));
  }
  return true;
}

}  // namespace shooters_edge::labs
