// AUTOTRACK (games/shooters_edge/client/demo_autotrack.hpp): the hottest player's cuts over a
// multiview recording. The kit's rating holds the worked numbers (red 200, the launcher and the
// quad with 15 frags is 2015; yellow 100 and the arc with 30 frags is 730; a fresh spawn's
// scattergun with 40 frags is 140). Over a minute's recorded team game of bots: the plan opens at
// the start on a player, cuts more than once, never twice within its gap, every cut names a slot
// that played, a cut for a death comes after the death, and the plan is the same every run.
//
// ge:summary Tests: autotrack's rating and its cuts over a recorded team game of bots
// ge:keywords test autotrack hottest-player streak cuts mvd team-game
// ge:skill shooter replay testing
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "check.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_autotrack.hpp"
#include "shooters_edge/client/demo_scan.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_team.hpp"

namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;

TEST(the_kit_rating_holds_the_worked_numbers) {
  // red armour 32768, launcher 32, quad 4194304
  CHECK(cl::autotrack_kit(32768 | 32 | 4194304 | 1, 200, 15) == 2015);
  // yellow 16384, arc 64
  CHECK(cl::autotrack_kit(16384 | 64 | 1, 100, 30) == 730);
  CHECK(cl::autotrack_kit(1 | 4096, 0, 40) == 140);
}

TEST(autotrack_cuts_a_recorded_team_game) {
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
  o.frames = 4615;
  o.seed = 2;
  const cn::TeamMatchReport r = cn::run_team_match(o);
  CHECK(r.ok);
  cl::Demo d;
  CHECK(cl::open_demo(r.mvd, d, error));
  cl::DemoScan scan;
  CHECK(cl::scan_demo(d, scan, error));
  cl::AutotrackPlan plan, again;
  const cl::AutotrackSettings set;
  CHECK(cl::autotrack_plan(d, scan, set, plan, error));
  CHECK(cl::autotrack_plan(d, scan, set, again, error));
  std::printf("autotrack: %zu cuts over %.1f s, %zu kills\n", plan.cuts.size(), plan.length_s,
              scan.kills.size());
  for (const cl::AutotrackCut& c : plan.cuts)
    std::printf("  %6.1f s  %-8s %-6s hot %.0f\n", c.t,
                scan.names[static_cast<size_t>(c.slot)].c_str(), c.why.c_str(), c.hot);
  CHECK(plan.cuts.size() >= 3);
  CHECK(!plan.cuts.empty() && plan.cuts[0].why == "start");
  CHECK(plan.cuts.size() == again.cuts.size());
  for (size_t i = 0; i < plan.cuts.size(); i++) {
    const cl::AutotrackCut& c = plan.cuts[i];
    CHECK(c.slot >= 0 && c.slot < 32 && (scan.players & (1u << c.slot)) != 0);
    if (i < again.cuts.size()) CHECK(again.cuts[i].slot == c.slot && again.cuts[i].t == c.t);
    if (i > 0) CHECK(c.t - plan.cuts[i - 1].t >= set.gap_s - 1e-9);
    if (c.why == "death" && i > 0) {
      bool died = false;
      for (const cl::DemoKill& k : scan.kills)
        died |= k.victim == plan.cuts[i - 1].slot && k.t <= c.t && k.t > plan.cuts[i - 1].t;
      CHECK(died);
    }
  }
  CHECK(cl::autotrack_at(plan, -1) == -1);
  CHECK(cl::autotrack_at(plan, plan.length_s) == plan.cuts.back().slot);
}
