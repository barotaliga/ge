// plugins/shooter/settings: the shooter's server settings by name. Every setting reads back the
// default it was made with; a value is set by name, refused outside its range or when not a number
// (the error names the setting and the range); the live ones copy without touching the loop's; the
// server info carries only the pm_ switches that differ from the defaults.
//
// ge:summary Tests: the shooter's server settings table, set by name, ranges, server info
// ge:keywords test server-settings set_setting setting_text changed_info pm_ktjump sv_mintic
// ge:skill shooter testing
#include <cstring>
#include <string>

#include "check.hpp"
#include "shooter/settings.hpp"

namespace sh = ge::shooter;

TEST(every_setting_reads_back_its_default_and_names_are_unique) {
  int count = 0;
  const sh::Setting* table = sh::settings_table(count);
  CHECK(count >= 20);
  const sh::ServerSettings defaults;
  for (int i = 0; i < count; i++) {
    sh::ServerSettings set;
    std::string error;
    CHECK(sh::set_setting(set, table[i].name, sh::setting_text(table[i], defaults), error));
    CHECK(table[i].get(set) == table[i].get(defaults));
    CHECK(table[i].low <= table[i].get(defaults) && table[i].get(defaults) <= table[i].high);
    CHECK(sh::find_setting(table[i].name) == &table[i]);
    for (int j = i + 1; j < count; j++) CHECK(std::strcmp(table[i].name, table[j].name) != 0);
  }
  CHECK(sh::setting_text(*sh::find_setting("sv_mintic"), defaults) == "0.013");
  CHECK(sh::setting_text(*sh::find_setting("sv_gravity"), defaults) == "800");
  CHECK(sh::setting_text(*sh::find_setting("pm_slidefix"), defaults) == "0");
}

TEST(a_value_is_set_by_name_and_refused_out_of_range) {
  sh::ServerSettings set;
  std::string error;
  CHECK(sh::set_setting(set, "sv_mintic", "0.01", error));
  CHECK(set.mintic == 0.01);
  CHECK(sh::set_setting(set, "pm_ktjump", "0", error));
  CHECK(set.move.ktjump == 0);
  CHECK(sh::set_setting(set, "sv_speedcheck", "0", error));
  CHECK(!set.speedcheck);
  CHECK(sh::set_setting(set, "sv_gravity", "400", error));
  CHECK(set.move.gravity == 400);

  CHECK(!sh::set_setting(set, "sv_mintic", "0.5", error));
  CHECK(error.find("sv_mintic") != std::string::npos);
  CHECK(error.find("0.001..0.1") != std::string::npos);
  CHECK(error.find("next:") != std::string::npos);
  CHECK(set.mintic == 0.01);
  CHECK(!sh::set_setting(set, "pm_airstep", "2", error));
  CHECK(error.find("0 or 1") != std::string::npos);
  CHECK(!sh::set_setting(set, "sv_maxspeed", "fast", error));
  CHECK(error.find("not a number") != std::string::npos);
  CHECK(!sh::set_setting(set, "sv_nonsense", "1", error));
  CHECK(error.find("not a shooter server setting") != std::string::npos);
}

TEST(live_settings_copy_and_the_loops_stay) {
  sh::ServerSettings from, to;
  from.move.gravity = 100;
  from.move.airstep = true;
  from.mintic = 0.05;
  from.speedcheck = false;
  sh::apply_live_settings(to, from);
  CHECK(to.move.gravity == 100);
  CHECK(to.move.airstep);
  CHECK(to.mintic == 0.013);
  CHECK(to.speedcheck);
}

TEST(the_server_info_names_only_the_changed_switches) {
  sh::ServerSettings set;
  // pm_speedjump is always told, so a client knows whether the server runs the easy jump itself.
  const auto told = sh::changed_info(set);
  CHECK(told.size() == 1 && told[0].first == "pm_speedjump" && told[0].second == "0");
  set.move.gravity = 100;  // sent with the connection's own data, not the server info
  CHECK(sh::changed_info(set).size() == 1);
  set.move.ktjump = 0;
  set.move.slidefix = true;
  const auto info = sh::changed_info(set);
  CHECK(info.size() == 3);
  CHECK(info[0].first == "pm_ktjump" && info[0].second == "0");
  CHECK(info[1].first == "pm_slidefix" && info[1].second == "1");
  CHECK(info[2].first == "pm_speedjump");
}
