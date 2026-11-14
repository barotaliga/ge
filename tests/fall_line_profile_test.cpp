// games/fall_line/sim/profile.hpp: the skier's profile (body and career) taken from seed 38 and
// carried into seed 7: the profile's fields equal, the rest of the run seed 7's own (its bytes
// and its digests, step by step); the career summed again from the second run; a profile refused
// from a run where he died, and into a run already stepped; and the same through the play
// contract (`save_character`, `save::extract` from a whole run's save, `import_character`).
//
// ge:summary Test: Fall Line's skier profile carried from seed 38 into seed 7, and nothing else
// ge:keywords test profile character career body carry import extract seed
// ge:skill save sim testing
#include "fall_line/sim/profile.hpp"

#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/play.hpp"
#include "play/saves.hpp"
#include "save/part.hpp"

namespace {

namespace fs = fall_line::sim;
namespace gp = ge::play;
namespace sv = ge::save;
namespace sk = ge::skier;

void drive(fs::Driver& d, int64_t steps) {
  for (int64_t i = 0; i < steps && !d.over(); i++) d.step(fs::bot_controls(d.state(), 0, d.plan()));
}

template <class T>
std::vector<uint8_t> bytes_of(const T& v) {
  std::vector<uint8_t> out;
  sv::Writer w(out, 1);
  w(v);
  return out;
}

// A body with two injuries, as a hard run leaves one.
sk::BodyState hurt() {
  sk::BodyState b;
  b.count = 2;
  b.injuries[0].part = sk::Part::head;
  b.injuries[0].ais = 2;
  b.injuries[0].t = 12.5;
  b.injuries[1].part = sk::Part::pelvis;
  b.injuries[1].ais = 1;
  b.worst[static_cast<size_t>(sk::Part::head)] = 2;
  b.worst[static_cast<size_t>(sk::Part::pelvis)] = 1;
  b.peak = 14.25;
  b.blows = 3;
  return b;
}

gp::Setup setup(double seed) {
  gp::Setup s;
  std::string error;
  gp::make_setup(fs::kGame, seed, {{"rivals", "0"}}, s, error);
  return s;
}

}  // namespace

TEST(a_profile_from_seed_38_carried_into_seed_7_changes_nothing_else) {
  const auto map38 = std::make_shared<const ge::mountain::mapgen::Level>(fs::race_map(38));
  const auto map7 = std::make_shared<const ge::mountain::mapgen::Level>(fs::race_map(7));
  fs::Driver a(map38, 0);
  std::string error;
  CHECK(fs::carry_in(a, fs::Profile{hurt(), {}}, error));
  drive(a, 1800);
  fs::Profile p;
  CHECK_MSG(fs::profile_of(a, p, error), error);
  CHECK(p.career.runs == 1 && p.career.seconds == a.state().t);
  CHECK(p.career.top_speed > 5 && p.career.wipeouts == a.report(38).wipeouts);
  CHECK(bytes_of(p.body) == bytes_of(hurt()));

  fs::Driver b(map7, 0), c(map7, 0);
  CHECK(fs::carry_in(b, p, error));
  CHECK(b.carried() == p.career);
  CHECK(bytes_of(b.state().racers[0].body) == bytes_of(p.body));
  // Everything else seed 7's own: the profile taken back out, the run is the fresh one's bytes.
  fs::Driver undone = b;
  undone.carry({}, {});
  CHECK(bytes_of(undone) == bytes_of(c));
  // ...and it skis as seed 7 does: the same digest at every step.
  for (int i = 0; i < 1200 && !c.over(); i++) {
    b.step(fs::bot_controls(b.state(), 0, b.plan()));
    c.step(fs::bot_controls(c.state(), 0, c.plan()));
    CHECK_MSG(b.digest() == c.digest(), "the carried run parted at step " + std::to_string(i));
    if (b.digest() != c.digest()) break;
  }
  fs::Profile two;
  CHECK(fs::profile_of(b, two, error));
  CHECK(two.career.runs == 2 && two.career.seconds == p.career.seconds + b.state().t);
  CHECK(two.career.top_speed >= p.career.top_speed);
  CHECK(two.career.wipeouts == p.career.wipeouts + b.report(7).wipeouts);
}

TEST(a_dead_skier_or_a_stepped_run_carries_nothing) {
  const auto map7 = std::make_shared<const ge::mountain::mapgen::Level>(fs::race_map(7));
  fs::Driver d(map7, 0);
  sk::BodyState dead = hurt();
  dead.mortal = 3;
  std::string error;
  CHECK(fs::carry_in(d, fs::Profile{dead, {}}, error));
  fs::Profile p;
  CHECK(!fs::profile_of(d, p, error) && error.find("killed") != std::string::npos);
  fs::Driver e(map7, 0);
  drive(e, 10);
  CHECK(!fs::carry_in(e, fs::Profile{}, error) && error.find("already") != std::string::npos);
}

TEST(the_play_contract_saves_a_character_and_imports_it_into_another_seed) {
  std::string error;
  std::unique_ptr<gp::Run> a = fs::kGame.open(setup(38), error);
  CHECK_MSG(a != nullptr, error);
  std::vector<double> in(fs::kGame.axes.size(), 0);
  for (int i = 0; i < 900 && !a->over(); i++) {
    std::fill(in.begin(), in.end(), 0.0);
    a->pilot(in);
    a->step(in);
  }
  std::vector<uint8_t> character, run, cut;
  CHECK_MSG(gp::save_character(fs::kGame, setup(38), *a, {}, character, error), error);
  CHECK_MSG(gp::save_run(fs::kGame, setup(38), *a, 900, {}, run, error), error);
  CHECK_MSG(sv::extract(run, "fall_line", fs::kGame.character, "", cut, error), error);
  sv::FileReader rc, rx;
  CHECK(rc.open(character) && rx.open(cut));
  fs::Profile from_part, from_run;
  CHECK(rc.get(fs::kProfileTag, 0, fs::kProfileVersion, from_part));
  CHECK(rx.get(fs::kProfileTag, 0, fs::kProfileVersion, from_run));
  CHECK(from_part.career == from_run.career && from_part.career.runs == 1);

  std::unique_ptr<gp::Run> b = fs::kGame.open(setup(7), error);
  CHECK_MSG(gp::import_character(fs::kGame, *b, cut, error), error);
  CHECK(!gp::import_character(fs::kGame, *b, run, error));  // a whole run is loaded, not imported
  std::vector<uint8_t> again;
  CHECK_MSG(gp::save_character(fs::kGame, setup(7), *b, {}, again, error), error);
  sv::FileReader ra;
  fs::Profile back;
  CHECK(ra.open(again) && ra.get(fs::kProfileTag, 0, fs::kProfileVersion, back));
  CHECK(back.career.runs == 2 && back.career.seconds == from_run.career.seconds);
}
