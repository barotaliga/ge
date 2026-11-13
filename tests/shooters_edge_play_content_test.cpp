// client/play_content.hpp: the files a match on a map draws from, named after a headless start
// with bots: the map, the server's models and sounds, the basics and the pictures; a map the
// files lack refused.
//
// ge:summary Test: the files a match on a map needs, for the hub's Play page
// ge:keywords test play content files play_files map hub
// ge:skill shooter net testing
#include <algorithm>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "check.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/play_content.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "vfs/pak.hpp"

namespace cl = shooters_edge::client;

namespace {

ge::brush::Entity entity(std::initializer_list<std::pair<std::string, std::string>> pairs) {
  ge::brush::Entity e;
  for (const auto& p : pairs) e.pairs.push_back(p);
  return e;
}

// The fixture halls with four deathmatch spots and a rocket launcher, as `maps/halls.bsp`.
cl::SearchPath halls() {
  ge::brush::Fixture f = ge::brush::sample_fixture();
  for (const char* at : {"128 192 40", "400 96 40", "700 256 40", "544 700 40"})
    f.entities.push_back(
        entity({{"classname", "info_player_deathmatch"}, {"origin", at}, {"angle", "0"}}));
  f.entities.push_back(entity({{"classname", "weapon_rocketlauncher"}, {"origin", "128 192 0"}}));
  std::vector<uint8_t> bsp, pak;
  std::string error;
  cl::SearchPath files;
  if (!ge::brush::write_bsp(f, ge::brush::Family::v29, &bsp, &error) ||
      !ge::vfs::write_pak({{"maps/halls.bsp", bsp}}, &pak, &error) ||
      !cl::add_archive(files, "pak0.pak", std::move(pak), error))
    std::fprintf(stderr, "the fixture did not build: %s\n", error.c_str());
  return files;
}

bool has(const std::vector<std::string>& v, const std::string& s) {
  return std::find(v.begin(), v.end(), s) != v.end();
}

}  // namespace

TEST(a_match_on_a_map_names_its_files) {
  const cl::SearchPath files = halls();
  std::vector<std::string> names;
  std::string error;
  CHECK_MSG(cl::play_files(files, "halls", shooters_edge::net::listen_options(), names, error),
            error);
  CHECK(has(names, "maps/halls.bsp") && has(names, "maps/halls.lit"));
  CHECK(has(names, "gfx/palette.lmp") && has(names, "progs/player.mdl"));
  CHECK(has(names, "gfx.wad") && has(names, "gfx/"));
  CHECK(has(names, "progs/g_rock2.mdl"));  // the launcher on the floor, from the server's list
  CHECK(has(names, "progs/v_rock2.mdl"));  // and in hand
  bool sound = false;
  for (const std::string& n : names) sound = sound || n.rfind("sound/weapons/", 0) == 0;
  CHECK(sound);
  CHECK(std::is_sorted(names.begin(), names.end()));
  for (const std::string& n : names)
    CHECK_MSG(std::none_of(n.begin(), n.end(), [](char c) { return c >= 'A' && c <= 'Z'; }), n);
}

TEST(a_map_the_files_lack_is_refused) {
  const cl::SearchPath files = halls();
  std::vector<std::string> names;
  std::string error;
  CHECK(!cl::play_files(files, "nowhere", shooters_edge::net::listen_options(), names, error));
  CHECK_MSG(error.find("nowhere") != std::string::npos, error);
  CHECK(names.empty());
}

TEST(the_item_boxes_kept_as_maps_are_not_played) {
  CHECK(cl::map_playable("dm3") && cl::map_playable("e1m1") && cl::map_playable("halls"));
  CHECK(!cl::map_playable("b_batt0") && !cl::map_playable("b_shell1"));
}
