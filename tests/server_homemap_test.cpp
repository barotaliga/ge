// engine/server/homemap: an empty server goes back to its home map after ten seconds, once; a
// player arriving starts the wait again; no home map, or already on it, never.
//
// ge:summary Test: the home map an empty server goes back to
// ge:keywords test sv_homemap home-map empty-server home_due
// ge:skill net testing
#include "check.hpp"
#include "server/homemap.hpp"

using namespace ge::server;

TEST(an_empty_server_goes_home_after_ten_seconds) {
  HomeWatch w;
  CHECK(!home_due(w, "dm3", "e1m2", 1, 0));
  CHECK(!home_due(w, "dm3", "e1m2", 0, 5));  // the last one left at 5 s
  CHECK(!home_due(w, "dm3", "e1m2", 0, 14));
  CHECK(!home_due(w, "dm3", "e1m2", 1, 14.5));  // someone came: the wait starts again
  CHECK(!home_due(w, "dm3", "e1m2", 0, 15));
  CHECK(!home_due(w, "dm3", "e1m2", 0, 24.9));
  CHECK(home_due(w, "dm3", "e1m2", 0, 25));
  CHECK(!home_due(w, "dm3", "e1m2", 0, 25.1));  // asked once
  HomeWatch home, none;
  CHECK(!home_due(home, "dm3", "dm3", 0, 0) && !home_due(home, "dm3", "dm3", 0, 100));
  CHECK(!home_due(none, "", "e1m2", 0, 0) && !home_due(none, "", "e1m2", 0, 100));
}
