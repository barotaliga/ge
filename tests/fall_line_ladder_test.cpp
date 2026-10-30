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

TEST(fall_lines_ladder_is_its_values_over_the_engines_setting) {
  const gr::QualityLadder& ladder = fa::quality_ladder();
  CHECK(ladder.rows.size() == fa::picture_rows().size() && ladder.table.width == 1920);
  // The presets in order, each reachable by its name; top is every row at its dearest.
  CHECK(ladder.presets.size() == 4);
  for (fa::Preset p : {fa::Preset::low, fa::Preset::medium, fa::Preset::high, fa::Preset::top}) {
    const auto pick = gr::preset_pick(ladder, fa::preset_name(p));
    CHECK(pick && *pick == fa::preset_pick(p));
    if (pick) CHECK(static_cast<int>(pick->size()) == fa::kPictureRows);
  }
  CHECK(fa::preset_pick(fa::Preset::top) == gr::top_pick(ladder.rows));
  // The game's word for it is the engine's over this ladder.
  std::string error;
  const auto half = fa::quality_pick("0.5", 8.3, 2560, 1440, error);
  const auto same = gr::quality_pick(ladder, "0.5", 8.3, 2560, 1440, error);
  CHECK(half && same && *half == *same && error.empty());
  CHECK(!fa::quality_pick("custom", 8.3, 2560, 1440, error));
}
