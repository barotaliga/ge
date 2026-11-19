// engine/server/limit: the value's syntax read and refused, the bucket's burst, its refill, a good
// try's token given back, and 0 meaning no limit.
//
// ge:summary Test: sv_rconrate and sv_passwordrate's bucket and syntax
// ge:keywords test sv_rconrate sv_passwordrate parse_limit limit_take
// ge:skill net testing
#include "check.hpp"
#include "server/limit.hpp"

using namespace ge::server;

TEST(the_value_is_read_in_every_form_and_refused_when_wrong) {
  TryLimit r;
  std::string error;
  CHECK(parse_limit("sv_rconrate", "10", r, error) && r.limit == 10 && r.period_s == 1 &&
        r.burst == 5);
  CHECK(parse_limit("sv_rconrate", "3/min*2", r, error) && r.limit == 3 && r.period_s == 60 &&
        r.burst == 2);
  CHECK(parse_limit("sv_rconrate", "1/10", r, error) && r.period_s == 10);
  CHECK(parse_limit("sv_rconrate", "0", r, error) && r.limit == 0);
  for (const char* bad : {"", "x", "1/week", "1*0", "-1", "1/0"})
    CHECK(!parse_limit("sv_rconrate", bad, r, error) && error.find("sv_rconrate") == 0);
  ServerConfig c;
  TryLimit rcon{10, 1, 10}, password{1, 1, 5};
  c.rules["sv_passwordrate"] = "2*3";
  CHECK(limit_settings(c, rcon, password, error) && rcon.limit == 10 && password.limit == 2 &&
        password.burst == 3);
}

TEST(the_bucket_empties_refills_and_takes_good_tries_back) {
  const TryLimit rule{1, 1, 3};
  TryBucket b;
  CHECK(limit_take(b, 0, rule) && limit_take(b, 0, rule) && limit_take(b, 0, rule));
  CHECK(!limit_take(b, 0.5, rule));  // the burst spent, half a token back
  CHECK(limit_take(b, 1.0, rule));   // a whole one now
  CHECK(!limit_take(b, 1.0, rule));
  limit_give(b, rule);  // that try was good
  CHECK(limit_take(b, 1.0, rule));
  CHECK(limit_take(b, 100, rule) && limit_take(b, 100, rule) && limit_take(b, 100, rule));
  CHECK(!limit_take(b, 100, rule));  // refilled to the burst, no further
  TryBucket open;
  for (int i = 0; i < 100; i++) CHECK(limit_take(open, 0, TryLimit{0, 1, 5}));
}
