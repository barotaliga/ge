// plugins/classic/server_sheet: a stock QuakeWorld server's info gives its sheet (a missing limit
// reads 0, the match mod's rules with their words, votes marked), the match mod's listings fill
// the rules the info leaves out, and a Quake II server's `info` lines give its sheet (fixed rows,
// a row a deathmatch flag; the match mod's votes and `settings` listing).
//
// ge:summary Test: a stock classic server's settings sheet from its info and listings
// ge:keywords test server-settings settings-sheet qw_sheet_from_info qw_match_listing_read
// ge:keywords q2_info_read q2_sheet_from_info q2_match_listing_read
// ge:skill classic net testing
#include "check.hpp"
#include "classic/server_sheet.hpp"

using namespace ge::classic;
namespace gs = ge::server;

namespace {

std::string value_of(const gs::SettingsSheet& s, std::string_view name) {
  const gs::SheetRow* r = gs::sheet_find(s, name);
  return r != nullptr ? r->value : std::string("<none>");
}

}  // namespace

TEST(a_plain_quakeworld_server_shows_its_info_fixed) {
  gs::SettingsSheet s;
  const std::string info = "\\maxfps\\77\\deathmatch\\3\\map\\dm3\\maxclients\\16\\hostname\\Arena";
  CHECK(qw_sheet_source(info, false) == SheetSource::qw_plain);
  qw_sheet_from_info(s, info);
  CHECK(s.whole);
  CHECK(value_of(s, "map") == "dm3" && value_of(s, "deathmatch") == "3");
  CHECK(value_of(s, "timelimit") == "0" && value_of(s, "fraglimit") == "0");  // left out: 0
  CHECK(gs::sheet_find(s, "mode") == nullptr);
  for (const gs::SheetRow& r : s.rows) CHECK(r.how == gs::SheetHow::fixed);
}

TEST(the_match_mod_shows_its_mode_and_its_words) {
  gs::SettingsSheet s;
  const std::string info =
      "\\maxfps\\77\\deathmatch\\3\\map\\dm2\\timelimit\\20\\teamplay\\2\\ktxver\\1.48\\mode\\4on4-"
      "midair\\sv_antilag\\2";
  CHECK(qw_sheet_source(info, false) == SheetSource::qw_match);
  CHECK(qw_mode_base("4on4-midair") == "4on4" && qw_mode_has("4on4-midair", "midair"));
  CHECK(!qw_mode_has("4on4-midair", "ra") && !qw_mode_has("ffa", "ffa"));
  qw_sheet_from_info(s, info);
  CHECK(value_of(s, "mode") == "4on4" && value_of(s, "midair") == "on");
  CHECK(value_of(s, "antilag") == "on");
  const gs::SheetRow* tl = gs::sheet_find(s, "timelimit");
  CHECK(tl != nullptr && tl->how == gs::SheetHow::direct);
  CHECK(gs::sheet_change_lines(*tl, "22") == std::vector<std::string>({"timeup1", "timeup1"}));
  const gs::SheetRow* dm = gs::sheet_find(s, "deathmatch");
  CHECK(gs::sheet_change_lines(*dm, "4") == std::vector<std::string>({"dmm4"}));
  const gs::SheetRow* mode = gs::sheet_find(s, "mode");
  CHECK(gs::sheet_change_lines(*mode, "1on1") == std::vector<std::string>({"1on1"}));
  const gs::SheetRow* map = gs::sheet_find(s, "map");
  CHECK(map->how == gs::SheetHow::vote &&
        gs::sheet_change_lines(*map, "e1m2")[0] == "votemap e1m2");
  const gs::SheetRow* lag = gs::sheet_find(s, "antilag");
  CHECK(lag->how == gs::SheetHow::vote && gs::sheet_change_lines(*lag, "off")[0] == "antilag");
  // A switch not yet read turns with one press.
  const gs::SheetRow* dis = gs::sheet_find(s, "discharge");
  CHECK(dis->value.empty() &&
        gs::sheet_change_lines(*dis, "on") == std::vector<std::string>({"discharge"}));
}

