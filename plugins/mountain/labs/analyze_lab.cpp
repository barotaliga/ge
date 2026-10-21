// LAB `analyze`: the generator's scoreboard over a seed or a sweep: build each map (at each
// --version: a ski area's map is held to its course's and its area's checks), hold the
// finished map to the rule book
// (`analysis::analyze_level`, the same verdict the generator accepts on), and name every finding
// by its R-rule. With `--refusals`, also build each attempt the search threw away and print
// why, in the analysis' own words: the search made readable.
//
// ge:summary Lab: maps over seeds held to the rule book; findings by R-rule; refused attempts
// ge:keywords lab analyze analyze_level findings rules rule-book sweep refusals attempts
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#include <map>
#include <string>

#include "lab/lab.hpp"
#include "mountain/analysis/index.hpp"
#include "mountain/labs/level_map.hpp"
#include "mountain/mapgen/generate.hpp"

namespace {

namespace an = ge::mountain::analysis;

void run(ge::lab::Lab& lab) {
  std::vector<double> seeds = lab.arg_nums("seeds");
  const int count = static_cast<int>(lab.arg_num("count"));
  if (count > 0) {
    seeds.clear();
    for (int i = 0; i < count; i++) seeds.push_back(lab.arg_num("from") + i);
  }
  const bool refusals = lab.arg_bool("refusals");
  ge::lab::LabTable maps{"maps", {"map", "attempt", "errors", "warns", "rules"}, {}};
  ge::lab::LabTable refused{"refused attempts", {"map", "attempt", "why"}, {}};
  std::map<std::string, int> by_rule;
  int maps_built = 0, attempts = 0, errors = 0, warns = 0;
  for (const int version : ge::mountain::labs::map_versions(lab)) {
    for (const double seed : seeds) {
      const std::string s = ge::mountain::labs::map_id(seed, version);
      const auto built = ge::mountain::labs::build_map(lab, seed, version);
      if (!built) continue;
      const ge::mountain::mapgen::Level* level = &*built;
      maps_built++;
      attempts += level->attempt + 1;
      const an::LevelAnalysis a = an::analyze_level(*level);
      int e = 0, w = 0;
      std::string rules;
      for (const an::Finding& f : a.findings) {
        (f.severity == an::Severity::error ? e : w)++;
        by_rule[f.rule]++;
        rules += (rules.empty() ? "" : " ") + f.rule;
      }
      errors += e;
      warns += w;
      maps.rows.push_back({s, std::to_string(level->attempt), std::to_string(e), std::to_string(w),
                           rules.empty() ? "-" : rules});
      if (e) {
        lab.fail(
            "seed " + s + " was accepted with " + std::to_string(e) + " error findings",
            "The generator must refuse what the analysis errors on: compare "
            "plugins/mountain/mapgen/generate.cpp's accept test with analysis/index.cpp. Skill: "
            "mountain.");
      }
      for (int k = 0; refusals && k < level->attempt; k++) {
        ge::mountain::mapgen::GenerateOptions opts;
        opts.version = level->version;
        opts.region = level->region;
        opts.only = k;
        try {
          ge::mountain::mapgen::generate_level(seed, opts);
          refused.rows.push_back({s, std::to_string(k), "(built when tried alone)"});
        } catch (const ge::mountain::mapgen::GenerateError& err) {
          refused.rows.push_back({s, std::to_string(k), err.what()});
        }
      }
    }
  }
  lab.metric("maps", maps_built, "count");
  lab.metric("attempts-per-map", maps_built ? double(attempts) / maps_built : 0, "attempts");
  lab.metric("errors", errors, "count");
  lab.metric("warns", warns, "count");
  for (const auto& [rule, n] : by_rule) lab.metric("findings." + rule, n, "count");
  lab.table(std::move(maps));
  if (refusals) lab.table(std::move(refused));
}

const ge::lab::LabSpec kSpec{
    "analyze",
    ge::lab::LabKind::sim,
    "plugin:mountain",
    "plugins/mountain/analysis",
    "mountain",
    "Does each map pass the rule book, which rules does it brush against, and why were the "
    "attempts before it refused?",
    {
        {"seeds", "38", "seeds, comma-separated"},
        {"count", "0", "sweep this many seeds from --from instead"},
        {"from", "1", "the sweep's first seed"},
        {"refusals", "", "also build each refused attempt alone and print its refusal", true},
        ge::mountain::labs::kVersionFlag,
        ge::mountain::labs::kRegionFlag,
    },
    "--seeds=38 --version=1,8",
    run,
};
GE_LAB(kSpec);

}  // namespace
