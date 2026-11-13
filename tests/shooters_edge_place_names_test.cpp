// The window's place names (games/shooters_edge/client/place_names.hpp): places added where the
// player stands, removed nearest first, saved under the game folder in the shared format and read
// back by name and when the map starts; the same map again keeps unsaved edits; a name that would
// leave the folder refused; no game, no adding; a place reaches loc_dist (500 units in the
// sequel, any distance in the first game).
//
// ge:summary Tests the window's place names: added, removed, saved, read at map start
// ge:keywords test shooters-edge place-names addloc saveloc loadloc removeloc map_started loc_dist
// ge:skill shooter console testing
#include <filesystem>
#include <fstream>
#include <string>

#include "check.hpp"
#include "console/console.hpp"
#include "shooters_edge/client/place_names.hpp"

namespace cl = shooters_edge::client;
namespace fs = std::filesystem;

namespace {

std::string read_all(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

}  // namespace

TEST(places_are_made_in_the_game_saved_and_read_when_the_map_starts) {
  const fs::path game = fs::temp_directory_path() / "ge_se_place_names";
  fs::remove_all(game);
  ge::console::Console con;
  cl::PlaceNames p;
  p.game_dir = game;
  cl::SearchPath files;
  bool playing = false;
  ge::shooter::Vec3f here{0, 0, 0};
  cl::add_place_commands(con, p, cl::classic_adapter(), files, [&](ge::shooter::Vec3f& at) {
    at = here;
    return playing;
  });
  cl::map_started(p, cl::classic_adapter(), files, "dm2");
  con.execute("addloc quad");
  CHECK(p.locs.points.empty());  // no game playing
  playing = true;
  here = {100, 200, 24};
  con.execute("addloc quad");
  here = {-50, 0, 24};
  con.execute("locations_add \"$loc_name_ra room\"");
  CHECK(p.locs.points.size() == 2);
  con.execute("saveloc dm2");
  CHECK_MSG(read_all(game / "locs" / "dm2.loc") ==
                " 800 1600  192 quad\n-400    0  192 $loc_name_ra room\n",
            read_all(game / "locs" / "dm2.loc"));
  con.execute("saveloc ../x");
  CHECK(!fs::exists(game / "x.loc"));
  // The nearest goes.
  here = {90, 190, 24};
  con.execute("removeloc");
  CHECK(p.locs.points.size() == 1 && p.locs.points[0].name == "ra room");
  // The same map again keeps the edit; another map and back reads the saved file.
  cl::map_started(p, cl::classic_adapter(), files, "dm2");
  CHECK(p.locs.points.size() == 1);
  cl::map_started(p, cl::classic_adapter(), files, "dm4");
  CHECK(p.locs.points.empty());
  cl::map_started(p, cl::classic_adapter(), files, "dm2");
  CHECK(p.locs.points.size() == 2);
  con.execute("clearlocs");
  CHECK(p.locs.points.empty());
  con.execute("loadloc dm2");
  CHECK(p.locs.points.size() == 2);
  // tp_loadlocs 0: nothing read at a map's start.
  con.execute("tp_loadlocs 0");
  cl::map_started(p, cl::classic_adapter(), files, "dm4");
  cl::map_started(p, cl::classic_adapter(), files, "dm2");
  CHECK(p.locs.points.empty());
}

TEST(the_sequel_s_loc_sub_commands_edit_the_same_places) {
  const fs::path game = fs::temp_directory_path() / "ge_se_place_names_two";
  fs::remove_all(game);
  ge::console::Console con;
  cl::PlaceNames p;
  p.game_dir = game;
  cl::SearchPath files;
  ge::shooter::Vec3f here{0, 0, 0};
  cl::add_place_commands(con, p, cl::sequel_adapter(), files, [&](ge::shooter::Vec3f& at) {
    at = here;
    return true;
  });
  cl::map_started(p, cl::sequel_adapter(), files, "q2dm1");
  con.execute("loc add red armor");
  CHECK(p.locs.points.size() == 1 && p.locs.points[0].raw == "red armor");
  here = {10, 0, 0};
  con.execute("loc set mega health");
  CHECK(p.locs.points.size() == 1 && p.locs.points[0].raw == "mega health");
  con.execute("loc save");
  CHECK(fs::exists(game / "locs" / "q2dm1.loc"));
  // A place reaches 500 units in this game: farther off, there is none to rename or remove.
  CHECK(p.range != nullptr && p.range->value == "500");
  here = {600, 0, 0};
  con.execute("loc set far away");
  con.execute("loc del");
  CHECK(p.locs.points.size() == 1 && p.locs.points[0].raw == "mega health");
  con.execute("loc_dist 0");
  con.execute("loc del");
  CHECK(p.locs.points.empty());
}

TEST(a_classic_place_reaches_any_distance) {
  ge::console::Console con;
  cl::PlaceNames p;
  p.game_dir = fs::temp_directory_path() / "ge_se_place_names_three";
  cl::SearchPath files;
  cl::add_place_commands(con, p, cl::classic_adapter(), files, [](ge::shooter::Vec3f& at) {
    at = {9000, 0, 0};
    return true;
  });
  CHECK(p.range != nullptr && p.range->value == "0");
  p.locs.points.push_back({.name = "start", .at = {0, 0, 0}, .raw = "start"});
  con.execute("removeloc");
  CHECK(p.locs.points.empty());
}
