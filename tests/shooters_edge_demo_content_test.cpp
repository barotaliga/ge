// games/shooters_edge/client/demo_content.hpp: the files a demo draws from: its map, the models
// and sounds its lists name (the map's inline models left out), the basics every picture needs,
// lower case and sorted; the same names from the recording and from its replay file; a file that
// is not a demo refused.
//
// ge:summary Test: a demo's content names: map, precached models and sounds, basics, refusals
// ge:keywords test demo content files precache models sounds content-pack demo_files
// ge:skill shooter net testing
#include "shooters_edge/client/demo_content.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/demo_write.hpp"

namespace se = shooters_edge::client;

namespace {

bool holds(const std::vector<std::string>& names, const std::string& n) {
  return std::find(names.begin(), names.end(), n) != names.end();
}

}  // namespace

TEST(a_demo_names_its_map_models_sounds_and_the_basics) {
  const std::vector<uint8_t> rec = se::write_multiview_match(se::highlight_match("Tower"));
  std::vector<std::string> names;
  std::string error;
  CHECK_MSG(se::demo_files(rec, names, error), error);
  CHECK(std::is_sorted(names.begin(), names.end()));
  CHECK(std::adjacent_find(names.begin(), names.end()) == names.end());
  CHECK(holds(names, "maps/tower.bsp") && holds(names, "maps/tower.lit"));  // lower case
  CHECK(holds(names, "gfx/palette.lmp") && holds(names, "progs/player.mdl"));
  CHECK(holds(names, "particles/"));
  for (const std::string& n : names) {
    CHECK_MSG(n.find('*') == std::string::npos, n);  // no inline model
    CHECK_MSG(std::none_of(n.begin(), n.end(), [](char c) { return c >= 'A' && c <= 'Z'; }), n);
  }
  std::string list;
  for (const std::string& n : names) list += n + "\n";
  CHECK_MSG(names.size() > 12, list);
}

TEST(the_replay_file_names_the_same_files) {
  const std::vector<uint8_t> rec = se::write_multiview_match(se::highlight_match());
  std::vector<uint8_t> file;
  std::string error;
  CHECK(se::pack_demo(rec, file, error));
  std::vector<std::string> plain, packed;
  CHECK(se::demo_files(rec, plain, error));
  CHECK_MSG(se::demo_files(file, packed, error), error);
  CHECK(plain == packed);
}

TEST(a_file_that_is_no_demo_is_refused) {
  const std::vector<uint8_t> junk = {'n', 'o', 't', ' ', 'a', ' ', 'd', 'e', 'm', 'o'};
  std::vector<std::string> names;
  std::string error;
  CHECK(!se::demo_files(junk, names, error));
  CHECK(!error.empty());
}
