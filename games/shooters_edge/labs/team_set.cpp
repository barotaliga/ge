// THE SET OF THE `se-team` LAB (team_set.hpp): the seeds and the variants' flags read, every
// match run on its own thread, the means and the tables.
//
// ge:summary The se-team lab's set run: seeds and variants read, matches in parallel, the means
// ge:keywords lab se-team set run_set seeds variants parse_seeds parse_flags replaced set-run
// ge:keywords deaths-tsv lava-deaths where-they-die
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#include "shooters_edge/labs/team_set.hpp"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <map>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "core/num_text.hpp"

namespace shooters_edge::labs::se_team {

namespace {

namespace gl = ge::lab;
using ge::core::num::to_fixed;

// THE SET (`--seeds=1-6`, `--variants="name:flags;name:flags"`): the A/B a bot change is judged by
// (the bots skill, § Shooter's Edge's bot plan). Every variant's match on every seed, each a child
// run of this lab with its own flags (the variant's over this run's, `--seed` the seed's), all in
// parallel on `--jobs` threads (a match is one thread; matches share nothing), read only once
// every one is done. Reported: each variant's means over its seeds as `<variant>.<metric>` (the
// metric's own key when there are no variants), the A/B table (frags by side, red-blue, frags a
// minute, damage, accuracy, the key items lying, speed, wet, the pilots' stuck and waits), and a
// row a seed. `{seed}` and `{variant}` in any flag's value (`--trace=<work>/t{variant}{seed}.tsv`,
// `--save`) are replaced per run. A head to head reads red-blue against the both-with variant's
// red-blue: the sides are not even.
struct SetRun {
  std::string variant;
  int seed = 0;
  std::vector<std::pair<std::string, std::string>> args;
  std::vector<gl::Metric> metrics;
  std::vector<std::string> failures;
  std::string deaths;  // the match's deaths.tsv, its header line left out
  std::string header;  // and that line
};

// "1-6", "1,3,5" or "7-9,12" as the seeds, in order; empty when it does not read.
std::vector<int> parse_seeds(const std::string& text) {
  std::vector<int> out;
  size_t at = 0;
  while (at < text.size()) {
    const size_t end = std::min(text.find(',', at), text.size());
    const std::string part = text.substr(at, end - at);
    const size_t dash = part.find('-');
    const int lo = std::atoi(part.substr(0, dash).c_str());
    const int hi = dash == std::string::npos ? lo : std::atoi(part.substr(dash + 1).c_str());
    if (lo <= 0 || hi < lo || hi - lo > 1000) return {};
    for (int s = lo; s <= hi; s++) out.push_back(s);
    at = end + 1;
  }
  return out;
}

// A variant's flags ("--features=-hop --sides=1") as name/value pairs; false on a word that is no
// `--name=value` or `--name`.
bool parse_flags(const std::string& text, std::vector<std::pair<std::string, std::string>>& out) {
  size_t at = 0;
  while (at < text.size()) {
    if (text[at] == ' ') {
      at++;
      continue;
    }
    const size_t end = std::min(text.find(' ', at), text.size());
    const std::string word = text.substr(at, end - at);
    if (!word.starts_with("--") || word.size() < 3) return false;
    const size_t eq = word.find('=');
    out.emplace_back(word.substr(2, eq == std::string::npos ? std::string::npos : eq - 2),
                     eq == std::string::npos ? "1" : word.substr(eq + 1));
    at = end;
  }
  return true;
}

std::string replaced(std::string v, const std::string& variant, int seed) {
  for (const auto& [from, to] :
       {std::pair<std::string, std::string>{"{seed}", std::to_string(seed)},
        {"{variant}", variant}}) {
    for (size_t at = v.find(from); at != std::string::npos; at = v.find(from, at + to.size()))
      v.replace(at, from.size(), to);
  }
  return v;
}

}  // namespace

void run_set(gl::Lab& lab, void (*run)(gl::Lab&)) {
  const std::vector<int> seeds =
      parse_seeds(lab.arg("seeds").empty() ? lab.arg("seed") : lab.arg("seeds"));
  if (seeds.empty()) {
    lab.fail("se-team: --seeds=" + lab.arg("seeds") + " reads as no seeds",
             "next: --seeds=1-6, --seeds=1,3,5 or --seeds=7-12 (seeds from 1)");
    return;
  }
  // The variants, in the order given; none: one variant of this run's own flags.
  std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::string>>>> variants;
  const std::string& list = lab.arg("variants");
  for (size_t at = 0; at < list.size();) {
    const size_t end = std::min(list.find(';', at), list.size());
    const std::string item = list.substr(at, end - at);
    const size_t colon = item.find(':');
    std::vector<std::pair<std::string, std::string>> flags;
    if (colon == 0 || colon == std::string::npos || !parse_flags(item.substr(colon + 1), flags)) {
      lab.fail("se-team: --variants: '" + item + "' is no name:flags",
               "next: --variants=\"on:--sides=1;off:--features=-hop --sides=1\" (a name, a colon, "
               "then the flags the variant changes; variants apart by ';')");
      return;
    }
    variants.emplace_back(item.substr(0, colon), std::move(flags));
    at = end + 1;
  }
  const bool named = !variants.empty();
  if (!named) variants.emplace_back("set", std::vector<std::pair<std::string, std::string>>{});

  std::vector<SetRun> runs;
  for (const auto& [name, flags] : variants) {
    for (const int seed : seeds) {
      SetRun s{name, seed, {}, {}, {}, {}, {}};
      for (const auto& [k, v] : lab.args()) {
        if (k == "seeds" || k == "variants" || k == "jobs") continue;
        s.args.emplace_back(k, k == "seed" ? std::to_string(seed) : v);
      }
      for (const auto& [k, v] : flags) {
        bool found = false;
        for (auto& [ak, av] : s.args)
          if (ak == k) {
            av = v;
            found = true;
          }
        if (!found) s.args.emplace_back(k, v);
      }
      for (auto& [k, v] : s.args) v = replaced(v, name, seed);
      runs.push_back(std::move(s));
    }
  }
  // Each run on the next free thread; a child draws no pictures (only_shots of none).
  const int jobs =
      std::max(1, lab.arg_num("jobs") > 0 ? static_cast<int>(lab.arg_num("jobs"))
                                          : static_cast<int>(std::thread::hardware_concurrency()));
  std::atomic<size_t> next{0};
  const auto work = [&] {
    for (size_t i = next++; i < runs.size(); i = next++) {
      SetRun& s = runs[i];
      gl::Lab child(lab.spec(), s.args);
      child.only_shots({"-"});
      run(child);
      s.metrics = child.metrics();
      s.failures = child.failures();
      for (const gl::Lab::File& f : child.files())
        if (f.name == "deaths.tsv") {
          const std::string all(f.bytes.begin(), f.bytes.end());
          const size_t eol = all.find('\n');
          s.deaths = eol == std::string::npos ? std::string() : all.substr(eol + 1);
          if (s.header.empty() && eol != std::string::npos) s.header = all.substr(0, eol + 1);
        }
    }
  };
  {
    std::vector<std::thread> pool;
    for (int k = 1; k < std::min<int>(jobs, static_cast<int>(runs.size())); k++)
      pool.emplace_back(work);
    work();
    for (std::thread& t : pool) t.join();
  }
  // EVERY DEATH OF THE SET (deaths.tsv): each match's rows behind its variant and seed, so where
  // and how the players die (in lava, by what way in) is read across the set as in one match.
  {
    std::string header;
    for (const SetRun& s : runs)
      if (header.empty()) header = s.header;
    std::string all = "variant\tseed\t" + header;
    for (const SetRun& s : runs)
      for (size_t at = 0; at < s.deaths.size();) {
        const size_t eol = std::min(s.deaths.find('\n', at), s.deaths.size());
        if (eol > at)
          all += s.variant + "\t" + std::to_string(s.seed) + "\t" +
                 s.deaths.substr(at, eol - at) + "\n";
        at = eol + 1;
      }
    lab.attach("deaths.tsv", std::vector<uint8_t>(all.begin(), all.end()), gl::Det::deterministic);
  }
  for (const SetRun& s : runs)
    for (const std::string& f : s.failures)
      lab.fail("se-team: variant " + s.variant + ", seed " + std::to_string(s.seed) + ": " + f,
               "next: run that one alone: se-team --seed=" + std::to_string(s.seed) +
                   " with the variant's flags");

  // Each variant's mean of every metric all its seeds report, in the first run's order.
  const auto value = [](const SetRun& s, const std::string& key, double& out) {
    for (const gl::Metric& m : s.metrics)
      if (m.key == key) {
        out = m.value;
        return true;
      }
    return false;
  };
  std::map<std::string, std::map<std::string, double>> means;
  for (const auto& [name, flags] : variants) {
    const SetRun* first = nullptr;
    for (const SetRun& s : runs)
      if (s.variant == name && first == nullptr) first = &s;
    for (const gl::Metric& m : first->metrics) {
      double sum = 0;
      int n = 0;
      for (const SetRun& s : runs) {
        double v = 0;
        if (s.variant == name && value(s, m.key, v)) {
          sum += v;
          n++;
        }
      }
      if (n != static_cast<int>(seeds.size())) continue;
      const double mean = sum / n;
      means[name][m.key] = mean;
      lab.metric(named ? name + "." + m.key : m.key, mean, m.unit, m.det);
    }
  }
  // THE A/B TABLE: a row a variant.
  static constexpr std::pair<const char*, const char*> kCols[] = {
      {"red", "team.red.frags"},
      {"blue", "team.blue.frags"},
      {"fpm", "team.frags-per-minute"},
      {"dmg", "team.damage-per-min"},
      {"acc", "team.accuracy"},
      {"ya", "team.lying.ya"},
      {"ya.going", "team.lying.ya.going"},
      {"ya.near", "team.lying.ya.near"},
      {"mh", "team.lying.mh"},
      {"rl", "team.lying.rl"},
      {"speed", "team.speed-avg"},
      {"wet", "team.wet"},
      {"stuck", "team.stuck"},
      {"waits", "team.waits"}};
  gl::LabTable ab{
      "the set: each variant's means over seeds " + lab.arg("seeds"), {"variant", "red-blue"}, {}};
  for (const auto& [label, key] : kCols) ab.columns.emplace_back(label);
  for (const auto& [name, flags] : variants) {
    const std::map<std::string, double>& m = means[name];
    const auto get = [&](const char* key) {
      const auto it = m.find(key);
      return it == m.end() ? std::string("-") : to_fixed(it->second, 2);
    };
    std::vector<std::string> row{name};
    const auto r = m.find("team.red.frags"), b = m.find("team.blue.frags");
    row.push_back(r == m.end() || b == m.end() ? "-" : to_fixed(r->second - b->second, 1));
    for (const auto& [label, key] : kCols) row.push_back(get(key));
    ab.rows.push_back(std::move(row));
  }
  lab.table(ab);
  // A row a run: the frags by side and a minute, so a seed that swings the mean is seen.
  gl::LabTable each{"the set: each run", {"variant", "seed", "red", "blue", "fpm", "speed"}, {}};
  for (const SetRun& s : runs) {
    std::vector<std::string> row{s.variant, std::to_string(s.seed)};
    for (const char* key :
         {"team.red.frags", "team.blue.frags", "team.frags-per-minute", "team.speed-avg"}) {
      double v = 0;
      row.push_back(value(s, key, v) ? to_fixed(v, 2) : "-");
    }
    each.rows.push_back(std::move(row));
  }
  lab.table(each);
}

}  // namespace shooters_edge::labs::se_team
