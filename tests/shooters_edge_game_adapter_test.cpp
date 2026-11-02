// The first classic game's adapter (games/shooters_edge/client/game_adapter.hpp): its mods are the
// folders beside the base that hold archives or a program; each mode finds its program through a
// mod's folder over the base, passing over a program of the other mode's layout and a file of a
// form this build has no runner for, each with its reason; a map's info and textures are read off
// its file (the pictures the same views `read_map_pictures` gives, so the drawn map is unchanged);
// the demos are listed and found by a bare name with the game's extensions; a model is read into
// its form with a picture beside it standing in for a skin; a sound is read from `sound/`; the
// menus' pictures are the packs' with a PNG or TGA of a picture's name standing in for it; a
// config file in the packs runs through `exec` after the player's own; a map's place names are
// read from its loc file.
//
// ge:summary Tests the first game's adapter: mods, programs by form, map info, textures, demos
// ge:keywords test shooters-edge game-adapter list_mods find_program program-form map_info
// ge:keywords map_textures list_demos find_demo qwprogs open_model open_sound replacement-skin
// ge:keywords pictures replacement-picture find_config read_configs_from open_locations
// ge:skill shooter testing
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "assets/image.hpp"
#include "assets/tga.hpp"
#include "audio/wav.hpp"
#include "brush/pictures.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classicenhance/profile.hpp"
#include "console/console.hpp"
#include "morph/write.hpp"
#include "progs/assembler.hpp"
#include "progs/layout.hpp"
#include "shooter/locs.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/menu/classic_art.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "vfs/pak.hpp"

namespace cl = shooters_edge::client;
namespace pf = shooters_edge::profiles;
namespace pg = ge::progs;
namespace fs = std::filesystem;
namespace as = ge::assets;

namespace {

void write(const fs::path& p, const std::vector<uint8_t>& b) {
  fs::create_directories(p.parent_path());
  std::ofstream f(p, std::ios::binary);
  f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}

std::vector<uint8_t> program(const pg::Layout& form) { return pg::Assembler(form).bytes(); }

std::string base_dir() { return std::string(ge::classicenhance::classic_profile().base_dir); }

}  // namespace

TEST(the_mods_are_the_folders_beside_the_base_that_hold_a_program_or_an_archive) {
  const fs::path root = fs::temp_directory_path() / "ge_se_game_adapter_mods";
  fs::remove_all(root);
  write(root / base_dir() / "progs.dat", program(pg::kSinglePlayerLayout));
  write(root / "arena" / "qwprogs.dat", program(pg::kMultiplayerLayout));
  std::vector<uint8_t> pak;
  std::string error;
  CHECK(ge::vfs::write_pak({{"maps/x.bsp", {1, 2, 3}}}, &pak, &error));
  write(root / "maps_only" / "pak0.pak", pak);
  write(root / "notes" / "readme.txt", {'h', 'i'});  // no archive, no program: not a mod
  const cl::GameAdapter& a = cl::classic_adapter();
  const std::vector<cl::ModFolder> mods = a.list_mods(a, root.string());
  CHECK(mods.size() == 2);
  if (mods.size() == 2) {
    CHECK(mods[0].folder == "arena" && mods[0].archives == 0);
    CHECK(mods[0].programs.size() == 1 && mods[0].programs[0] == "qwprogs.dat");
    CHECK(mods[1].folder == "maps_only" && mods[1].archives == 1 && mods[1].programs.empty());
  }
  fs::remove_all(root);
}

