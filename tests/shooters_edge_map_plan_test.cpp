// A MAP'S TEAM PLAN (games/shooters_edge/bots/map_plan.hpp), worked out from the fixture halls
// furnished for a team game: the key items become areas, a base is chosen among the areas a way
// leads to that hold no power, the quad's area is a rush with a lead, and the plan is the same
// every run. An item's worth orders the powers over the armours over the weapons.
//
// ge:summary Tests: a map's team plan from the halls: areas, a base, the quad's rush, the worths
// ge:keywords test map-plan areas base rush worth halls
// ge:skill bots testing
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "check.hpp"
#include "shooters_edge/bots/goals.hpp"
#include "shooters_edge/bots/map_plan.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_team.hpp"

namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;
namespace bt = shooters_edge::bots;

TEST(the_worths_order_the_powers_the_armours_and_the_weapons) {
  bt::ItemSpot quad, ra, ya, rl, ga;
  quad.kind = bt::ItemKind::quad;
  ra.kind = bt::ItemKind::armor3;
  ya.kind = bt::ItemKind::armor2;
  ga.kind = bt::ItemKind::armor1;
  rl.kind = bt::ItemKind::weapon;
  rl.weapon = 32;
  CHECK(bt::plan_worth(quad) > bt::plan_worth(ra));
  CHECK(bt::plan_worth(ra) > bt::plan_worth(rl));
  CHECK(bt::plan_worth(ra) > bt::plan_worth(ya) && bt::plan_worth(ya) > bt::plan_worth(ga));
}

TEST(the_halls_get_a_plan_with_a_base_and_a_rush) {
  std::vector<uint8_t> bsp;
  ge::progs::Progs progs;
  std::string error;
  CHECK(ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bsp, &error));
  CHECK(cl::baseline_progs(progs, error));
  cn::TeamMatchOptions o;
  o.map = bsp;
  o.map_name = "halls";
  o.entities = cn::kTeamHallsEntities;
  o.progs = &progs;
  o.frames = 400;
  o.record = false;
  const cn::TeamMatchReport a = cn::run_team_match(o);
  const cn::TeamMatchReport b = cn::run_team_match(o);
  CHECK(a.ok && b.ok);
  std::printf("%s", a.plan.c_str());
  CHECK(a.plan_areas >= 2);  // the halls are small: the hall's items one area, the quad's another
  CHECK(a.plan_base >= 0);
  CHECK(a.plan.find("rush") != std::string::npos && a.plan.find("quad") != std::string::npos);
  CHECK(a.plan == b.plan);
}

TEST(a_maps_goals_are_only_what_its_items_and_areas_allow) {
  // Two armours and a launcher, held in two areas; no power on the map.
  std::vector<bt::ItemSpot> items(3);
  items[0].kind = bt::ItemKind::armor3;
  items[1].kind = bt::ItemKind::armor2;
  items[2].kind = bt::ItemKind::weapon;
  items[2].weapon = 32;  // the rocket launcher's bit
  for (size_t i = 0; i < items.size(); i++) items[i].node = static_cast<int32_t>(i);
  bt::MapPlan plan;
  plan.areas.resize(2);
  plan.areas[0].name = "ra";
  plan.areas[0].items = {0, 2};
  plan.areas[0].entries = {5, 6};
  plan.areas[1].name = "ya";
  plan.areas[1].items = {1};
  plan.base = 0;
  const auto has = [](const std::vector<bt::Goal>& goals, bt::GoalKind k) {
    for (const bt::Goal& g : goals)
      if (g.kind == k) return true;
    return false;
  };
  std::vector<bt::Goal> goals = bt::map_goals(plan, items, "no-map");
  CHECK(has(goals, bt::GoalKind::take) && has(goals, bt::GoalKind::time));
  CHECK(has(goals, bt::GoalKind::hold) && has(goals, bt::GoalKind::retake));
  CHECK(has(goals, bt::GoalKind::arm));
  CHECK(!has(goals, bt::GoalKind::rush) && !has(goals, bt::GoalKind::escort));  // no power
  CHECK(!has(goals, bt::GoalKind::clear) && !has(goals, bt::GoalKind::drop));   // nor a quad run
  int spam = 0;
  for (const bt::Goal& g : goals) {
    spam += g.kind == bt::GoalKind::spam;
    CHECK(!(g.kind == bt::GoalKind::take && g.item == 2));  // a weapon is armed, not "taken"
    if (g.kind == bt::GoalKind::retake) CHECK((g.needs & bt::kNeedLost) != 0);
  }
  CHECK(spam == 2);  // one a way into the base

  // A quad on the map: its rush and escort, and the areas cleared on its run.
  items.push_back({});
  items.back().kind = bt::ItemKind::quad;
  items.back().node = 3;
  plan.areas.push_back({});
  plan.areas.back().name = "quad";
  plan.areas.back().items = {3};
  plan.areas.back().power = true;
  goals = bt::map_goals(plan, items, "no-map");
  CHECK(has(goals, bt::GoalKind::rush) && has(goals, bt::GoalKind::escort));
  CHECK(has(goals, bt::GoalKind::clear) && has(goals, bt::GoalKind::drop));
  for (const bt::Goal& g : goals)
    if (g.kind == bt::GoalKind::clear)
      CHECK((g.needs & bt::kNeedPower) != 0 && g.horizon == bt::Horizon::power);
  CHECK(!bt::goals_text(goals).empty());
}
