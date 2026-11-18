// engine/server/rate: a client's rate as the game reads it (none, zero, the network's range), held
// to sv_maxrate's cap and sv_minrate's floor (never above the cap), and the player told when
// the rate asked for is out of the server's bounds.
//
// ge:summary Test: a client's rate held to sv_minrate and sv_maxrate, and the player's line
// ge:keywords test rate sv_maxrate sv_minrate rate_in_force rate_verdict
// ge:skill net testing
#include "check.hpp"
#include "server/rate.hpp"

using namespace ge::server;

TEST(the_rate_in_force_is_the_games_reading_held_to_the_bounds) {
  CHECK(rate_in_force("", {}) == 99999);  // none named: the most
  CHECK(rate_in_force("0", {}) == 2500);
  CHECK(rate_in_force("100", {}) == 500);  // the network's range
  CHECK(rate_in_force("900000", {}) == 300000);
  CHECK(rate_in_force("25000", {0, 8000}) == 8000);
  CHECK(rate_in_force("", {0, 8000}) == 8000);
  CHECK(rate_in_force("2000", {4000, 0}) == 4000);
  CHECK(rate_in_force("2000", {9000, 8000}) == 8000);  // the floor never above the cap
  const RateBounds sequel{5000, 5000, 100, 15000};     // another game's reading
  CHECK(rate_in_force("", {}, sequel) == 5000 && rate_in_force("50000", {}, sequel) == 15000);
  CHECK(rate_floor({9000, 8000}) == 8000 && rate_floor({3000, 0}) == 3000);
}

TEST(a_rate_out_of_bounds_is_reported_and_set_back) {
  CHECK(!rate_verdict("", {4000, 8000}).out);  // none named: never told
  CHECK(!rate_verdict("6000", {4000, 8000}).out);
  const RateVerdict high = rate_verdict("25000", {0, 8000});
  CHECK(high.out && high.rate == 8000);
  CHECK(high.said == "\nYour rate setting is too high for this server.\nRate set to 8000\n");
  const RateVerdict low = rate_verdict("2000", {4000, 0});
  CHECK(low.out && low.rate == 4000 && low.said.find("too low") != std::string::npos);
}
