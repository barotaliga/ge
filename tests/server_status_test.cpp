// engine/server/status: the uptime in both forms, the settings read and refused.
//
// ge:summary Test: sv_uptime's forms, sv_status and sv_statusrate read
// ge:keywords test sv_status sv_statusrate sv_uptime uptime_text status_settings
// ge:skill net testing
#include "check.hpp"
#include "server/status.hpp"

using namespace ge::server;

TEST(the_uptime_reads_short_and_in_words) {
  CHECK(uptime_text(65, 1) == "01.05");
  CHECK(uptime_text(3 * 3600 + 7 * 60 + 9, 1) == "3:07.09");
  CHECK(uptime_text(2 * 86400 + 3600 + 5, 1) == "2+1:00.05");
  CHECK(uptime_text(65, 2) == "1 min, 5 secs");
  CHECK(uptime_text(86400 + 2 * 3600 + 3 * 60 + 4, 2) == "1 day, 2 hours, 3 mins, 4 secs");
  CHECK(uptime_text(65, 0).empty());
}

TEST(the_settings_are_read_and_refused) {
  ServerConfig c;
  StatusRule r;
  std::string error;
  CHECK(status_settings(c, r, error) && r.show == 2 && r.uptime == 0 && r.rate.limit == 0);
  c.rules["sv_status"] = "1";
  c.rules["sv_uptime"] = "2";
  c.rules["sv_statusrate"] = "15";
  CHECK(status_settings(c, r, error) && r.show == 1 && r.uptime == 2 && r.rate.limit == 15);
  c.rules["sv_status"] = "3";
  CHECK(!status_settings(c, r, error) && error.find("sv_status \"3\"") == 0);
}
