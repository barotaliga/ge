// engine/render/ladder.hpp: the fit keeps every row at its top when the budget allows; the exact
// fit finds the dear stop a greedy walk would hide; a smaller budget never buys more worth; the
// rows given up first go first, in their order; a free stop is always kept; a slower machine
// fits less; and a picture over every budget comes back as the cheapest, marked as not fitting.
// The quality setting over any game's ladder: custom picks nothing, a preset by its name, a share
// from 0 (every row's cheapest) to 1 (the top) whose worth never falls, auto inside a budget and
// scaled by the window's pixels, and a word it does not know refused with a line naming the
// ladder's own presets.
//
// ge:summary Tests for the quality ladder's fit: exact, monotone, the first rows, the scale
// ge:keywords test render quality-ladder fit_ladder knapsack machine_scale quality_pick
// ge:keywords r_quality presets quality-setting
// ge:skill render testing
#include <cmath>
#include <string>
#include <vector>

#include "check.hpp"
#include "render/ladder.hpp"

namespace {

namespace gr = ge::render;

// SHADOWS: a cheap step that shows little in front of a dear one that matters.
constexpr gr::LadderStop kShadows[] = {{"off", 0, 0}, {"figures", 2.0, 10}, {"all", 2.5, 40}};
constexpr gr::LadderStop kTrails[] = {{"off", 0, 0}, {"low", 1.0, 30}, {"high", 2.0, 36}};
constexpr gr::LadderStop kBloom[] = {{"off", 0, 0}, {"on", 0, 3}};
constexpr gr::LadderStop kAa[] = {{"off", 0, 0}, {"taa", 1.0, 2}};
constexpr gr::LadderStop kScale[] = {{"half", 0, 0}, {"three-quarters", 1.5, 3}, {"full", 3, 5}};

std::vector<gr::LadderRow> rows() {
  return {{"shadows", kShadows},
          {"trails", kTrails},
          {"bloom", kBloom},
          {"aa", kAa, 1},
          {"scale", kScale, 2}};
}

// A made-up game's whole ladder: its rows, a table measured at 1000 x 1000 and two presets.
const std::vector<gr::LadderRow> kGameRows = rows();
constexpr int kLowStops[] = {0, 1, 0, 0, 0};
constexpr int kHighStops[] = {2, 1, 1, 1, 1};
constexpr gr::LadderPreset kGamePresets[] = {{"low", kLowStops}, {"high", kHighStops}};
const gr::QualityLadder kGame{kGameRows, {"test machine", 1000, 1000, 5}, kGamePresets};

}  // namespace

TEST(a_budget_that_holds_the_top_keeps_every_row_there) {
  const auto r = rows();
  const gr::LadderFit f = gr::fit_ladder(r, 20, 5);
  CHECK(f.fits);
  CHECK((f.pick == gr::LadderPick{2, 2, 1, 1, 2}));
  CHECK(std::fabs(f.cost_ms - 8.5) < 1e-9);
  CHECK(std::fabs(f.worth - 86) < 1e-9);
}

TEST(the_first_rows_go_first_in_their_order) {
  const auto r = rows();
  // 1 ms short of the top: the anti-aliasing goes, nothing else.
  gr::LadderFit f = gr::fit_ladder(r, 12.5, 5);
  CHECK(f.fits && (f.pick == gr::LadderPick{2, 2, 1, 0, 2}));
  // 2.5 ms short: the anti-aliasing, then the scale a stop.
  f = gr::fit_ladder(r, 11, 5);
  CHECK(f.fits && (f.pick == gr::LadderPick{2, 2, 1, 0, 1}));
  // 4 ms short: both first rows at their bottoms, the rest still whole.
  f = gr::fit_ladder(r, 9.5, 5);
  CHECK(f.fits && (f.pick == gr::LadderPick{2, 2, 1, 0, 0}));
}

TEST(the_exact_fit_finds_the_dear_stop_a_greedy_walk_hides) {
  const auto r = rows();
  // 3.5 ms over the floor with the first rows at their bottoms: shadows' "all" (2.5) and trails'
  // "low" (1) are worth 70; trails' "high" with shadows' "figures" (4) does not fit, and "figures"
  // with "low" (3) is worth only 40.
  const gr::LadderFit f = gr::fit_ladder(r, 8.5, 5);
  CHECK(f.fits);
  CHECK((f.pick == gr::LadderPick{2, 1, 1, 0, 0}));
  CHECK(std::fabs(f.worth - 73) < 1e-9);
}

