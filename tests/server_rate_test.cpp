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
  const RateBounds q2{5000, 5000, 100, 15000};         // another game's reading
  CHECK(rate_in_force("", {}, q2) == 5000 && rate_in_force("50000", {}, q2) == 15000);
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

TEST(a_client_on_the_lan_is_held_to_no_bound_when_sv_lanrate_is_on) {
  namespace gn = ge::net;
  CHECK(rate_on_lan(gn::ipv4(192, 168, 1, 5, 27910)) && rate_on_lan(gn::ipv4(10, 0, 0, 1, 1)));
  CHECK(rate_on_lan(gn::ipv4(172, 20, 0, 1, 1)) && !rate_on_lan(gn::ipv4(172, 32, 0, 1, 1)));
  CHECK(!rate_on_lan(gn::ipv4(8, 8, 8, 8, 1)));
  const RateLimits l{15000, 60000};
  const RateLimits lan = rate_limits_for(l, true, gn::ipv4(192, 168, 1, 5, 1));
  CHECK(lan.min == 0 && lan.max == 0);
  CHECK(rate_limits_for(l, false, gn::ipv4(192, 168, 1, 5, 1)).max == 60000);
  CHECK(rate_limits_for(l, true, gn::ipv4(8, 8, 8, 8, 1)).min == 15000);
  CHECK(rate_in_force("200000", lan) == 200000);
}
