// games/shooters_edge/app/ladder.hpp and look.hpp's stops: every row runs from its switch off to
// its top, cheapest first; each stop sets what it says on the renderer and the cloud's march; the
// presets rise; r_quality picks the stops and `custom` leaves the switches and levels as set; the
// cvars reach the picture.
//
// ge:summary Tests for Shooter's Edge's quality ladder: the rows, the stops, the presets, r_quality
// ge:keywords test shooter quality-ladder stops preset picture_stops ce_shadows_quality r_quality
// ge:skill render testing
#include <string>

#include "check.hpp"
#include "console/console.hpp"
#include "shooters_edge/app/ladder.hpp"
#include "shooters_edge/app/look.hpp"

namespace {

namespace lk = shooters_edge::look;
namespace gr = ge::render;
namespace ba = ge::brush::app;

}  // namespace

TEST(every_row_runs_from_off_to_its_top_cheapest_first) {
  const auto rows = lk::ladder_rows();
  CHECK(static_cast<int>(rows.size()) == lk::kLadderRows);
  for (const gr::LadderRow& row : rows) {
    CHECK(row.stops.size() == 4);  // off, low, medium, high
    CHECK(row.stops[0].cost_ms == 0 && row.stops[0].worth == 0);
    for (size_t s = 1; s < row.stops.size(); ++s) {
      CHECK(row.stops[s].cost_ms >= row.stops[s - 1].cost_ms);
      CHECK(row.stops[s].worth > row.stops[s - 1].worth);
    }
  }
}

TEST(the_presets_rise_and_top_is_today_s_look) {
  const gr::QualityLadder& ladder = lk::quality_ladder();
  double cost = -1, worth = -1;
  for (const gr::LadderPreset& p : ladder.presets) {
    const gr::LadderPick pick(p.stops.begin(), p.stops.end());
    CHECK(gr::ladder_cost(ladder.rows, pick) >= cost);
    CHECK(gr::ladder_worth(ladder.rows, pick) > worth);
    cost = gr::ladder_cost(ladder.rows, pick);
    worth = gr::ladder_worth(ladder.rows, pick);
  }
  const auto top = gr::preset_pick(ladder, "top");
  CHECK(top.has_value() && *top == gr::top_pick(ladder.rows));
  // Every stop of a preset is on: a preset never turns a switch off.
  for (const gr::LadderPreset& p : ladder.presets)
    for (const int s : p.stops) CHECK(s > 0);
}

TEST(a_pick_is_its_stops_and_back) {
  const lk::Stops s{lk::Stop::low, lk::Stop::high, lk::Stop::off};
  const lk::Stops back = lk::stops_of(lk::pick_of(s));
  CHECK(back.shadows == s.shadows && back.water == s.water && back.skies == s.skies);
}

TEST(custom_keeps_each_switch_and_its_level) {
  lk::Picture p;
  lk::Stops s = lk::custom_stops(p);
  CHECK(s.shadows == lk::Stop::high && s.water == lk::Stop::high && s.skies == lk::Stop::high);
  p.ce.water = false;
  p.shadows_quality = "medium";
  p.skies_quality = "fuzzy";  // a word it does not know: high
  s = lk::custom_stops(p);
  CHECK(s.shadows == lk::Stop::medium && s.water == lk::Stop::off && s.skies == lk::Stop::high);
  // A level with its switch off stays off.
  p.ce.shadows = false;
  CHECK(lk::custom_stops(p).shadows == lk::Stop::off);
}

TEST(r_quality_picks_the_stops_and_custom_leaves_them) {
  lk::Picture p;
  p.ce.water = false;
  std::string error;
  CHECK(lk::picture_stops(p, 2560, 1440, error).water == lk::Stop::off);  // custom
  p.quality = "low";
  const lk::Stops low = lk::picture_stops(p, 2560, 1440, error);
  CHECK(low.shadows == lk::Stop::low && low.water == lk::Stop::low && low.skies == lk::Stop::low);
  // The switches as drawn follow the pick, the player's own left as they were.
  const ge::classicenhance::Switches on = lk::drawn_switches(p.ce, low);
  CHECK(on.water && on.shadows && on.skies && !p.ce.water);
  p.quality = "top";
  const lk::Stops top = lk::picture_stops(p, 2560, 1440, error);
  CHECK(top.shadows == lk::Stop::high && top.water == lk::Stop::high &&
        top.skies == lk::Stop::high);
  CHECK(error.empty());
  p.quality = "shiny";
  CHECK(lk::picture_stops(p, 2560, 1440, error).water == lk::Stop::off);  // the switches rule
  CHECK(error.find("r_quality \"shiny\"") != std::string::npos);
}

