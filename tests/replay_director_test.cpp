// The director (engine/replay/director.hpp): marks cut into shots best first, every shot spaced
// from the others, a mark inside a shot covered and one too near dropped, the reel's length and
// shot count held, shots in run order; the reel's clock through shots at their rates; bad rules
// named.
//
// ge:summary Tests: the director ranks and spaces marks into shots; the reel clock; bad rules
// ge:keywords test replay director cut shot spacing covered dropped reel rate rules
// ge:skill replay testing
// ge:test tests/replay_director_test.cpp
#include <string>
#include <vector>

#include "check.hpp"
#include "replay/director.hpp"

namespace gr = ge::replay;

namespace {

// At 10 Hz: a flight opens 2 s before its mark and holds 3 s at half speed; a crash 1 s and 2 s.
const std::vector<gr::ShotRule> kRules{{"flight", 2, 3, 0.5, 1, "side"},
                                       {"crash", 1, 2, 1, 2, "chase"}};

}  // namespace

TEST(the_best_marks_are_cut_spaced_and_in_run_order) {
  const std::vector<gr::Mark> marks{
      {100, "flight", 1},   // 0: 80..130, within 2 s of the crash's 110..140: dropped
      {120, "crash", 1},    // 1: score 2, shot 110..140
      {300, "flight", 3},   // 2: score 3, shot 280..330, taken first
      {345, "flight", 1},   // 3: 325..375, within 2 s of 280..330: dropped
      {500, "gate", 9},     // 4: no rule
      {310, "crash", 0.5},  // 5: inside 280..330: covered
  };
  const gr::Cut cut = gr::plan_cut(marks, kRules, {2, 0, 0}, 10, 1000);
  CHECK(cut.shots.size() == 2);
  CHECK(cut.shots[0].mark == 1 && cut.shots[0].from == 110 && cut.shots[0].to == 140);
  CHECK(cut.shots[0].lens == "chase" && cut.shots[0].rate == 1 && cut.shots[0].score == 2);
  CHECK(cut.shots[1].mark == 2 && cut.shots[1].from == 280 && cut.shots[1].to == 330);
  CHECK(cut.covered == std::vector<size_t>({5}));
  CHECK(cut.dropped == std::vector<size_t>({0, 3}));
  CHECK(cut.unruled == std::vector<size_t>({4}));
  for (size_t i = 1; i < cut.shots.size(); i++) {
    CHECK(cut.shots[i].from >= cut.shots[i - 1].to + 20);
  }
}

TEST(the_reel_is_held_to_its_length_and_count) {
  const std::vector<gr::Mark> marks{{100, "crash", 1}, {400, "crash", 3}, {700, "crash", 2}};
  gr::Cut cut = gr::plan_cut(marks, kRules, {2, 6, 0}, 10, 1000);  // 3 s a shot, 6 s at most
  CHECK(cut.shots.size() == 2 && cut.shots[0].mark == 1 && cut.shots[1].mark == 2);
  CHECK(cut.dropped == std::vector<size_t>({0}));
  cut = gr::plan_cut(marks, kRules, {2, 0, 1}, 10, 1000);
  CHECK(cut.shots.size() == 1 && cut.shots[0].mark == 1);
  // At the run's edges a shot is held inside it.
  cut =
      gr::plan_cut(std::vector<gr::Mark>{{5, "crash", 1}, {995, "crash", 1}}, kRules, {}, 10, 1000);
  CHECK(cut.shots[0].from == 0 && cut.shots[1].to == 1000);
}

TEST(the_reel_clock_plays_each_shot_at_its_rate) {
  const std::vector<gr::Mark> marks{{100, "flight", 1}, {400, "crash", 1}};
  const gr::Cut cut = gr::plan_cut(marks, kRules, {}, 10, 1000);
  // The flight 80..130 at half speed is 10 s, the crash 390..420 is 3 s.
  CHECK(gr::reel_seconds(cut, 10) == 13);
  gr::ReelAt at = gr::reel_at(cut, 10, 0);
  CHECK(at.shot == 0 && at.step == 80 && !at.over);
  at = gr::reel_at(cut, 10, 4);
  CHECK(at.shot == 0 && at.step == 100);
  at = gr::reel_at(cut, 10, 10);
  CHECK(at.shot == 1 && at.step == 390);
  at = gr::reel_at(cut, 10, 11.5);
  CHECK(at.shot == 1 && at.step == 405);
  at = gr::reel_at(cut, 10, 99);
  CHECK(at.over && at.shot == 1 && at.step == 420);
  CHECK(gr::reel_at(gr::Cut{}, 10, 3).over);
}

TEST(bad_rules_are_named) {
  CHECK(gr::rules_problem(kRules).empty());
  CHECK(gr::rules_problem(std::vector<gr::ShotRule>{{"flight", 1, 1, 0, 1, ""}}).find("rate") !=
        std::string::npos);
  CHECK(gr::rules_problem(std::vector<gr::ShotRule>{{"Flight", 1, 1, 1, 1, ""}}).find("a-z") !=
        std::string::npos);
  CHECK(gr::rules_problem(std::vector<gr::ShotRule>{{"a", 1, 1, 1, 1, ""}, {"a", 1, 1, 1, 1, ""}})
            .find("two rules") != std::string::npos);
}
