// LAB `save-run`: FALL LINE'S RUN SAVED AND LOADED (docs/saves.md § The run saved). For each
// scene (a race with a field on human hands; the free ride with its crowd, up a lift and down
// again; out on the snowmobile; dropped by the helicopter; under the paramotor) the bot drives a
// straight run for `--seconds`, and the Driver is saved every `--every` seconds as the game saves
// it (the `DRVR` chunk, play.hpp). Each save is loaded into a run of the same scene already played
// on a few seconds (so a field missing from a describe keeps a value of its own) and stepped on
// beside the straight run's digests: a scene passes when no save parts from the run never saved,
// and when every loaded run that reaches the end saves to the straight run's bytes.
//
// Reported per scene: the saves made and how many parted (and at which step first), the save's
// bytes, and the time to save and to load it (measured); the bytes by part of the state; and the
// time to build the map the setup names, which a load pays before the save is read.
//
//   make lab LAB=save-run                       # every scene, 120 s each, a save every 10 s
//   make lab LAB=save-run ARGS="--scenes=free --every=1"
//
// ge:summary Lab: Fall Line's run saved every few seconds and loaded beside the run never saved
// ge:keywords lab save load save-run digest parted free-ride crowd lift sled heli para race bytes
// ge:keywords save-ms load-ms map-build
// ge:skill save sim labs
// ge:test tests/play_saves_test.cpp
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/crowd.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/hands.hpp"
#include "fall_line/sim/heli_outing.hpp"
#include "fall_line/sim/outing.hpp"
#include "fall_line/sim/para_outing.hpp"
#include "fall_line/sim/para_ride.hpp"
#include "fall_line/sim/play.hpp"
#include "fall_line/sim/simulate.hpp"
#include "fall_line/sim/sled_outing.hpp"
#include "lab/lab.hpp"
#include "save/file.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
namespace sv = ge::save;
using Det = ge::lab::Det;
using Pilot = std::function<fs::Controls(const fs::Driver&)>;

std::string fmt(double v, int places) { return ge::core::num::to_fixed(v, places); }

