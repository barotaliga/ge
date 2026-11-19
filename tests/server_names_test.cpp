// engine/server/names: sv_namelimit's sliding window refuses the rename past N in S seconds and
// allows it again once the oldest leaves the window; the settings read and refused; who is told.
//
// ge:summary Test: sv_namelimit's window, its settings, sv_shownames' audience
// ge:keywords test sv_namelimit sv_shownames rename_allowed name_settings
// ge:skill net testing
#include "check.hpp"
#include "server/names.hpp"

using namespace ge::server;

TEST(the_rename_past_the_limit_is_refused_until_the_window_slides) {
  const NameRule rule{3, 10, 0};
  NameWatch w;
  CHECK(rename_allowed(w, 0, rule));
  CHECK(rename_allowed(w, 1, rule));
  CHECK(rename_allowed(w, 2, rule));
  CHECK(!rename_allowed(w, 5, rule));    // a fourth inside 10 s
  CHECK(rename_allowed(w, 10.5, rule));  // the first has left the window
  CHECK(!rename_allowed(w, 10.6, rule));
  NameWatch free_watch;
  for (int i = 0; i < 40; i++) CHECK(rename_allowed(free_watch, i * 0.01, NameRule{0, 0, 0}));
}

TEST(the_settings_are_read_and_refused_out_of_range) {
  ServerConfig c;
  NameRule r;
  std::string error;
  CHECK(name_settings(c, r, error) && r.changes == 5 && r.window_s == 60 && r.show == 0);
  c.rules["sv_namelimit"] = "2 30";
  c.rules["sv_shownames"] = "1";
  CHECK(name_settings(c, r, error) && r.changes == 2 && r.window_s == 30 && r.show == 1);
  c.rules["sv_namelimit"] = "3/min";
  CHECK(name_settings(c, r, error) && r.changes == 3 && r.window_s == 60);
  c.rules["sv_namelimit"] = "4/10";
  CHECK(name_settings(c, r, error) && r.changes == 4 && r.window_s == 10);
  c.rules["sv_namelimit"] = "0";
  CHECK(name_settings(c, r, error) && r.changes == 0);
  c.rules["sv_namelimit"] = "two";
  CHECK(!name_settings(c, r, error) && error.find("sv_namelimit") != std::string::npos);
  CHECK(rename_told(NameRule{5, 60, 1}, true) && !rename_told(NameRule{5, 60, 1}, false));
  CHECK(rename_told(NameRule{5, 60, 2}, false) && !rename_told(NameRule{}, true));
}
