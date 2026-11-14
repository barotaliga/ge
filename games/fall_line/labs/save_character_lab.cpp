// LAB `save-character`: A SKIER CARRIED FROM ONE MOUNTAIN TO ANOTHER (docs/saves.md § Part of a
// run). The bot races seed `--from` with a skier who comes to it hurt (two injuries from an
// earlier run); his profile (profile.hpp: the body and the career) is taken out of the run's save
// as a character file (`save::extract`) and carried into a run opened on seed `--into`. Held:
// the profile's fields equal on the other side; the rest of the run seed `--into`'s own (the
// profile taken back out, its bytes are a fresh run's, and it skis to the fresh run's digest at
// every step); and the career taken from the second run is both runs summed.
//
//   make lab LAB=save-character
//   make lab LAB=save-character ARGS="--from=38 --into=7 --seconds=90"
//
// ge:summary Lab: Fall Line's skier profile taken from seed 38 and carried into seed 7,
// ge:summary nothing else
// ge:keywords lab save-character profile character career body carry import extract seed
// ge:skill save sim labs
// ge:test tests/fall_line_profile_test.cpp
#include <array>
#include <memory>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/play.hpp"
#include "fall_line/sim/profile.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "save/file.hpp"
#include "save/part.hpp"

namespace {

namespace fs = fall_line::sim;
namespace gl = ge::lab;
namespace mg = ge::mountain::mapgen;
namespace sk = ge::skier;
namespace sv = ge::save;

std::string fmt(double v, int places) { return ge::core::num::to_fixed(v, places); }

template <class T>
std::vector<uint8_t> bytes_of(const T& v) {
  std::vector<uint8_t> out;
  sv::Writer w(out, 1);
  w(v);
  return out;
}

// Two injuries from a run before this lab's: a concussion and a bruised pelvis.
sk::BodyState hurt_before() {
  sk::BodyState b;
  b.count = 2;
  b.injuries[0].part = sk::Part::head;
  b.injuries[0].ais = 2;
  b.injuries[1].part = sk::Part::pelvis;
  b.injuries[1].ais = 1;
  b.worst[static_cast<size_t>(sk::Part::head)] = 2;
  b.worst[static_cast<size_t>(sk::Part::pelvis)] = 1;
  b.peak = 14.25;
  b.blows = 3;
  return b;
}

void step_bot(fs::Driver& d) { d.step(fs::bot_controls(d.state(), 0, d.plan())); }

std::vector<std::string> career_row(const std::string& name, const fs::Career& c) {
  return {name,
          std::to_string(c.runs),
          std::to_string(c.finishes),
          c.best_time > 0 ? fmt(c.best_time, 2) : "-",
          fmt(c.seconds, 1),
          fmt(c.distance, 0),
          fmt(c.top_speed * 3.6, 1),
          std::to_string(c.wipeouts),
          std::to_string(c.resets),
          std::to_string(c.missed_gates)};
}

void run(gl::Lab& lab) {
  const double from = lab.arg_num("from");
  const double into = lab.arg_num("into");
  const auto most = static_cast<int64_t>(lab.arg_num("seconds") * fs::kHz);
  const auto map_from = std::make_shared<const mg::Level>(fs::race_map(from));
  const auto map_into = std::make_shared<const mg::Level>(fs::race_map(into));

  // The first run, and the whole run's save the game writes.
  fs::Driver a(map_from, 0);
  std::string error;
  fs::carry_in(a, fs::Profile{hurt_before(), {}}, error);
  gl::Trace speed_a{"seed " + fmt(from, 0), {}, {}};
  while (a.state().steps < most && !a.over()) {
    step_bot(a);
    if (a.state().steps % 12 == 0) {
      speed_a.x.push_back(a.state().t);
      speed_a.y.push_back(a.state().racers[0].skier.speed * 3.6);
    }
  }
  fs::Profile p;
  if (!fs::profile_of(a, p, error)) {
    lab.fail("save-character: no profile from seed " + fmt(from, 0) + ": " + error,
             "next: run make test T=fall_line_profile; read profile.cpp; skill: save");
    return;
  }
  sv::Meta meta;
  meta.game = "fall_line";
  std::vector<uint8_t> whole, character;
  {
    sv::FileWriter w(whole, meta);
    w.put(fs::kDriverTag, 0, fs::kDriverVersion, a);
    w.put(fs::kProfileTag, 0, fs::kProfileVersion, p);
    w.finish();
  }
  const std::array<uint32_t, 1> tags{fs::kProfileTag};
  if (!sv::extract(whole, "fall_line", tags, "the skier", character, error)) {
    lab.fail("save-character: the character does not cut out of the run's save: " + error,
             "next: run make test T=fall_line_profile; read profile.cpp; skill: save");
    return;
  }
  sv::FileReader r;
  fs::Profile carried;
  if (!r.open(character, "character", "fall_line") ||
      !r.get(fs::kProfileTag, 0, fs::kProfileVersion, carried)) {
    lab.fail("save-character: the character file does not read: " + r.problem(),
             "next: run make test T=fall_line_profile; read profile.cpp; skill: save");
    return;
  }

  // Carried into the other seed, beside that seed's fresh run.
  fs::Driver b(map_into, 0), c(map_into, 0);
  fs::carry_in(b, carried, error);
  const bool career_equal = b.carried() == p.career;
  const bool body_equal = bytes_of(b.state().racers[0].body) == bytes_of(p.body);
  fs::Driver undone = b;
  undone.carry({}, {});
  const bool rest_equal = bytes_of(undone) == bytes_of(c);
  int64_t parted = -1;
  gl::Trace speed_b{"seed " + fmt(into, 0) + ", the skier carried in", {}, {}};
  while (b.state().steps < most && !b.over() && !c.over()) {
    step_bot(b);
    step_bot(c);
    if (b.digest() != c.digest() && parted < 0) parted = b.state().steps;
    if (b.state().steps % 12 == 0) {
      speed_b.x.push_back(a.state().t + b.state().t);
      speed_b.y.push_back(b.state().racers[0].skier.speed * 3.6);
    }
  }
  fs::Profile two, alone;
  fs::profile_of(b, two, error);
  fs::profile_of(c, alone, error);

  lab.metric("character.bytes", static_cast<double>(character.size()), "bytes");
  lab.metric("profile.injuries", p.body.count, "count");
  lab.metric("career.equal", career_equal ? 1 : 0, "bool");
  lab.metric("body.equal", body_equal ? 1 : 0, "bool");
  lab.metric("rest.equal", rest_equal ? 1 : 0, "bool");
  lab.metric("parted.step", static_cast<double>(parted), "step");
  lab.metric("career.runs", two.career.runs, "count");
  lab.metric("career.seconds", two.career.seconds, "s");
  lab.metric("career.top-kmh", two.career.top_speed * 3.6, "km/h");
  if (!career_equal || !body_equal)
    lab.fail("save-character: the profile carried into seed " + fmt(into, 0) +
                 " is not the one taken from seed " + fmt(from, 0),
             "next: run make test T=fall_line_profile; read profile.cpp; skill: save");
  if (!rest_equal || parted >= 0)
    lab.fail("save-character: carrying the profile changed seed " + fmt(into, 0) + "'s own run" +
                 (parted >= 0 ? " (parted at step " + std::to_string(parted) + ")" : ""),
             "next: run make test T=fall_line_profile; read profile.cpp; skill: save");
  if (two.career.runs != 2 || two.career.seconds != p.career.seconds + b.state().t)
    lab.fail("save-character: the career from the second run is not both runs summed",
             "next: run make test T=fall_line_profile; read profile.cpp; skill: save");

  gl::LabTable t{"the skier's career",
                 {"profile", "runs", "finishes", "best s", "skied s", "m", "top km/h", "wipeouts",
                  "resets", "missed"},
                 {}};
  t.rows.push_back(career_row("taken from seed " + fmt(from, 0), p.career));
  t.rows.push_back(career_row("seed " + fmt(into, 0) + " alone", alone.career));
  t.rows.push_back(career_row("after both", two.career));
  lab.table(t);
  gl::LabTable h{
      "what was carried and what was not",
      {"held", "result"},
      {{"career equal on the other side", career_equal ? "yes" : "NO"},
       {"body equal (" + std::to_string(p.body.count) + " injuries)", body_equal ? "yes" : "NO"},
       {"the rest of the run seed " + fmt(into, 0) + "'s bytes", rest_equal ? "yes" : "NO"},
       {"the carried run's digest parted from seed " + fmt(into, 0) + "'s",
        parted < 0 ? "never" : "at step " + std::to_string(parted)},
       {"character file", std::to_string(character.size()) + " bytes"}}};
  lab.table(h);

  gl::LineChart chart;
  chart.spikes = false;
  chart.frame = {"ONE SKIER, TWO MOUNTAINS",
                 "the bot's speed down seed " + fmt(from, 0) + ", then down seed " + fmt(into, 0) +
                     " with the profile carried in (" + std::to_string(p.body.count) +
                     " injuries, the career so far)",
                 "s skied, both runs end to end",
                 "speed km/h",
                 960,
                 360};
  chart.traces = {speed_a, speed_b};
  chart.marks = {{a.state().t, "carried here"}};
  const gl::Chart drawn = gl::line_chart(chart);
  lab.shot({"carried",
            drawn.title,
            "The skier's speed down the first mountain and then the second: at the mark his "
            "profile (body and career) is taken out of the first run's save and carried into "
            "a run opened on the second seed, which then skis exactly as that seed's fresh run.",
            "two runs end to end; the second run's line is the fresh seed's own",
            {{"from", fmt(from, 0)}, {"into", fmt(into, 0)}}},
           gl::to_picture(drawn.plot));
}

const gl::LabSpec kSpec{
    "save-character",
    gl::LabKind::sim,
    "game:fall_line",
    "games/fall_line/sim/profile.hpp on engine/save/part.hpp and engine/play/saves.hpp",
    "save",
    "Is the skier's profile taken from one seed carried into another with its fields equal, and "
    "is the rest of that run the other seed's own?",
    {
        {"from", "38", "the seed the profile is taken from"},
        {"into", "7", "the seed it is carried into"},
        {"seconds", "300", "each run, s at most (both races finish in it)"},
    },
    "--seconds=8",
    run,
};
GE_LAB(kSpec);

}  // namespace