TEST(auto_fits_the_budget_and_a_tight_one_lowers_the_rows) {
  lk::Picture p;
  p.quality = "auto";
  std::string error;
  const gr::PriceTable& t = lk::quality_ladder().table;
  p.budget_ms = 100;
  const lk::Stops roomy = lk::picture_stops(p, t.width, t.height, error);
  CHECK(roomy.shadows == lk::Stop::high && roomy.water == lk::Stop::high &&
        roomy.skies == lk::Stop::high);
  p.budget_ms = t.floor_ms + 0.01;
  const lk::Stops tight = lk::picture_stops(p, t.width, t.height, error);
  CHECK(tight.shadows < lk::Stop::high && tight.water < lk::Stop::high);
}

TEST(the_kept_stops_follow_r_quality_and_the_switches) {
  lk::Picture p;
  lk::KeptStops kept;
  std::string refused;
  CHECK(lk::picture_stops(p, 800, 600, kept, refused).water == lk::Stop::high);
  p.water_quality = "low";  // custom: read again every frame
  CHECK(lk::picture_stops(p, 800, 600, kept, refused).water == lk::Stop::low);
  p.quality = "medium";
  CHECK(lk::picture_stops(p, 800, 600, kept, refused).water == lk::Stop::medium);
  p.quality = "nope";
  CHECK(lk::picture_stops(p, 800, 600, kept, refused).water == lk::Stop::low);
  CHECK(!refused.empty());
  refused.clear();
  lk::picture_stops(p, 800, 600, kept, refused);  // said once, not every frame
  CHECK(refused.empty());
}

TEST(each_shadow_stop_sets_the_casters) {
  const lk::Picture p;
  const auto at = [&](lk::Stop s) {
    return lk::world_options(lk::Look::modern, {}, p, {s, lk::Stop::high, lk::Stop::high});
  };
  const ba::Options off = at(lk::Stop::off), low = at(lk::Stop::low), med = at(lk::Stop::medium),
                    high = at(lk::Stop::high);
  CHECK(off.shadowed == 0);
  CHECK(low.shadowed > 0 && low.caster_lights == 0 && !low.capsules);
  CHECK(med.caster_lights == ba::kCasterLights && !med.soft_casters && !med.capsules);
  CHECK(high.caster_lights == ba::kCasterLights && high.soft_casters && high.capsules);
  CHECK(high.shadowed == ba::Options{}.shadowed);
}

TEST(each_sky_stop_sets_the_pack_sky_the_cloud_and_the_far_land) {
  const lk::Picture p;
  CHECK(lk::world_options(lk::Look::modern, {}, p, {lk::Stop::high, lk::Stop::high, lk::Stop::off})
            .pack_sky);
  CHECK(!lk::world_options(lk::Look::modern, {}, p, {lk::Stop::high, lk::Stop::high, lk::Stop::low})
             .pack_sky);
  CHECK(!lk::cloud_at(lk::Stop::off) && !lk::cloud_at(lk::Stop::low));
  CHECK(lk::cloud_at(lk::Stop::medium) && lk::cloud_at(lk::Stop::high));
  CHECK(!lk::far_land_at(lk::Stop::medium) && lk::far_land_at(lk::Stop::high));
}

TEST(the_cvars_reach_the_picture) {
  ge::console::Console con;
  lk::Picture p;
  lk::add_cvars(con, p);
  con.execute("ce_water_quality medium; ce_shadows_quality low; r_quality auto; r_budget 6");
  CHECK(p.water_quality == "medium" && p.shadows_quality == "low" && p.skies_quality == "high");
  CHECK(p.quality == "auto" && p.budget_ms == 6);
}