TEST(each_mode_finds_its_program_through_the_mod_folder) {
  const fs::path root = fs::temp_directory_path() / "ge_se_game_adapter_programs";
  fs::remove_all(root);
  write(root / base_dir() / "progs.dat", program(pg::kSinglePlayerLayout));  // the campaign
  write(root / "mod" / "qwprogs.dat", program(pg::kMultiplayerLayout));
  const cl::GameAdapter& a = cl::adapter_of(pf::classic_game());
  cl::SearchPath sp;
  std::string error, why;
  CHECK_MSG(cl::open_game_dir(root.string(), "mod", sp, error), error);
  cl::FoundProgram p;
  CHECK_MSG(a.find_program(a, sp, false, p, why), why);
  CHECK(p.file.name == "qwprogs.dat" && p.file.form == pf::ProgramForm::qc);
  CHECK(p.file.side == pf::ProgramSide::server && !cl::single_player(p.progs));
  CHECK_MSG(a.find_program(a, sp, true, p, why), why);
  CHECK(p.file.name == "progs.dat" && cl::single_player(p.progs));

  // The base game alone: its progs.dat is the single-player form, so multiplayer finds none.
  cl::SearchPath only_base;
  CHECK_MSG(cl::open_game_dir(root.string(), base_dir(), only_base, error), error);
  CHECK(!a.find_program(a, only_base, false, p, why));
  CHECK_MSG(why.find("progs.dat is a single-player program") != std::string::npos, why);

  // A file that is no program is passed over with its reason.
  write(root / "mod" / "qwprogs.dat", {1, 2, 3});
  cl::SearchPath broken;
  CHECK_MSG(cl::open_game_dir(root.string(), "mod", broken, error), error);
  CHECK(!a.find_program(a, broken, false, p, why));
  CHECK_MSG(why.find("qwprogs.dat did not load") != std::string::npos, why);

  // A game whose multiplayer program is of a form with no runner: named, never read as QC.
  static constexpr pf::ProgramFile kLibrary[] = {
      {.name = "qwprogs.dat", .form = pf::ProgramForm::game_library}};
  pf::GameProfile other = pf::classic_game();
  other.multiplayer_programs = kLibrary;
  cl::GameAdapter b = a;
  b.game = &other;
  write(root / "mod" / "qwprogs.dat", program(pg::kMultiplayerLayout));
  cl::SearchPath lib;
  CHECK_MSG(cl::open_game_dir(root.string(), "mod", lib, error), error);
  CHECK(!b.find_program(b, lib, false, p, why));
  CHECK_MSG(why.find("native game library program, which this build has no runner for") !=
                std::string::npos,
            why);
  CHECK(cl::runs_form(pf::ProgramForm::qc) && !cl::runs_form(pf::ProgramForm::sandboxed_bytecode));
  fs::remove_all(root);
}

