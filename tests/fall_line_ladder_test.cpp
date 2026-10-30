// games/fall_line/app/ladder.hpp: every row runs cheapest first from a stop that costs and is
// worth nothing; each stop sets what it says on the frame; the presets rise in worth and cost;
// the automatic preset keeps the top on a machine with room and lowers the resolution first on
// one without.
//
// ge:summary Tests for Fall Line's quality ladder: the rows, the stops, the presets, the fit
// ge:keywords test fall-line quality-ladder preset apply_picture auto_picture
// ge:skill render testing
#include <string>

#include "check.hpp"
#include "fall_line/app/ladder.hpp"

namespace {

namespace fa = fall_line::app;
namespace gr = ge::render;

}  // namespace

TEST(every_row_runs_cheapest_first_from_nothing) {
  const auto rows = fa::picture_rows();
  CHECK(static_cast<int>(rows.size()) == fa::kPictureRows);
  for (const gr::LadderRow& row : rows) {
    CHECK(row.stops.size() >= 2);
    CHECK(row.stops[0].cost_ms == 0 && row.stops[0].worth == 0);
    for (size_t s = 1; s < row.stops.size(); ++s) {
      CHECK(row.stops[s].cost_ms >= row.stops[s - 1].cost_ms);
      CHECK(row.stops[s].worth > row.stops[s - 1].worth);
    }
  }
  CHECK(rows[static_cast<size_t>(fa::PictureRow::resolution)].first == 1);
}

TEST(each_stop_sets_what_it_says_on_the_frame) {
  const fa::FrameOptions low = fa::apply_picture(fa::preset_pick(fa::Preset::low));
  CHECK(low.taa && low.scale == 0.5 && low.shadow_size == 1024);
  CHECK(low.clouds == 0 && low.bloom == 0);
  CHECK(low.reach_m == ge::render::clipmap_reach(fa::kGroundSpacing, 6));
  CHECK(low.snow && low.trails && low.trail_maps.fine_size == 1024);
  const fa::FrameOptions top = fa::apply_picture(fa::preset_pick(fa::Preset::top));
  CHECK(top.scale == 1.0 && top.shadow_size == 4096 && top.snow && top.trails && top.spray);
  CHECK(top.precip && top.fall.particles == 24576 && top.clouds == 1.0 && top.bloom > 0);
  CHECK(top.trail_maps.fine_size == 2048 && top.trail_maps.coarse_size == 2048);
  CHECK(top.reach_m == 0);  // the whole ground, unmisted
  gr::LadderPick none(static_cast<size_t>(fa::kPictureRows), 0);
  const fa::FrameOptions bare = fa::apply_picture(none);
  CHECK(!bare.snow && !bare.trails && !bare.spray && !bare.precip);
  CHECK(bare.reach_m == ge::render::clipmap_reach(fa::kGroundSpacing, 5));
}

TEST(the_presets_rise_in_worth_and_cost) {
  const auto rows = fa::picture_rows();
  double worth = -1, cost = -1;
  for (fa::Preset p : {fa::Preset::low, fa::Preset::medium, fa::Preset::high, fa::Preset::top}) {
    const gr::LadderPick pick = fa::preset_pick(p);
    CHECK(static_cast<int>(pick.size()) == fa::kPictureRows);
    const double w = gr::ladder_worth(rows, pick), c = gr::ladder_cost(rows, pick);
    CHECK(w > worth && c >= cost);
    worth = w;
    cost = c;
  }
}

TEST(the_automatic_preset_lowers_the_resolution_first) {
  const auto rows = fa::picture_rows();
  const gr::LadderPick top = fa::preset_pick(fa::Preset::top);
  const double top_ms = fa::price_table().floor_ms + gr::ladder_cost(rows, top);
  // A machine that drew the top at the table's own frame, with a budget to spare: the top.
  gr::LadderFit f = fa::auto_picture(top_ms * 1.5, top, top_ms);
  CHECK(f.fits && f.pick == top);
  // The same machine 3 % short: the resolution gives, nothing else.
  f = fa::auto_picture(top_ms * 0.97, top, top_ms);
  const size_t res = static_cast<size_t>(fa::PictureRow::resolution);
  CHECK(f.fits && f.pick[res] < top[res]);
  for (size_t i = 0; i < top.size(); ++i)
    if (i != res) CHECK(f.pick[i] == top[i]);
}

TEST(the_quality_setting_names_a_preset_a_share_or_a_budget) {
  const auto rows = fa::picture_rows();
  std::string error;
  // custom: no pick, the individual r_ settings rule.
  CHECK(!fa::quality_pick("custom", 8.3, 1920, 1080, error) && error.empty());
  CHECK(!fa::quality_pick("", 8.3, 1920, 1080, error) && error.empty());
  // The presets by name.
  for (fa::Preset p : {fa::Preset::low, fa::Preset::medium, fa::Preset::high, fa::Preset::top}) {
    const auto pick = fa::quality_pick(fa::preset_name(p), 8.3, 1920, 1080, error);
    CHECK(pick && *pick == fa::preset_pick(p));
  }
  // A word it does not know: no pick, and a line that says so.
  CHECK(!fa::quality_pick("ultra", 8.3, 1920, 1080, error) && !error.empty());
  error.clear();
  CHECK(!fa::quality_pick("1.5", 8.3, 1920, 1080, error) && !error.empty());
  error.clear();
  // 0 is every row's cheapest cost, 1 the top; the worth never falls as the share rises.
  const auto none = fa::quality_pick("0", 8.3, 1920, 1080, error);
  const auto all = fa::quality_pick("1", 8.3, 1920, 1080, error);
  CHECK(none && all && error.empty());
  if (!none || !all) return;
  CHECK(gr::ladder_cost(rows, *none) == 0);
  CHECK(*all == fa::preset_pick(fa::Preset::top));
  double last = -1;
  for (const char* share : {"0", "0.1", "0.25", "0.5", "0.75", "0.9", "1"}) {
    const auto pick = fa::quality_pick(share, 8.3, 1920, 1080, error);
    CHECK(pick && error.empty());
    if (!pick) continue;
    const double worth = gr::ladder_worth(rows, *pick);
    CHECK(worth >= last);
    last = worth;
  }
  // auto: a budget past the top's price draws the top; a smaller window fits more in the same.
  const auto roomy = fa::quality_pick("auto", 100, 1920, 1080, error);
  const auto tight = fa::quality_pick("auto", 3, 1920, 1080, error);
  const auto small = fa::quality_pick("auto", 3, 1280, 720, error);
  CHECK(roomy && tight && small);
  if (!roomy || !tight || !small) return;
  CHECK(*roomy == fa::preset_pick(fa::Preset::top));
  CHECK(gr::ladder_worth(rows, *small) >= gr::ladder_worth(rows, *tight));
  CHECK(gr::ladder_worth(rows, *tight) < gr::ladder_worth(rows, *roomy));
}
