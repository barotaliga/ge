// The content scan: the fixture set's shipped sounds and models, every sound and model its maps
// name with where, the numbered sets they pick (the world's music track left out), the asset map
// read (comments, pins, refusals) and the scan held to it.
//
// ge:summary Test: the content scan over the fixture set, the asset map's parse and the match
// ge:keywords test content scan assets noise model sounds worldtype assetmap pin parse match
// ge:skill brush free-assets
#include <string>
#include <vector>

#include "brush/content.hpp"
#include "check.hpp"
#include "vfs/pak.hpp"

namespace {

namespace br = ge::brush;

br::ContentScan scanned(br::ContentFixture* fx) {
  std::string error;
  CHECK(br::content_fixture(fx, &error));
  ge::vfs::Pak pak;
  CHECK(ge::vfs::read_pak(fx->pak, &pak, &error));
  std::vector<br::ContentFile> files;
  files.reserve(pak.entries.size());
  for (const ge::vfs::PakEntry& e : pak.entries) {
    files.push_back({e.path, ge::vfs::pak_file(fx->pak, e)});
  }
  return br::scan_content(files);
}

const br::ScannedAsset* find(const br::ContentScan& s, br::AssetKind kind, const std::string& n) {
  for (const br::ScannedAsset& a : s.assets) {
    if (a.kind == kind && a.name == n) return &a;
  }
  return nullptr;
}

TEST(the_scan_finds_what_is_shipped_and_what_the_maps_name) {
  br::ContentFixture fx;
  const br::ContentScan s = scanned(&fx);
  CHECK(s.maps.size() == 2 && s.maps[0] == "maps/hall.bsp" && s.maps[1] == "maps/side.bsp");
  CHECK(s.unread.empty());

  const br::ScannedAsset* slide = find(s, br::AssetKind::sound, "doors/slide.wav");
  CHECK(slide && slide->shipped && slide->uses.size() == 1);
  CHECK(slide->uses[0].map == "maps/side.bsp" && slide->uses[0].key == "noise1");
  const br::ScannedAsset* gong = find(s, br::AssetKind::sound, "misc/gong.wav");
  CHECK(gong && !gong->shipped && gong->uses[0].classname == "trigger_once");
  const br::ScannedAsset* crate = find(s, br::AssetKind::model, "progs/crate.mdl");
  CHECK(crate && crate->shipped && crate->uses.size() == 1);
  const br::ScannedAsset* box = find(s, br::AssetKind::model, "maps/b_box.bsp");
  CHECK(box && !box->shipped);
  // An inline brush model is the map's own.
  CHECK(!find(s, br::AssetKind::model, "*1"));
  // Sounds come first, then models, each by name.
  CHECK(s.assets.size() == 5 && s.assets[0].kind == br::AssetKind::sound &&
        s.assets.back().kind == br::AssetKind::model);
}

TEST(the_scan_lists_the_numbered_sets_but_not_the_music_track) {
  br::ContentFixture fx;
  const br::ContentScan s = scanned(&fx);
  // func_button 1, func_door 2 (both maps), func_plat 1, worldspawn worldtype 1.
  CHECK(s.sets.size() == 4);
  CHECK(s.sets[1].classname == "func_door" && s.sets[1].value == "2" && s.sets[1].maps.size() == 2);
  CHECK(s.sets[3].classname == "worldspawn" && s.sets[3].key == "worldtype");
  for (const br::SetUse& u : s.sets) CHECK(u.key != "sounds" || u.classname != "worldspawn");
}

TEST(the_asset_map_reads_and_the_scan_is_held_to_it) {
  br::ContentFixture fx;
  const br::ContentScan s = scanned(&fx);
  br::AssetMap map;
  std::string error;
  CHECK(br::parse_asset_map(fx.asset_map, &map, &error));
  CHECK(map.lines.size() == 6 && map.lines[1].pinned && !map.lines[0].pinned);
  CHECK(map.lines[3].set && map.lines[3].key == "sounds" && map.lines[3].value == "2" &&
        map.lines[3].field == "noise1");

  const br::ContentMatch m = br::compare_content(s, map);
  int mapped = 0;
  for (const std::string& o : m.asset_ours) mapped += o.empty() ? 0 : 1;
  CHECK(mapped == 3);  // slide, hum, crate; gong and the box are not
  CHECK(m.set_ours[1] == "noise1=door/slide_stop noise2=door/slide");
  CHECK(m.set_ours[0].empty() && m.set_ours[2].empty());
  // The world's type is mapped by the rows of the classes that pick by it.
  CHECK(m.set_ours[3] == "item_box.noise=pickup/box");

  CHECK(!br::parse_asset_map("sound a\n", &map, &error));
  CHECK(error.find("line 1") != std::string::npos);
  CHECK(!br::parse_asset_map("\nset func_door sounds noise1 x\n", &map, &error));
  CHECK(error.find("line 2") != std::string::npos);
  CHECK(!br::parse_asset_map("music a b\n", &map, &error));
  CHECK(br::parse_asset_map("sound a#b c  # a note\n", &map, &error));
  CHECK(map.lines.size() == 1 && map.lines[0].classic == "a#b" && map.lines[0].ours == "c");
}

}  // namespace
