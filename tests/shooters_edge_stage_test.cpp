// The staged packs (games/shooters_edge/client/stage.hpp): `fixture` fills an empty search path
// with stand-in players and what a match throws, each a model file that reads back; it stages
// only what the path lacks, so a game folder's own model wins; an unknown name is refused with the
// names there are.
//
// ge:summary Tests the staged packs: the fixture's stand-ins, only what is missing, unknown names
// ge:keywords test stage-pak fixture stand-in player model search-path
// ge:skill shooter testing
#include <algorithm>
#include <string>
#include <vector>

#include "check.hpp"
#include "morph/read.hpp"
#include "morph/write.hpp"
#include "shooters_edge/client/stage.hpp"
#include "vfs/pak.hpp"

namespace cl = shooters_edge::client;

TEST(fixture_fills_an_empty_path_with_players_that_read) {
  cl::SearchPath path;
  std::vector<std::string> staged;
  std::string error;
  CHECK_MSG(cl::stage_pak(path, "fixture", &staged, error), error);
  CHECK(staged.size() == 7);
  std::vector<uint8_t> store;
  const auto bytes = cl::find_file(path, "progs/player.mdl", store);
  CHECK(!bytes.empty());
  ge::morph::Model m;
  CHECK_MSG(ge::morph::read_model(bytes, "progs/player.mdl", &m, &error), error);
  float lo = 1e9f, hi = -1e9f;  // the player's hull: 56 units tall, from 24 below its origin
  for (size_t i = 2; i < m.frames[0].positions.size(); i += 3) {
    lo = std::min(lo, m.frames[0].positions[i]);
    hi = std::max(hi, m.frames[0].positions[i]);
  }
  CHECK_MSG(lo < -23 && lo > -25 && hi > 31 && hi < 33,
            std::to_string(lo) + ".." + std::to_string(hi));
}

TEST(fixture_stages_only_what_the_path_lacks) {
  cl::SearchPath path;
  std::string error;
  std::vector<uint8_t> pak;
  const std::vector<uint8_t> own = ge::morph::write_model(ge::morph::sample_model());
  CHECK(ge::vfs::write_pak({{"progs/player.mdl", own}}, &pak, &error));
  CHECK(cl::add_archive(path, "pak0.pak", std::move(pak), error));
  std::vector<std::string> staged;
  CHECK(cl::stage_pak(path, "fixture", &staged, error));
  CHECK(staged.size() == 6);
  for (const std::string& f : staged) CHECK(f != "progs/player.mdl");
  std::vector<uint8_t> store;
  const auto found = cl::find_file(path, "progs/player.mdl", store);
  CHECK(std::vector<uint8_t>(found.begin(), found.end()) == own);  // the game's own still wins
  CHECK(cl::stage_pak(path, "fixture", &staged, error) && staged.empty());  // all there now
}

TEST(an_unknown_pack_is_refused_with_the_names_there_are) {
  cl::SearchPath path;
  std::string error;
  CHECK(!cl::stage_pak(path, "players", nullptr, error));
  CHECK_MSG(error.find("the staged packs: fixture") != std::string::npos, error);
  CHECK(path.archives.empty());
}