TEST(a_maps_info_and_textures_are_read_off_its_file) {
  std::vector<uint8_t> map;
  std::string error;
  CHECK_MSG(ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &map, &error),
            error);
  std::vector<uint8_t> pak;
  CHECK(ge::vfs::write_pak({{"maps/room.bsp", map}}, &pak, &error));
  cl::SearchPath files;
  CHECK_MSG(cl::add_archive(files, "pak0.pak", std::move(pak), error), error);
  const cl::GameAdapter& a = cl::classic_adapter();
  const std::vector<std::string> maps = a.list_maps(a, files);
  CHECK(maps.size() == 1 && maps[0] == "room");
  cl::MapInfo info;
  CHECK_MSG(a.map_info(a, files, "room", info, error), error);
  CHECK(info.name == "room" && info.title == ge::brush::sample_fixture().message);
  CHECK(info.family == ge::brush::Family::v29 && info.textures > 0);
  CHECK(!a.map_info(a, files, "nowhere", info, error));
  CHECK(error.find("maps/nowhere.bsp") != std::string::npos);

  std::vector<uint8_t> store;
  const std::vector<ge::brush::MapPicture> pics = a.map_textures(a, files, "room", map, store);
  CHECK(pics.size() == info.textures);
  const std::vector<ge::brush::MapPicture> direct = ge::brush::read_map_pictures(map);
  CHECK(pics.size() == direct.size());
  for (size_t i = 0; i < pics.size() && i < direct.size(); ++i) {
    CHECK(pics[i].name == direct[i].name && pics[i].width == 64 && pics[i].height == 64);
    CHECK(pics[i].level[0] == direct[i].level[0] && pics[i].level[3] != nullptr);
  }
  // A TGA in the game's files stands in for a texture by its name, alpha and all; a map's own
  // folder first.
  as::Image tga;
  tga.width = 4;
  tga.height = 2;
  tga.rgba.assign(size_t{4} * 2 * 4, 9);
  std::vector<uint8_t> tga_pak;
  as::Image only_map = tga;
  only_map.rgba.assign(size_t{4} * 2 * 4, 77);
  CHECK(ge::vfs::write_pak({{"textures/" + direct[0].name + ".tga", as::encode_tga(tga)},
                            {"textures/room/" + direct[0].name + ".tga", as::encode_tga(only_map)},
                            {"textures/" + direct[1].name + ".tga", {1, 2, 3}}},
                           &tga_pak, &error));
  CHECK_MSG(cl::add_archive(files, "pak1.pak", std::move(tga_pak), error), error);
  const std::vector<ge::brush::MapPicture> swapped = a.map_textures(a, files, "room", map, store);
  CHECK(swapped[0].rgba_width == 4 && swapped[0].rgba_height == 2);
  CHECK(swapped[0].rgba == only_map.rgba && swapped[0].level[0] == direct[0].level[0]);
  CHECK(swapped[1].rgba.empty());  // a file that does not read: the map's own is drawn
  CHECK(a.map_textures(a, files, "other", map, store)[0].rgba == tga.rgba);

  // A family that keeps its pictures in other files gives none from the map.
  std::vector<uint8_t> v38;
  CHECK(ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v38, &v38, &error));
  CHECK(a.map_textures(a, files, "room", v38, store).empty());
}

TEST(the_demos_are_listed_and_found_by_a_bare_name) {
  std::vector<uint8_t> pak;
  std::string error;
  CHECK(ge::vfs::write_pak({{"demo1.dem", {1}}, {"demos/final.mvd", {2}}, {"maps/x.bsp", {3}}},
                           &pak, &error));
  cl::SearchPath files;
  CHECK_MSG(cl::add_archive(files, "pak0.pak", std::move(pak), error), error);
  const cl::GameAdapter& a = cl::classic_adapter();
  const std::vector<std::string> demos = a.list_demos(a, files);
  CHECK(demos.size() == 2 && demos[0] == "demo1.dem" && demos[1] == "demos/final.mvd");
  std::vector<uint8_t> bytes;
  std::string found;
  CHECK_MSG(a.find_demo(a, files, "demos/final", bytes, found, error), error);
  CHECK(bytes.size() == 1 && bytes[0] == 2);
  CHECK(!a.find_demo(a, files, "nothing", bytes, found, error));
}

TEST(a_model_is_read_into_its_form_with_a_picture_standing_in_for_a_skin) {
  const ge::morph::Model sample = ge::morph::sample_model();
  as::Image skin0;
  skin0.width = 8;
  skin0.height = 4;
  skin0.rgba.assign(size_t{8} * 4 * 4, 200);
  skin0.rgba[3] = 0;  // the first texel see-through
  std::vector<uint8_t> bsp, pak;
  std::string error;
  CHECK(ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bsp, &error));
  CHECK(ge::vfs::write_pak(
      {{"progs/armor.mdl", ge::morph::write_model(sample)},
       {"progs/armor.mdl_0.tga", as::encode_tga(skin0, true)},
       {"progs/s_bubble.spr", ge::morph::write_sprite(ge::morph::sample_sprite())},
       {"maps/b_bh25.bsp", bsp},
       {"progs/odd.md9", {1, 2, 3}}},
      &pak, &error));
  cl::SearchPath files;
  CHECK_MSG(cl::add_archive(files, "pak0.pak", std::move(pak), error), error);
  const cl::GameAdapter& a = cl::classic_adapter();
  cl::OpenedModel m;
  CHECK_MSG(a.open_model(a, files, "progs/armor.mdl", m, error), error);
  CHECK(m.form == cl::ModelForm::morph);
  CHECK(m.morph.skins.size() == sample.skins.size() && m.skins.size() == sample.skins.size());
  CHECK(m.skins[0].width == 8 && m.skins[0].rgba == skin0.rgba);             // alpha kept
  for (size_t s = 1; s < m.skins.size(); s++) CHECK(m.skins[s].width == 0);  // none beside it
  CHECK_MSG(a.open_model(a, files, "progs/s_bubble.spr", m, error), error);
  CHECK(m.form == cl::ModelForm::sprite && !m.sprite.frames.empty());
  CHECK_MSG(a.open_model(a, files, "maps/b_bh25.bsp", m, error), error);
  CHECK(m.form == cl::ModelForm::brush && !m.brush.surfaces.empty());
  // Not there: false with no reason. A kind the game does not draw: the reason, and what next.
  CHECK(!a.open_model(a, files, "progs/missing.mdl", m, error) && error.empty());
  CHECK(!a.open_model(a, files, "progs/odd.md9", m, error) &&
        error.find("next:") != std::string::npos);
}

