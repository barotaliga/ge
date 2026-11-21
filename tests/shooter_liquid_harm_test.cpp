// plugins/shooter/liquid_harm.hpp: lava and slime's classic ticks as the engine's harm rows. The
// amounts and clocks by liquid and suit; the engine's `step_harm` over the rows deals what a
// game's rules on the classic hazard clock deal (a tick when the clock has run out, the clock set
// again from that step), frame by frame on the 1/72 s host through a scripted dive (feet in,
// waist in, under, out and back), the clock starting again each time in; and the map's liquids laid
// by `add_liquids` carry the rows.
//
// ge:summary Test: lava and slime's classic ticks as harm rows, the engine's step against the clock
// ge:keywords test classic-harm lava slime suit tick interval step_harm dive hazard-clock
// ge:skill shooter water testing
#include <vector>

#include "check.hpp"
#include "shooter/liquid_harm.hpp"
#include "water/harm.hpp"

namespace {

namespace gw = ge::water;
namespace sh = ge::shooter;
constexpr double kDt = 1.0 / 72;

// The dive: a second at each level 1, 2, 3, half a second out, then a second under again.
std::vector<int> dive() {
  std::vector<int> levels;
  for (int level : {1, 2, 3})
    for (int f = 0; f < 72; f++) levels.push_back(level);
  for (int f = 0; f < 36; f++) levels.push_back(0);
  for (int f = 0; f < 72; f++) levels.push_back(3);
  return levels;
}

// A game's rules on the hazard clock, in the game's float seconds: a tick when the clock has run
// out (strictly before now), the clock set again from now, and zeroed on going in.
std::vector<double> rules_clock(int contents, bool suit) {
  std::vector<double> out;
  float finished = 0;
  float now = 1;  // the host's clock starts at 1 s
  bool in = false;
  for (int level : dive()) {
    const gw::Harm h = sh::classic_harm(contents, suit);
    double dealt = 0;
    if (level >= 1 && h.kind != gw::HarmKind::none && finished < now) {
      finished = now + static_cast<float>(h.interval);
      dealt = h.tick[level - 1];
    }
    if (level >= 1 && !in) finished = 0;  // going in starts the clock again
    in = level >= 1;
    out.push_back(dealt);
    now += static_cast<float>(kDt);
  }
  return out;
}

double sum(const std::vector<double>& v) {
  double s = 0;
  for (double x : v) s += x;
  return s;
}

}  // namespace

TEST(lava_and_slime_tick_by_the_level_on_their_clocks) {
  const gw::Harm lava = sh::classic_harm(sh::kContentsLava);
  CHECK(lava.kind == gw::HarmKind::heat);
  CHECK(lava.tick[0] == 10 && lava.tick[1] == 20 && lava.tick[2] == 30);
  CHECK(lava.interval == 0.2);
  CHECK(lava.contact == 0);                                        // the ticks alone, no heat
  CHECK(sh::classic_harm(sh::kContentsLava, true).interval == 1);  // the suit: once a second
  const gw::Harm slime = sh::classic_harm(sh::kContentsSlime);
  CHECK(slime.kind == gw::HarmKind::chemical);
  CHECK(slime.tick[0] == 4 && slime.tick[1] == 8 && slime.tick[2] == 12);
  CHECK(slime.interval == 1);
  CHECK(sh::classic_harm(sh::kContentsSlime, true).kind == gw::HarmKind::none);
  CHECK(sh::classic_harm(sh::kContentsWater).kind == gw::HarmKind::none);
}

TEST(the_engines_step_deals_what_the_classic_clock_deals_through_a_dive) {
  for (int contents : {sh::kContentsLava, sh::kContentsSlime}) {
    for (bool suit : {false, true}) {
      const std::vector<double> want = rules_clock(contents, suit);
      const gw::Harm h = sh::classic_harm(contents, suit);
      gw::HarmState st;
      const std::vector<int> levels = dive();
      std::vector<double> got;
      for (int level : levels) got.push_back(gw::step_harm(h, 20, level, st, kDt));
      CHECK(got.size() == want.size());
      int same = 0;
      for (size_t i = 0; i < want.size(); i++) same += got[i] == want[i] ? 1 : 0;
      CHECK(same == static_cast<int>(want.size()));  // frame by frame
    }
  }
  // The numbers the dive comes to: lava 5 ticks a second (10, 20, 30 by the level) and one more
  // each time in; slime one a second and one more each time in.
  const std::vector<double> lava = rules_clock(sh::kContentsLava, false);
  CHECK(sum(lava) == 6 * 10 + 5 * 20 + 5 * 30 + 6 * 30);
  const std::vector<double> slime = rules_clock(sh::kContentsSlime, false);
  CHECK(sum(slime) == 2 * 4 + 8 + 12 + 2 * 12);
}
