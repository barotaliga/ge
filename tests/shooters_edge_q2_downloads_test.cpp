// games/shooters_edge/net/q2_fetch.hpp, q2_downloads.hpp: what a Quake II client fetches on
// joining. The level's files stage by stage (the map and models; the models' skins and frames,
// sounds, pictures, figures and the sky; the map's wall textures), the paths a client may ask
// for, the looks it replaces, and the download switches by kind of file.
//
// ge:summary Test: Quake II's files fetched on joining, stage by stage, and the download switches
// ge:keywords test q2 download-order fetch-stages allow_download q2_fetch_level q2_fetch_textures
// ge:skill net shooter testing
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "morph/model.hpp"
#include "morph/write.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/net/q2_downloads.hpp"
#include "shooters_edge/net/q2_fetch.hpp"

namespace {

namespace br = ge::brush;
namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;

}  // namespace

TEST(joining_fetches_the_level_stage_by_stage) {
  // Stage 1 the map and the models (no brush or view-weapon names); stage 2 the skins and frames
  // the models name, the sounds (a `#` name whole, `*` ones the figures'), the pictures, each
  // figure's files (a bad look made "male/grunt") and the sky's six sides; stage 3 the map's walls.
  cl::DemoState s;
  s.models = {"",
              "maps/base1.bsp",
              "*1",
              "models/items/armor/body/tris.md2",
              "sprites/s_bfg1.sp2",
              "#w_shotgun.md2"};
  s.sounds = {"", "weapons/rocklf1a.wav", "*death1.wav", "#world/amb1.wav"};
  s.images = {"", "w_railgun", "/players/male/custom.pcx"};
  s.looks[0] = "female/jezebel";
  s.looks[1] = "bad look";
  s.sky = "unit1_";
  CHECK(cn::q2_fetch_models(s) ==
        std::vector<std::string>(
            {"maps/base1.bsp", "models/items/armor/body/tris.md2", "sprites/s_bfg1.sp2"}));
  ge::morph::Model model = ge::morph::sample_model();
  model.skin_paths = {"models/items/armor/body/skin.pcx"};
  ge::morph::Sprite sprite = ge::morph::sample_sprite();
  for (size_t k = 0; k < sprite.frames.size(); k++) {
    sprite.frames[k].picture = "sprites/s_bfg1_" + std::to_string(k) + ".pcx";
    sprite.frames[k].indices.clear();
  }
  const std::vector<uint8_t> md2 = ge::morph::write_model2(model);
  const std::vector<uint8_t> sp2 = ge::morph::write_sprite2(sprite);
  const cn::Q2FetchRead read = [&](const std::string& path) {
    return path.ends_with(".md2") ? md2 : path.ends_with(".sp2") ? sp2 : std::vector<uint8_t>{};
  };
  const std::vector<std::string> level = cn::q2_fetch_level(s, read);
  std::vector<std::string> want = {"models/items/armor/body/skin.pcx"};
  for (size_t k = 0; k < sprite.frames.size(); k++)
    want.push_back("sprites/s_bfg1_" + std::to_string(k) + ".pcx");
  for (const char* p : {"sound/weapons/rocklf1a.wav",
                        "world/amb1.wav",
                        "pics/w_railgun.pcx",
                        "players/male/custom.pcx",
                        "players/female/tris.md2",
                        "players/female/weapon.md2",
                        "players/female/w_shotgun.md2",
                        "players/female/weapon.pcx",
                        "players/female/jezebel.pcx",
                        "players/female/jezebel_i.pcx",
                        "players/female/death1.wav",
                        "players/male/tris.md2",
                        "players/male/weapon.md2",
                        "players/male/w_shotgun.md2",
                        "players/male/weapon.pcx",
                        "players/male/grunt.pcx",
                        "players/male/grunt_i.pcx",
                        "players/male/death1.wav",
                        "env/unit1_rt.tga",
                        "env/unit1_lf.tga",
                        "env/unit1_bk.tga",
                        "env/unit1_ft.tga",
                        "env/unit1_up.tga",
                        "env/unit1_dn.tga"})
    want.emplace_back(p);
  CHECK(level == want);
  CHECK(cn::q2_fetch_level(s, read, false).size() == want.size() - 2);  // cl_vwep 0
  // The walls: one `.wal` a texture the map's surfaces name.
  std::vector<uint8_t> map;
  std::string error;
  CHECK_MSG(br::write_bsp(br::sample_fixture(), br::Family::v38, &map, &error), error);
  br::Map m;
  CHECK(br::read_bsp(map, &m, &error));
  const std::vector<std::string> walls = cn::q2_fetch_textures(map);
  CHECK(!walls.empty() && walls.front() == "textures/" + m.textures.front().name + ".wal");
  CHECK(cn::q2_fetch_textures({}).empty());
  // The paths a client may ask for, and the looks it replaces.
  CHECK(cn::q2_fetch_path("Players\\Male/./Grunt.PCX") == "players/male/grunt.pcx");
  CHECK(!cn::q2_fetch_path_ok("grunt.pcx"));              // no folder
  CHECK(!cn::q2_fetch_path_ok("save/game.sav"));          // never saves
  CHECK(!cn::q2_fetch_path_ok("players/../config.cfg"));  // never up
  CHECK(!cn::q2_fetch_path_ok("maps/q2dm1.exe"));         // not a kind of file it fetches
  CHECK(cn::q2_fetch_path_ok("maps/q2dm1.bsp"));
  CHECK(cn::q2_fetch_look("female/bad skin") == "female/athena");
  CHECK(cn::q2_fetch_look("/viper") == "male/viper");
  CHECK(cn::q2_fetch_look("cyborg") == "male/grunt");
}

TEST(the_download_switches_gate_each_kind_of_file) {
  // allow_download and its kinds (net/q2_downloads.hpp): each folder its switch, the rest
  // `others` (off by default), and nothing at all once `allow_download` is 0 or -1.
  cn::Q2Downloads d;
  CHECK(cn::q2_download_allowed(d, "maps/q2dm1.bsp"));
  CHECK(cn::q2_download_allowed(d, "players/male/grunt.pcx"));
  CHECK(cn::q2_download_allowed(d, "sprites/s_bfg1.sp2"));
  CHECK(cn::q2_download_allowed(d, "env/unit1_up.pcx"));
  CHECK(!cn::q2_download_allowed(d, "locs/q2dm1.loc"));
  d.textures = false;
  CHECK(!cn::q2_download_allowed(d, "textures/e1u1/floor1_1.wal"));
  CHECK(!cn::q2_download_allowed(d, "env/unit1_up.pcx"));
  d.others = true;
  CHECK(cn::q2_download_allowed(d, "locs/q2dm1.loc"));
  for (const int all : {0, -1}) {
    d.all = all;
    CHECK(!cn::q2_download_allowed(d, "maps/q2dm1.bsp"));
  }
}
