// `secli mod run`: the package loaded, the scripted match played plain and twice with it, the two
// modded runs compared frame by frame, the cost timed (mod_run.hpp).
//
// ge:summary secli mod run's command: load, play plain and modded twice, compare, time
// ge:keywords secli mod-run replay hook-cost parted-frame
// ge:skill mod shooter agent-interface
// ge:test tests/shooters_edge_cli_mod_run_test.cpp
#include "shooters_edge/cli/mod_run.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "core/json_write.hpp"
#include "mod/loader.hpp"
#include "shooters_edge/sim/match.hpp"
#include "shooters_edge/sim/mod_api.hpp"

namespace shooters_edge::cli {

namespace {

namespace gc = ge::gamecli;
namespace gm = ge::mod;
namespace se = shooters_edge::sim;
using ge::core::json_bool;
using ge::core::json_num;
using ge::core::json_obj;
using ge::core::json_str;

std::string hex64(uint64_t v) {
  char buf[24];
  std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(v));
  return buf;
}

// A whole number flag in [lo, hi]; false when it is something else.
bool int_flag(const gc::Ctx& ctx, const char* name, long fallback, long lo, long hi, long& out) {
  out = fallback;
  if (!ctx.has(name)) return true;
  const std::string v = ctx.flag(name);
  char* end = nullptr;
  out = std::strtol(v.c_str(), &end, 10);
  return !v.empty() && end != nullptr && *end == '\0' && out >= lo && out <= hi;
}

// One match, timed in milliseconds.
se::MatchReport timed(const se::MatchOptions& opt, double& ms) {
  const auto t0 = std::chrono::steady_clock::now();
  se::MatchReport r = se::run_match(opt);
  ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  return r;
}

int cmd_mod_run(gc::Ctx& ctx) {
  if (ctx.words.size() != 1)
    return gc::refuse(ctx, 2, "mod run takes one package (the folder holding mod.manifest)",
                      "secli mod run build/mods/rocket-bounce");
  long frames = 0, seed = 0;
  if (!int_flag(ctx, "frames", 1100, 1, 100000, frames))
    return gc::refuse(ctx, 2, "--frames is a whole number of server frames, 1 to 100000",
                      "secli mod run <package> --frames=1100");
  if (!int_flag(ctx, "seed", 1, 0, 0x7fffffff, seed))
    return gc::refuse(ctx, 2, "--seed is a whole number", "secli mod run <package> --seed=1");
  const std::string package = ctx.words[0];
  gm::LoadedMod mod;
  const gm::LoadResult load = gm::load_mod(package, {SE_MOD_GAME, SE_MOD_API}, mod);
  if (!load.ok())
    return gc::refuse(ctx, 1, "the game refused " + package + ": " + load.errors.front(),
                      "ge mod check " + package);

  se::MatchOptions opt;
  opt.frames = static_cast<int>(frames);
  // The plain match three times, the fastest kept: the cost below is a difference of times.
  double plain_ms = 0, modded_ms = 0, again_ms = 0;
  const se::MatchReport plain = timed(opt, plain_ms);
  for (int k = 0; k < 2; k++) {
    double ms = 0;
    (void)timed(opt, ms);
    plain_ms = std::min(plain_ms, ms);
  }
  opt.mod = mod.table;
  opt.mod_seed = static_cast<uint32_t>(seed);
  const se::MatchReport modded = timed(opt, modded_ms);
  const se::MatchReport again = timed(opt, again_ms);
  if (!modded.mod_errors.empty())
    return gc::refuse(ctx, 1, "the game refused the mod's table: " + modded.mod_errors.front(),
                      "build the mod against games/shooters_edge/sim/mod_api.hpp");

  int parted = -1;
  const size_t n = std::min(modded.frames.size(), again.frames.size());
  for (size_t i = 0; i < n && parted < 0; i++)
    if (modded.frames[i].hash != again.frames[i].hash) parted = static_cast<int>(i);
  if (parted < 0 && modded.frames.size() != again.frames.size()) parted = static_cast<int>(n);
  if (parted >= 0)
    return gc::refuse(ctx, 1,
                      mod.manifest.name +
                          " did not replay: two runs with the same seed part at "
                          "frame " +
                          std::to_string(parted),
                      "keep all of the mod's state in its block and draw only from "
                      "host->random (docs/mods.md § Determinism)");

  const bool changed = plain.final_hash != modded.final_hash;
  const int64_t calls = modded.mod_run.hook_calls;
  // The modded match's time over the plain one's (the faster of the two modded runs), as a share
  // of the plain match's and a hook call; under 2% is the timing's own noise.
  const double best_ms = std::min(modded_ms, again_ms);
  const double extra_ms = std::max(0.0, best_ms - plain_ms);
  const double share = plain_ms > 0 ? 100.0 * extra_ms / plain_ms : 0.0;
  const bool noise = share < 2.0;
  const double us_per_call =
      calls > 0 && !noise ? extra_ms * 1000.0 / static_cast<double>(calls) : 0.0;
  char cost[32], pct[32], plain_t[32], modded_t[32];
  std::snprintf(cost, sizeof cost, "%.3f", us_per_call);
  std::snprintf(pct, sizeof pct, "%.1f", noise ? 0.0 : share);
  std::snprintf(plain_t, sizeof plain_t, "%.1f", plain_ms);
  std::snprintf(modded_t, sizeof modded_t, "%.1f", best_ms);
  const std::string cost_words =
      noise ? "within the timing's noise"
            : std::string("about ") + pct + "% of the match's time, " + cost + " us a call";

  const std::string result = json_obj({
      {"name", json_str(mod.manifest.name)},
      {"version", json_str(mod.manifest.version)},
      {"platform", json_str(mod.platform)},
      {"frames", json_num(frames)},
      {"seed", json_num(seed)},
      {"replay", json_str("holds")},
      {"changed", json_bool(changed)},
      {"hook_calls", json_num(calls)},
      {"refusals", json_num(modded.mod_run.refusals)},
      {"log_lines", json_num(static_cast<long long>(modded.mod_log.size()))},
      {"plain_hash", json_str(hex64(plain.final_hash))},
      {"modded_hash", json_str(hex64(modded.final_hash))},
      {"plain_ms", plain_t},
      {"modded_ms", modded_t},
      {"cost_percent", pct},
      {"us_per_hook_call", cost},
  });
  std::string text = "played " + std::to_string(frames) + " frames without " + mod.manifest.name +
                     " and twice with it\n";
  text += "  replay: holds (every frame's hash agrees)\n";
  text += std::string("  changed the match: ") + (changed ? "yes" : "no") + "\n";
  text += "  hooks run: " + std::to_string(calls) +
          ", refused calls: " + std::to_string(modded.mod_run.refusals) + "\n";
  text += "  cost: " + cost_words + " (" + modded_t + " ms against " + plain_t + " ms)\n";
  const std::string summary = mod.manifest.name + " " + mod.manifest.version + " replays over " +
                              std::to_string(frames) + " frames" +
                              (changed ? "" : ", and changes nothing in the scenario");
  return gc::answer(ctx, result, text, summary,
                    changed ? "ge mod sign " + package
                            : "check the mod's hooks: the scenario fires rockets and grenades");
}

}  // namespace

gc::Command mod_run_command() {
  return {"mod run",
          "<package>",
          "a mod package played in the game's mod scenario twice: does it replay, what it changes, "
          "what its hooks cost",
          "mod run build/mods/rocket-bounce",
          "shooters_edge",
          cmd_mod_run};
}

gc::CommandFlags mod_run_flags() {
  return {"mod run",
          {{"frames", "<n>", "server frames to play (default 1100, 14 seconds)"},
           {"seed", "<n>", "the seed of the mod's stream (default 1)"}}};
}

}  // namespace shooters_edge::cli