double ms_since(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

// How far a run is played before a save is loaded into it.
constexpr int64_t kDirtySteps = 360;

// A scene: a fresh Driver and the bot that drives it (a copy of the pilot is the pilot as it was,
// so a save keeps one beside it).
struct Scene {
  std::string name;
  std::function<fs::Driver()> make;
  std::function<Pilot(const fs::Driver&)> pilot;
};

struct Result {
  int32_t saves = 0;
  int32_t parted = 0;
  int64_t first_parted = -1;
  int32_t ends_differ = 0;
  size_t most_bytes = 0;
  double save_ms = 0;  // the mean
  double load_ms = 0;  // the mean
  int64_t steps = 0;
};

void save_driver(const fs::Driver& d, std::vector<uint8_t>& out) {
  sv::Meta meta;
  meta.game = "fall_line";
  sv::FileWriter w(out, meta);
  w.put(fs::kDriverTag, 0, fs::kDriverVersion, d);
  w.finish();
}

Result check(const Scene& scene, int64_t limit, int64_t every, ge::lab::Lab& lab) {
  Result res;
  fs::Driver d = scene.make();
  Pilot pilot = scene.pilot(d);
  struct Saved {
    int64_t step;
    std::vector<uint8_t> bytes;
    Pilot pilot;
  };
  std::vector<Saved> saves;
  std::vector<uint64_t> digests;
  double save_total = 0;
  while (true) {
    const int64_t step = d.state().steps;
    if (step % every == 0) {
      Saved s{step, {}, pilot};
      const auto t0 = std::chrono::steady_clock::now();
      save_driver(d, s.bytes);
      save_total += ms_since(t0);
      res.most_bytes = std::max(res.most_bytes, s.bytes.size());
      saves.push_back(std::move(s));
    }
    digests.push_back(d.digest());
    if (step >= limit || d.over()) break;
    d.step(pilot(d));
  }
  res.steps = d.state().steps;
  std::vector<uint8_t> end_straight, end_loaded;
  save_driver(d, end_straight);

  double load_total = 0;
  for (Saved& s : saves) {
    fs::Driver l = scene.make();
    Pilot dirty = scene.pilot(l);
    for (int64_t i = 0; i < kDirtySteps && !l.over(); i++) l.step(dirty(l));
    const auto t0 = std::chrono::steady_clock::now();
    sv::FileReader r;
    const bool ok =
        r.open(s.bytes, "save", "fall_line") && r.get(fs::kDriverTag, 0, fs::kDriverVersion, l);
    load_total += ms_since(t0);
    if (!ok) {
      lab.fail(scene.name + ": the save at step " + std::to_string(s.step) +
                   " does not load: " + r.problem(),
               "Run tests/play_saves_test.cpp; a describe that reads differently from how it "
               "writes is the usual cause (skill: save).");
      return res;
    }
    int64_t parted = -1;
    for (int64_t i = s.step;; i++) {
      if (l.digest() != digests[static_cast<size_t>(i)]) {
        parted = i;
        break;
      }
      if (i == res.steps) {
        save_driver(l, end_loaded);
        if (end_loaded != end_straight) res.ends_differ++;
        break;
      }
      l.step(s.pilot(l));
    }
    if (parted >= 0) {
      res.parted++;
      if (res.first_parted < 0) res.first_parted = parted;
    }
  }
  res.saves = static_cast<int32_t>(saves.size());
  res.save_ms = save_total / res.saves;
  res.load_ms = load_total / res.saves;
  return res;
}

// The bytes of each part of `d`'s state as the save writes it.
void parts(const std::string& name, const fs::Driver& d, ge::lab::LabTable& t) {
  const fs::GameState& s = d.state();
  const auto size = [](const auto& v) {
    std::vector<uint8_t> out;
    sv::write(out, v, 1);
    return out.size();
  };
  const size_t all = size(d);
  const size_t state = size(s);
  const auto row = [&](const std::string& part, size_t n) {
    t.rows.push_back({name, part, std::to_string(n),
                      fmt(100 * static_cast<double>(n) / static_cast<double>(all), 1) + "%"});
  };
  row("racers", size(s.racers));
  row("crowd", size(s.crowd));
  row("army, gunfire, wanted", size(s.army) + size(s.gunfire) + size(s.wanted));
  row("machines (sled, heli, para)", size(s.sled) + size(s.heli) + size(s.para));
  row("events, injuries, clock", size(s.events) + size(s.injuries) + 8 + 8 + 4);
  row("the bot's plan", size(d.plan()));
  row("the tape and the tallies", all - state - size(d.plan()));
  row("all", all);
}

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const auto limit = static_cast<int64_t>(lab.arg_num("seconds") * fs::kHz);
  const auto every = static_cast<int64_t>(lab.arg_num("every") * fs::kHz);
  const auto rivals = static_cast<int32_t>(lab.arg_num("rivals"));
  const std::string wanted = lab.arg("scenes");
  const auto picked = [&](const std::string& s) {
    return wanted == "all" || ("," + wanted + ",").find("," + s + ",") != std::string::npos;
  };
  if (every <= 0 || limit <= 0) {
    lab.fail("--every and --seconds must be above 0",
             "Pass seconds, e.g. --every=10 (skill: save).");
    return;
  }

  // The maps, and the time a load pays to build the one its setup names.
  auto t0 = std::chrono::steady_clock::now();
  const auto race = std::make_shared<const mg::Level>(fs::race_map(seed));
  const double race_build = ms_since(t0);
  std::shared_ptr<const mg::Level> free;
  double free_build = 0;
  const bool any_free = picked("free") || picked("sled") || picked("heli") || picked("para");
  if (any_free) {
    t0 = std::chrono::steady_clock::now();
    free = std::make_shared<const mg::Level>(fs::free_map(seed));
    free_build = ms_since(t0);
  }

  std::vector<Scene> scenes;
  if (picked("race")) {
    scenes.push_back(
        {"race",
         [&] {
           fs::Driver d(race, rivals);
           d.set_hands(fs::HandsProfile::human, seed);
           return d;
         },
         [&](const fs::Driver&) -> Pilot {
           return [hands = fs::BotHands(fs::HandsProfile::human, fs::hands_seed(seed, 0))](
                      const fs::Driver& d) mutable {
             return hands.press(fs::bot_controls(d.state(), 0, d.plan(), nullptr, hands.lead()),
                                nullptr);
           };
         }});
  }
  if (free && picked("free")) {
    // The first run with a lift at its chain's foot: down it, up the lift, down again.
    const fs::GameState probe = fs::create_free_ride(free, 0);
    int32_t first = 0;
    for (int32_t r = 0; r < static_cast<int32_t>(probe.runs->size()); r++) {
      if (fs::outing_from(*free, *probe.lifts, r).lift >= 0) {
        first = r;
        break;
      }
    }
    scenes.push_back({"free",
                      [&, first] {
                        fs::GameState st = fs::create_free_ride(free, first);
                        fs::deal_crowd(st);
                        return fs::Driver(std::move(st), 1);
                      },
                      [&, first](const fs::Driver& d) -> Pilot {
                        const fs::OutingPilot p(d.state(),
                                                fs::outing_from(*free, *d.state().lifts, first));
                        return [p](const fs::Driver& dr) { return p(dr.state(), 0); };
                      }});
  }
  if (free && picked("sled")) {
    scenes.push_back({"sled",
                      [&] {
                        fs::GameState st = fs::create_free_ride(free, 0);
                        fs::stand_by_sled(st, 0);
                        return fs::Driver(std::move(st), 0, 1);
                      },
                      [&](const fs::Driver& d) -> Pilot {
                        const fs::GameState& st = d.state();
                        const fs::SledSpot spot{st.sled.body.x, st.sled.body.z,
                                                st.sled.body.heading};
                        const fs::SledOutingPilot p(st, fs::sled_climb_run(*free, spot));
                        return [p](const fs::Driver& dr) { return p(dr.state(), 0); };
                      }});
  }
  if (free && picked("heli")) {
    scenes.push_back({"heli",
                      [&] {
                        fs::GameState st = fs::create_free_ride(free, 0);
                        fs::stand_by_heli(st, 0);
                        return fs::Driver(std::move(st), 0, 0, 1);
                      },
                      [&](const fs::Driver& d) -> Pilot {
                        const fs::HeliOutingPilot p(d.state(), fs::heli_drop_run(*free));
                        return [p](const fs::Driver& dr) { return p(dr.state(), 0); };
                      }});
  }
  if (free && picked("para")) {
    scenes.push_back({"para", [&] { return fs::Driver(fs::create_para_ride(free)); },
                      [&](const fs::Driver& d) -> Pilot {
                        const fs::ParaOutingPilot p(d.state());
                        return [p](const fs::Driver& dr) { return p(dr.state(), 0); };
                      }});
  }
  if (scenes.empty()) {
    lab.fail("--scenes named none of race, free, sled, heli, para",
             "Pass --scenes=all or a comma list of them (skill: save).");
    return;
  }

  ge::lab::LabTable checks{"each scene saved every " + fmt(lab.arg_num("every"), 1) + " s",
                           {"scene", "steps", "saves", "parted", "first parted", "ends differ"},
                           {}};
  ge::lab::LabTable costs{
      "each save's bytes and times (measured)", {"scene", "most bytes", "save ms", "load ms"}, {}};
  ge::lab::LabTable sizes{
      "the save's bytes by part, at the run's start", {"scene", "part", "bytes", "share"}, {}};
  int32_t parted = 0;
  for (const Scene& sc : scenes) {
    const Result r = check(sc, limit, every, lab);
    parts(sc.name, sc.make(), sizes);
    checks.rows.push_back(
        {sc.name, std::to_string(r.steps), std::to_string(r.saves), std::to_string(r.parted),
         r.first_parted < 0 ? "-" : std::to_string(r.first_parted), std::to_string(r.ends_differ)});
    costs.rows.push_back(
        {sc.name, std::to_string(r.most_bytes), fmt(r.save_ms, 3), fmt(r.load_ms, 3)});
    lab.metric(sc.name + ".saves", r.saves, "count");
    lab.metric(sc.name + ".parted", r.parted, "count");
    lab.metric(sc.name + ".ends-differ", r.ends_differ, "count");
    lab.metric(sc.name + ".most-bytes", static_cast<double>(r.most_bytes), "bytes", Det::measured);
    lab.metric(sc.name + ".save-ms", r.save_ms, "ms", Det::measured);
    lab.metric(sc.name + ".load-ms", r.load_ms, "ms", Det::measured);
    parted += r.parted + r.ends_differ;
  }
  lab.table(std::move(checks));
  lab.table(std::move(costs), Det::measured);
  lab.table(std::move(sizes), Det::measured);
  lab.metric("map-build.race-ms", race_build, "ms", Det::measured);
  if (free) lab.metric("map-build.free-ms", free_build, "ms", Det::measured);
  lab.note("A load opens the run on its setup first, which builds the map: " + fmt(race_build, 0) +
           " ms for the race map" +
           (free ? ", " + fmt(free_build, 0) + " ms for the free ride's" : std::string()) +
           " (measured), against the load's own milliseconds above. Time it on PRESET=release.");
  if (parted > 0) {
    lab.fail(std::to_string(parted) + " saves did not step on as the run never saved",
             "A field the step writes is missing from a describe: compare the `state` of the two "
             "runs at the first parted step (`ge diverge`), find the type that holds it and add "
             "it to that type's describe (skill: save).");
  }
}

const ge::lab::LabSpec kSpec{
    "save-run",
    ge::lab::LabKind::sim,
    "game:fall_line",
    "games/fall_line/sim/play (Run::save, Run::load) on engine/save (every state described)",
    "save",
    "Does a run saved at any moment and loaded step on exactly as the run never saved, and what "
    "do the save and the load cost?",
    {
        {"seed", "38", "the maps' seed"},
        {"scenes", "all", "race, free, sled, heli, para, or a comma list"},
        {"seconds", "120", "each scene's run, s at most"},
        {"every", "10", "seconds between saves"},
        {"rivals", "3", "the race's bots beside the player"},
    },
    "--scenes=race --seconds=6 --every=2 --rivals=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
