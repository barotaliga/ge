// games/shooters_edge/client/demo_content.hpp: the files a demo draws from: its map, the models
// and sounds its lists name (the map's inline models left out), the basics every picture needs,
// lower case and sorted; the same names from the recording and from its replay file; a file that
// is not a demo refused; the sequel's: its maps, models' folders and pictures, and a map's
// textures.
//
// ge:summary Test: a demo's content names: map, precached models and sounds, basics, refusals
// ge:keywords test demo content files precache models sounds content-pack demo_files
// ge:skill shooter net testing
#include "shooters_edge/client/demo_content.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "check.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/client/sequel_demo.hpp"

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

TEST(a_sequel_demo_names_its_map_model_folders_and_pictures) {
  const std::vector<uint8_t> rec = se::sequel_fixture(2, 4);
  std::vector<std::string> names;
  std::string error;
  CHECK_MSG(se::demo_files(rec, names, error), error);
  CHECK(holds(names, "maps/yard.bsp") && !holds(names, "maps/yard.lit"));
  CHECK(holds(names, "models/things/thing1/"));  // the model's folder: it and its skins
  CHECK(holds(names, "pics/colormap.pcx") && !holds(names, "gfx/palette.lmp"));
  for (const std::string& n : names) CHECK(n.find("tris.md2") == std::string::npos);
}

TEST(a_sequel_map_names_its_texture_files) {
  // A version 38 map (the brush plugin's fixture).
  std::vector<uint8_t> bsp;
  std::string error;
  CHECK_MSG(ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v38, &bsp, &error),
            error);
  std::vector<std::string> more;
  se::content_needs("maps/yard.bsp", bsp, more);
  CHECK(!more.empty());
  for (const std::string& m : more) CHECK(m.rfind("textures/", 0) == 0 && m.ends_with(".wal"));
  more.clear();
  se::content_needs("sound/a.wav", bsp, more);  // only a map names more
  CHECK(more.empty());
}
