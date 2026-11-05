// Who warped (games/shooters_edge/client/warps.hpp): a player who appears arrives; one who moves
// as far as anything runs or flies in a frame warps nowhere; one who jumps farther alive leaves
// his old place and arrives at the new one, dead only arrives (a respawn); the shown share runs
// 0 to 1 arriving and 1 to 0 leaving, and the warps end on time.
//
// ge:summary Tests who warped: arrivals, departures, respawns, the shown share and the ends
// ge:keywords test warps watch_warps warp_shown arrival_shown spawn respawn teleport
// ge:skill shooter testing
#include <vector>

#include "check.hpp"
#include "shooters_edge/client/warps.hpp"

namespace cl = shooters_edge::client;

TEST(an_appearing_player_arrives_and_a_running_one_does_not_warp) {
  cl::WarpWatch w;
  std::vector<cl::Warp> began;
  std::vector<cl::Body> bodies = {{1, {0, 0, 0}, true}};
  cl::watch_warps(w, bodies, 0, 0.016, began);
  CHECK(began.size() == 1 && began[0].arriving && began[0].entity == 1);
  CHECK(cl::arrival_shown(w, 1, 0) == 0);
  began.clear();
  // A rocket jump's frame at a low frame rate: 1000 units a second for a tenth of a second.
  bodies[0].origin = {100, 0, 0};
  cl::watch_warps(w, bodies, 0.1, 0.1, began);
  CHECK(began.empty());
  CHECK(cl::arrival_shown(w, 1, 0.3) > 0 && cl::arrival_shown(w, 1, 0.3) < 1);
  cl::watch_warps(w, bodies, cl::kArriveS + 0.01, 0.016, began);
  CHECK(w.live.empty() && cl::arrival_shown(w, 1, cl::kArriveS + 0.01) == 1);
}

TEST(a_teleport_leaves_and_arrives_and_a_respawn_only_arrives) {
  cl::WarpWatch w;
  std::vector<cl::Warp> began;
  std::vector<cl::Body> bodies = {{3, {0, 0, 0}, true}, {4, {50, 0, 0}, false}};
  cl::watch_warps(w, bodies, 0, 0.016, began);
  began.clear();
  bodies[0].origin = {800, 400, 64};  // alive: a teleport
  bodies[1].origin = {-900, 0, 0};    // dead: a respawn
  bodies[1].alive = true;
  cl::watch_warps(w, bodies, 1, 0.016, began);
  int left = 0, arrived = 0;
  for (const cl::Warp& v : began) {
    if (!v.arriving) {
      left++;
      CHECK(v.entity == 3 && v.at.x == 0 && v.at.y == 0);
    } else {
      arrived++;
      CHECK(v.entity == 3 ? v.at.x == 800 : v.at.x == -900);
    }
  }
  CHECK(left == 1 && arrived == 2);
  for (const cl::Warp& v : w.live) {
    const double start = cl::warp_shown(v, 1), mid = cl::warp_shown(v, 1 + 0.2);
    CHECK(v.arriving ? start == 0 && mid > 0 : start == 1 && mid < 1);
  }
  began.clear();
  cl::watch_warps(w, bodies, 1 + cl::kLeaveS + 0.01, 0.016, began);
  for (const cl::Warp& v : w.live) CHECK(v.arriving);
}
