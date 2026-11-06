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