TEST(the_match_mods_listings_fill_the_rest) {
  gs::SettingsSheet s;
  qw_sheet_from_info(s, "\\ktxver\\1.48\\mode\\2on2\\map\\dm4");
  // Coloured labels (bit 7 set) as the listing prints them.
  std::string label = "Discharge";
  for (char& c : label) c = static_cast<char>(c | 0x80);
  CHECK(qw_match_listing_read(s, "Powerups      qp  " + label + "     on\n"));
  CHECK(value_of(s, "powerups") == "on" && value_of(s, "discharge") == "on");
  CHECK(qw_match_listing_read(s, "Drop Quad     off Drop Ring     on"));
  CHECK(value_of(s, "dropquad") == "off" && value_of(s, "dropring") == "on");
  CHECK(qw_match_listing_read(s, "Fair Backpacks lst Drop Backpacks on"));
  CHECK(value_of(s, "fairpacks") == "lst" && value_of(s, "droppack") == "on");
  CHECK(qw_match_listing_read(s, "KTX2 respawns") && value_of(s, "spawn") == "5");
  CHECK(qw_match_listing_read(s, "Overtime: sudden death") && value_of(s, "overtime") == "2");
  CHECK(qw_match_listing_read(s, "Spectalk: off") && value_of(s, "spectalk") == "off");
  CHECK(qw_match_listing_read(s, "Server locking: team") && value_of(s, "lockmode") == "team");
  CHECK(!qw_match_listing_read(s, "Ann: hello there"));
  CHECK(qw_match_listing_asks() == std::vector<std::string>({"status1", "status2"}));
}

TEST(a_quake_ii_server_shows_its_info_and_flags) {
  std::pair<std::string, std::string> kv;
  CHECK(q2_info_read("dmflags             1040\n", kv) && kv.first == "dmflags" &&
        kv.second == "1040");
  CHECK(!q2_info_read("Ann: hello", kv) && !q2_info_read("   indented            x", kv));
  std::vector<std::pair<std::string, std::string>> info = {{"mapname", "q2dm1"},
                                                           {"deathmatch", "1"},
                                                           {"dmflags", "1040"},
                                                           {"fraglimit", "25"},
                                                           {"gamename", "baseq2"}};
  CHECK(q2_sheet_source(info) == SheetSource::q2_plain);
  gs::SettingsSheet s;
  q2_sheet_from_info(s, info);
  CHECK(value_of(s, "map") == "q2dm1" && value_of(s, "fraglimit") == "25");
  CHECK(value_of(s, "timelimit") == "0");
  CHECK(value_of(s, "dmflags:16") == "on" && value_of(s, "dmflags:1024") == "on");
  CHECK(value_of(s, "dmflags:4") == "off");
  for (const gs::SheetRow& r : s.rows) CHECK(r.how == gs::SheetHow::fixed);
}

TEST(quake_iis_match_mod_votes_and_lists_its_settings) {
  std::vector<std::pair<std::string, std::string>> info = {
      {"mapname", "q2dm1"}, {"gamename", "OpenTDM"}, {"match_type", "TDM"}};
  CHECK(q2_sheet_source(info) == SheetSource::q2_match);
  gs::SettingsSheet s;
  q2_sheet_from_info(s, info);
  CHECK(value_of(s, "mode") == "tdm");
  CHECK(q2_match_listing_read(s, "Timelimit: 15 minutes"));
  CHECK(value_of(s, "timelimit") == "15");
  const gs::SheetRow* tl = gs::sheet_find(s, "timelimit");
  CHECK(tl->how == gs::SheetHow::vote &&
        gs::sheet_change_lines(*tl, "20")[0] == "vote timelimit 20");
  CHECK(q2_match_listing_read(s, "Weapon switch: faster") && value_of(s, "switchmode") == "fast");
  CHECK(!q2_match_listing_read(s, "Ann: gg"));
}