TEST(a_sound_is_read_from_the_sound_folder) {
  const std::vector<float> tone = {0.0f, 0.5f, -0.5f, 0.25f};
  std::vector<uint8_t> pak;
  std::string error;
  CHECK(ge::vfs::write_pak({{"sound/weapons/r_exp3.wav", ge::audio::wav_bytes(tone, 1, 11025)}},
                           &pak, &error));
  cl::SearchPath files;
  CHECK_MSG(cl::add_archive(files, "pak0.pak", std::move(pak), error), error);
  const cl::GameAdapter& a = cl::classic_adapter();
  ge::audio::Pcm pcm;
  CHECK_MSG(a.open_sound(a, files, "weapons/r_exp3.wav", pcm, error), error);
  CHECK(pcm.rate == 11025 && pcm.samples.size() == tone.size());
  CHECK(!a.open_sound(a, files, "weapons/none.wav", pcm, error) && error.empty());
}

TEST(a_picture_file_of_a_menu_pictures_name_stands_in_for_it) {
  // The packs' menu pictures, with the plaque and the glyph sheet each in a TGA of their name.
  const auto fixture = shooters_edge::menu::fixture_files();
  std::vector<std::pair<std::string, std::vector<uint8_t>>> listed(fixture.begin(), fixture.end());
  std::vector<uint8_t> pak;
  std::string error;
  CHECK(ge::vfs::write_pak(listed, &pak, &error));
  cl::SearchPath files;
  CHECK_MSG(cl::add_archive(files, "pak0.pak", std::move(pak), error), error);
  const cl::GameAdapter& a = cl::classic_adapter();
  const cl::ClassicPictures own = a.pictures(a, files, false, {});
  CHECK(own.menu.loaded() && own.menu.density == 1);
  const ge::ui::Image* plaque = own.menu.find("qplaque");
  CHECK(plaque != nullptr);
  // The glyph sheet at twice its size sets the set's density; the plaque at its own size is
  // scaled up to it, alpha kept; a picture of the wrong size is passed over for the pack's own.
  as::Image glyphs, qplaque, wrong;
  glyphs.width = glyphs.height = 256;
  glyphs.rgba.assign(size_t{256} * 256 * 4, 40);
  qplaque.width = plaque->width;
  qplaque.height = plaque->height;
  qplaque.rgba.assign(size_t(plaque->width) * size_t(plaque->height) * 4, 200);
  qplaque.rgba[3] = 0;
  const ge::ui::Image* title = own.menu.find("ttl_main");
  CHECK(title != nullptr);
  wrong.width = title->width + 1;
  wrong.height = title->height;
  wrong.rgba.assign(size_t(wrong.width) * size_t(wrong.height) * 4, 7);
  std::vector<uint8_t> extra;
  CHECK(ge::vfs::write_pak({{"gfx/conchars.tga", as::encode_tga(glyphs)},
                            {"gfx/qplaque.tga", as::encode_tga(qplaque, true)},
                            {"gfx/ttl_main.tga", as::encode_tga(wrong)}},
                           &extra, &error));
  CHECK_MSG(cl::add_archive(files, "pak1.pak", std::move(extra), error), error);
  const cl::ClassicPictures stood = a.pictures(a, files, false, {});
  CHECK(stood.menu.loaded() && stood.menu.density == 2);
  CHECK(stood.menu.glyphs.rgba == glyphs.rgba);
  const ge::ui::Image* big = stood.menu.find("qplaque");
  CHECK(big != nullptr && big->width == plaque->width * 2 && big->height == plaque->height * 2);
  CHECK(big->rgba[0] == 200 && big->rgba[3] == 0 && big->rgba[7] == 0);  // one texel, two pixels
  const ge::ui::Image* kept = stood.menu.find("ttl_main");
  CHECK(kept != nullptr && kept->rgba == ge::ui::scaled_nearest(*title, 2).rgba);
}