TEST(a_smaller_budget_never_buys_more_worth_and_a_free_stop_is_kept) {
  const auto r = rows();
  double last = 1e9;
  for (int k = 0; k <= 300; ++k) {
    const double budget = 20 - 0.05 * k;
    const gr::LadderFit f = gr::fit_ladder(r, budget, 5);
    CHECK(f.fits);
    CHECK(f.worth <= last + 1e-9);
    CHECK(f.cost_ms <= budget - 5 + 1e-9);
    CHECK(f.pick[2] == 1);  // the bloom costs nothing
    last = f.worth;
  }
}

TEST(a_slower_machine_fits_less_and_the_scale_reads_it) {
  const auto r = rows();
  const gr::LadderPick top = {2, 2, 1, 1, 2};
  // A machine that drew the top picture in 27 ms where the table says 13.5 is twice as slow.
  const double scale = gr::machine_scale(r, top, 5, 27);
  CHECK(std::fabs(scale - 2) < 1e-12);
  const gr::LadderFit here = gr::fit_ladder(r, 16, 5);
  const gr::LadderFit there = gr::fit_ladder(r, 16, 5, scale);
  CHECK(here.fits && there.fits);
  CHECK(there.worth < here.worth);
  CHECK(there.cost_ms <= 16 - 10 + 1e-9);
}

TEST(a_budget_under_the_floor_gives_the_cheapest_picture_unfit) {
  const auto r = rows();
  const gr::LadderFit f = gr::fit_ladder(r, 4, 5);
  CHECK(!f.fits);
  CHECK((f.pick == gr::LadderPick{0, 0, 0, 0, 0}));
}

TEST(the_quality_setting_reads_any_games_ladder) {
  std::string error;
  // custom: nothing, the game's own settings rule.
  CHECK(!gr::quality_pick(kGame, "custom", 10, 1000, 1000, error) && error.empty());
  CHECK(!gr::quality_pick(kGame, "", 10, 1000, 1000, error) && error.empty());
  // The game's presets by name; a name it does not have is refused, the line naming its own.
  const auto low = gr::quality_pick(kGame, "low", 10, 1000, 1000, error);
  CHECK(low && (*low == gr::LadderPick{0, 1, 0, 0, 0}) && error.empty());
  CHECK(!gr::quality_pick(kGame, "medium", 10, 1000, 1000, error));
  CHECK(error.find("low, high") != std::string::npos);
  error.clear();
  CHECK(!gr::quality_pick(kGame, "1.5", 10, 1000, 1000, error) && !error.empty());
  error.clear();
  // 0 is every row's cheapest, 1 the top, and the worth never falls between.
  const auto none = gr::quality_pick(kGame, "0", 10, 1000, 1000, error);
  const auto all = gr::quality_pick(kGame, "1", 10, 1000, 1000, error);
  CHECK(none && all && error.empty());
  if (!none || !all) return;
  CHECK(gr::ladder_cost(kGameRows, *none) == 0);
  CHECK(*all == gr::top_pick(kGameRows));
  double last = -1;
  for (const char* share : {"0", "0.2", "0.4", "0.6", "0.8", "1"}) {
    const auto pick = gr::quality_pick(kGame, share, 10, 1000, 1000, error);
    CHECK(pick);
    if (!pick) continue;
    CHECK(gr::ladder_worth(kGameRows, *pick) >= last);
    last = gr::ladder_worth(kGameRows, *pick);
  }
  // auto: the fit inside the budget at the table's size; a window of four times the pixels fits
  // the same budget as a quarter of it.
  const auto roomy = gr::quality_pick(kGame, "auto", 20, 1000, 1000, error);
  const auto big = gr::quality_pick(kGame, "auto", 20, 2000, 2000, error);
  const auto quarter = gr::quality_pick(kGame, "auto", 5, 1000, 1000, error);
  CHECK(roomy && big && quarter);
  if (!roomy || !big || !quarter) return;
  CHECK(*roomy == gr::top_pick(kGameRows));
  CHECK(*big == *quarter);
  CHECK(gr::ladder_worth(kGameRows, *big) < gr::ladder_worth(kGameRows, *roomy));
}

TEST(the_automatic_fit_reads_the_machine_from_one_timed_frame) {
  // A machine that drew the top in twice the table's time fits as the reference at half the budget.
  const gr::LadderPick top = gr::top_pick(kGameRows);
  const double table_top = kGame.table.floor_ms + gr::ladder_cost(kGameRows, top);
  const gr::LadderFit slow = gr::auto_fit(kGame, 20, top, 2 * table_top);
  const gr::LadderFit half = gr::fit_ladder(kGameRows, 10, kGame.table.floor_ms);
  CHECK(slow.pick == half.pick);
}
