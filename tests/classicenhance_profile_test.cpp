// A classic game's content profile (plugins/classicenhance/profile.hpp): the first row is the one
// every reader uses by default and found by its id; its packs' names, its first pack's place in
// an install, and its asset kinds' names and paths agree both ways; a pack's assets of a kind are
// found sorted and nothing of another kind slips in; the palette is read from the profile's path
// and its absence names the path and the profile.
//
// ge:summary Tests the content profile: rows, pack names, asset kinds, the palette
// ge:keywords test classicenhance profile content-profile asset-kind pack-name palette
// ge:skill classicenhance testing
#include <string>
#include <utility>
#include <vector>

#include "check.hpp"
#include "classicenhance/profile.hpp"
#include "vfs/pak.hpp"

namespace ce = ge::classicenhance;

TEST(the_first_row_is_the_default_and_found_by_id) {
  const ce::Profile& p = ce::classic_profile();
  CHECK(&p == &ce::kProfiles[0]);
  CHECK(ce::find_profile(p.id) == &p);
  CHECK(ce::find_profile("no-such-game") == nullptr);
  CHECK(ce::profile_ids().find(std::string(p.id)) != std::string::npos);
  CHECK(!p.what.empty() && !p.base_dir.empty() && !p.palette.empty());
}

TEST(packs_are_numbered_and_the_first_sits_in_the_base_folder) {
  const ce::Profile& p = ce::classic_profile();
  CHECK(ce::pack_name(p, 0) == "pak0.pak");
  CHECK(ce::pack_name(p, 12) == "pak12.pak");
  CHECK(ce::first_pack_path(p) == std::string(p.base_dir) + "/pak0.pak");
}

TEST(an_asset_kind_names_its_files_both_ways) {
  const ce::Profile& p = ce::classic_profile();
  CHECK(ce::asset_name(p.maps, "maps/e1m1.bsp") == "e1m1");
  CHECK(ce::asset_path(p.maps, "e1m1") == "maps/e1m1.bsp");
  CHECK(ce::asset_name(p.sounds, "sound/doors/drclos4.wav") == "doors/drclos4");
  CHECK(ce::asset_name(p.maps, "maps/e1m1.lit").empty());  // another extension
  CHECK(ce::asset_name(p.maps, "gfx/e1m1.bsp").empty());   // another folder
  CHECK(ce::asset_name(p.maps, "maps/.bsp").empty());      // no name
  CHECK(ce::asset_name(p.models, "progs/s_light.spr").empty());
  CHECK(ce::asset_name(p.sprites, "progs/s_light.spr") == "s_light");
}

TEST(a_packs_assets_of_a_kind_are_found_sorted) {
  const ce::Profile& p = ce::classic_profile();
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files = {
      {"maps/b.bsp", {1}}, {"sound/x.wav", {2}}, {"maps/a.bsp", {3}}, {"maps/a.lit", {4}}};
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(ge::vfs::write_pak(files, &bytes, &error), error);
  ge::vfs::Pak pak;
  CHECK_MSG(ge::vfs::read_pak(bytes, &pak, &error), error);
  const std::vector<const ge::vfs::PakEntry*> maps = ce::assets_of(pak, p.maps);
  CHECK(maps.size() == 2);
  if (maps.size() == 2) {
    CHECK(maps[0]->path == "maps/a.bsp");
    CHECK(maps[1]->path == "maps/b.bsp");
  }
  CHECK(ce::assets_of(pak, p.sounds).size() == 1);
  CHECK(ce::assets_of(pak, p.models).empty());
}

TEST(the_palette_comes_from_the_profiles_path) {
  const ce::Profile& p = ce::classic_profile();
  std::vector<uint8_t> rgb(768);
  for (size_t i = 0; i < rgb.size(); i++) rgb[i] = uint8_t(i * 7);
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(ge::vfs::write_pak({{std::string(p.palette), rgb}}, &bytes, &error), error);
  ge::vfs::Pak pak;
  CHECK_MSG(ge::vfs::read_pak(bytes, &pak, &error), error);
  ce::Palette palette;
  CHECK_MSG(ce::read_palette(p, pak, bytes, &palette, &error), error);
  CHECK(palette.rgb[3] == 21 && palette.rgb[767] == uint8_t(767 * 7));

  std::vector<uint8_t> none;
  CHECK_MSG(ge::vfs::write_pak({{"gfx/other.lmp", rgb}}, &none, &error), error);
  CHECK_MSG(ge::vfs::read_pak(none, &pak, &error), error);
  CHECK(!ce::read_palette(p, pak, none, &palette, &error));
  CHECK(error.find(std::string(p.palette)) != std::string::npos);
  CHECK(error.find(std::string(p.id)) != std::string::npos);
}