TEST(a_config_file_in_the_games_packs_runs_through_exec_after_the_players_own) {
  std::vector<uint8_t> pak;
  std::string error;
  const auto text = [](std::string_view t) { return std::vector<uint8_t>(t.begin(), t.end()); };
  CHECK(ge::vfs::write_pak({{"mymod.cfg", text("set se_test_cfg pack\n")},
                            {"mine.cfg", text("set se_test_cfg pack-mine\n")}},
                           &pak, &error));
  cl::SearchPath files;
  CHECK_MSG(cl::add_archive(files, "pak0.pak", std::move(pak), error), error);
  const cl::GameAdapter& a = cl::classic_adapter();
  std::string got;
  CHECK(a.find_config(a, files, "mymod.cfg", got) && got == "set se_test_cfg pack\n");
  CHECK(!a.find_config(a, files, "none.cfg", got));
  CHECK(!a.find_config(a, files, "../mymod.cfg", got));
  // The player's folder first: its mine.cfg wins over the pack's; mymod comes from the pack.
  const fs::path dir = fs::temp_directory_path() / "ge_se_game_adapter_cfg";
  fs::remove_all(dir);
  write(dir / "mine.cfg", text("set se_test_cfg player\n"));
  ge::console::Console con;
  con.cvars().add({.name = "se_test_cfg", .kind = ge::console::CvarKind::text});
  con.set_search_path({dir});
  cl::read_configs_from(con, a, files);
  CHECK(con.exec_file("mymod"));
  con.run();
  CHECK(con.cvars().find("se_test_cfg")->value == "pack");
  CHECK(con.exec_file("mine.cfg"));
  con.run();
  CHECK(con.cvars().find("se_test_cfg")->value == "player");
  CHECK(!con.exec_file("none.cfg"));
}

TEST(a_maps_place_names_are_read_from_its_loc_file) {
  std::vector<uint8_t> pak;
  std::string error;
  const std::string loc = "4096 0 256 ra\n-800 1600 0 $loc_name_mh$loc_name_separatorlow\nbad\n";
  CHECK(ge::vfs::write_pak(
      {{"locs/dm6.loc", std::vector<uint8_t>(loc.begin(), loc.end())}, {"locs/empty.loc", {}}},
      &pak, &error));
  cl::SearchPath files;
  CHECK_MSG(cl::add_archive(files, "pak0.pak", std::move(pak), error), error);
  const cl::GameAdapter& a = cl::classic_adapter();
  ge::shooter::Locs locs;
  CHECK(a.open_locations(a, files, "dm6", locs));
  CHECK(locs.points.size() == 2 && locs.from == "locs/dm6.loc");
  CHECK(locs.points[0].name == "ra" && locs.points[0].at.x == 512.0f);
  CHECK(locs.points[1].name == "mh-low");
  ge::shooter::Locs none;
  CHECK(!a.open_locations(a, files, "empty", none) && none.points.empty());
  CHECK(!a.open_locations(a, files, "e1m1", none));
}
