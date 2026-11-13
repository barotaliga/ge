// The first classic game's adapter (games/shooters_edge/client/game_adapter.hpp): its mods are the
// folders beside the base that hold archives or a program; each mode finds its program through a
// mod's folder over the base, passing over a program of the other mode's layout and a file of a
// form this build has no runner for, each with its reason; a map's info and textures are read off
// its file (the pictures the same views `read_map_pictures` gives, so the drawn map is unchanged);
// the demos are listed and found by a bare name with the game's extensions; a model is read into
// its form with a picture beside it standing in for a skin; a sound is read from `sound/`; the
// menus' pictures are the packs' with a PNG or TGA of a picture's name standing in for it; a
// config file in the packs runs through `exec` after the player's own; a map's place names are
// read from its loc file; the game's own held weapons are its first-person models, all or none;
// its menu, its modes and its HUD's pictures are its own.
//
// ge:summary Tests the first game's adapter: mods, programs by form, map info, textures, demos
// ge:keywords test shooters-edge game-adapter list_mods find_program program-form map_info
// ge:keywords map_textures list_demos find_demo demo_folder attract
// ge:keywords qwprogs open_model open_sound replacement-skin
// ge:keywords pictures replacement-picture find_config read_configs_from open_locations
// ge:keywords held_weapons menu-table game-modes hud_art_of
// ge:skill shooter testing
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "assets/image.hpp"
#include "assets/iqm.hpp"
#include "assets/pcx.hpp"
#include "assets/test_model.hpp"
#include "assets/tga.hpp"
#include "audio/wav.hpp"
#include "brush/pictures.hpp"
#include "brush/read.hpp"
#include "brush/wal.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classicenhance/profile.hpp"
#include "console/console.hpp"
#include "morph/write.hpp"
#include "progs/assembler.hpp"
#include "progs/layout.hpp"
#include "shooter/locs.hpp"
#include "shooters_edge/client/classic_pictures.hpp"
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

  // A family that keeps its pictures in other files: one entry a texture, its pixels from its wall
  // texture file wherever the open files hold one (a map of that family copied into the first
  // game's folder draws too), a texture with no file a check (no pixels); the sequel's adapter
  // reads it the same.
  std::vector<uint8_t> v38;
  CHECK(ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v38, &v38, &error));
  ge::brush::Map m38;
  CHECK_MSG(ge::brush::read_bsp(v38, &m38, &error), error);
  const std::vector<ge::brush::MapPicture> none = a.map_textures(a, files, "room", v38, store);
  CHECK(none.size() == m38.textures.size() && none[0].width == 0);
  ge::brush::WalHead head;
  head.name = m38.textures[0].name;
  std::vector<uint8_t> wal_pak;
  CHECK(ge::vfs::write_pak({{ge::brush::wal_path(m38.textures[0].name),
                             ge::brush::write_wal(head, 16, 8, std::vector<uint8_t>(128, 5))}},
                           &wal_pak, &error));
  CHECK_MSG(cl::add_archive(files, "pak2.pak", std::move(wal_pak), error), error);
  for (const cl::GameAdapter* g : {&a, &cl::sequel_adapter()}) {
    const std::vector<ge::brush::MapPicture> walls = g->map_textures(*g, files, "room", v38, store);
    CHECK(walls.size() == m38.textures.size());
    CHECK(walls[0].width == 16 && walls[0].height == 8 && walls[0].level[0][0] == 5);
    CHECK(walls[0].name == m38.textures[0].name);
    if (walls.size() > 1) CHECK(walls[1].width == 0 && walls[1].name == m38.textures[1].name);
  }
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
  CHECK(!a.find_demo(a, files, "final", bytes, found, error));  // the first game: top level only
}

TEST(the_sequel_finds_a_bare_demo_name_in_its_demos_folder_and_plays_it_behind_the_menu) {
  std::vector<uint8_t> pak;
  std::string error;
  CHECK(ge::vfs::write_pak({{"demos/q2demo1.dm2", {7}}}, &pak, &error));
  cl::SearchPath files;
  CHECK_MSG(cl::add_archive(files, "pak0.pak", std::move(pak), error), error);
  const cl::GameAdapter& a = cl::sequel_adapter();
  std::vector<uint8_t> bytes;
  std::string found;
  CHECK_MSG(a.find_demo(a, files, std::string(a.game->attract), bytes, found, error), error);
  CHECK(bytes.size() == 1 && bytes[0] == 7);
  CHECK_MSG(found.starts_with("demos/q2demo1.dm2"), found);
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
        error.find("use a .mdl, .md2, .iqm, .spr, .sp2 or .bsp") != std::string::npos);
}

TEST(a_mods_skeletal_model_is_baked_into_frames_with_its_materials_pictures_in_one_skin) {
  // The test model's two materials: `skin` has a picture beside the model, `fin` none (grey).
  const as::Model limb = as::make_test_model();
  as::Image skin;
  skin.width = 4;
  skin.height = 4;
  skin.rgba.assign(size_t{4} * 4 * 4, 220);
  std::vector<uint8_t> pak;
  std::string error;
  CHECK(ge::vfs::write_pak({{"progs/limb.iqm", as::write_iqm(limb, {.metres = 0.025})},
                            {"progs/skin.tga", as::encode_tga(skin, true)},
                            {"progs/bad.iqm", {'I', 'N', 'T', 'E', 'R'}}},
                           &pak, &error));
  cl::SearchPath files;
  CHECK_MSG(cl::add_archive(files, "pak0.pak", std::move(pak), error), error);
  const cl::GameAdapter& a = cl::classic_adapter();
  cl::OpenedModel m;
  CHECK_MSG(a.open_model(a, files, "progs/limb.iqm", m, error), error);
  CHECK(m.form == cl::ModelForm::morph && m.morph.skins.size() == 1 && m.skins.size() == 1);
  // A frame group a clip (`wave`, `twist`); the skin the two pictures one under the other.
  CHECK(!m.morph.frames.empty() && m.morph.frames.back().group == 1);
  CHECK(m.skins.size() == 1 && m.skins[0].width == 8 && m.skins[0].height == 12);
  CHECK(m.skins.size() == 1 && m.skins[0].rgba[0] == 220);
  // The limb is a metre tall: 40 map units up the file's +z.
  CHECK(!m.morph.frames.empty() && m.morph.frames[0].max[2] > 38 && m.morph.frames[0].max[2] < 48);
  CHECK(!m.morph.frames.empty() && std::abs(m.morph.frames[0].min[2]) < 1);
  CHECK(!a.open_model(a, files, "progs/bad.iqm", m, error) &&
        error.find("progs/bad.iqm") != std::string::npos &&
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

TEST(the_sequel_s_menu_pictures_are_read_from_its_pics_folder) {
  // The sequel's glyph sheet and main menu pictures as its own PCX files, the last colour clear.
  const auto pcx = [](int w, int h) {
    as::IndexedImage img;
    img.width = w;
    img.height = h;
    img.indices.assign(size_t(w) * size_t(h), 7);
    img.indices[0] = 255;
    img.palette.assign(768, 90);
    return as::encode_pcx(img);
  };
  std::vector<std::pair<std::string, std::vector<uint8_t>>> listed = {
      {"pics/conchars.pcx", pcx(128, 128)},          {"pics/m_main_game.pcx", pcx(99, 32)},
      {"pics/m_main_multiplayer.pcx", pcx(215, 32)}, {"pics/m_main_options.pcx", pcx(150, 32)},
      {"pics/m_main_quit.pcx", pcx(100, 32)},        {"pics/m_main_plaque.pcx", pcx(38, 166)},
      {"pics/m_cursor3.pcx", pcx(22, 29)},           {"pics/m_banner_options.pcx", pcx(275, 32)}};
  const auto read = [&](bool video) {
    auto all = listed;
    if (video) all.push_back({"pics/m_main_video.pcx", pcx(118, 32)});
    std::vector<uint8_t> pak;
    std::string error;
    CHECK(ge::vfs::write_pak(all, &pak, &error));
    cl::SearchPath files;
    CHECK_MSG(cl::add_archive(files, "pak0.pak", std::move(pak), error), error);
    const cl::GameAdapter& a = cl::sequel_adapter();
    return a.pictures(a, files, false, {}).menu;
  };
  const shooters_edge::menu::ClassicArt art = read(true);
  CHECK(art.plates() && !art.loaded() && art.screen_height() == 240);
  const ge::ui::Image* game = art.find("m_main_game");
  CHECK(game != nullptr && game->width == 99 && game->rgba[3] == 0 && game->rgba[7] == 255);
  CHECK(art.find("m_cursor3") != nullptr && art.find("m_banner_options") != nullptr);
  // The words cut from the five plates are cut only when all five were read: their print.
  CHECK(art.strip_print != 0);
  CHECK(read(false).strip_print == 0);
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

TEST(the_games_own_held_weapons_are_its_first_person_models_in_the_number_keys_order) {
  const pf::GameProfile& g = pf::classic_game();
  const std::vector<uint8_t> model = ge::morph::write_model(ge::morph::sample_model());
  std::vector<std::pair<std::string, std::vector<uint8_t>>> listed = {
      {"gfx/palette.lmp", std::vector<uint8_t>(768, 90)}};
  for (const std::string_view name : g.held_weapons) listed.push_back({std::string(name), model});
  // All but the last: the set is refused with the one missing named; nothing stands in for it.
  std::vector<uint8_t> pak;
  std::string error;
  std::vector<std::pair<std::string, std::vector<uint8_t>>> short_set(listed.begin(),
                                                                      listed.end() - 1);
  CHECK(ge::vfs::write_pak(short_set, &pak, &error));
  cl::SearchPath part;
  CHECK_MSG(cl::add_archive(part, "pak0.pak", std::move(pak), error), error);
  const cl::GameAdapter& a = cl::classic_adapter();
  std::vector<ge::viewmodel::HeldModel> held;
  std::string why;
  CHECK(!a.held_weapons(a, part, held, why) && held.empty());
  CHECK(why.find(std::string(g.held_weapons.back())) != std::string::npos);
  // No palette: refused, saying so; a palette and no models: false with nothing to say.
  cl::SearchPath none;
  CHECK(!a.held_weapons(a, none, held, why) && why.find("palette") != std::string::npos);
  CHECK(ge::vfs::write_pak({listed[0]}, &pak, &error));
  cl::SearchPath bare;
  CHECK_MSG(cl::add_archive(bare, "pak0.pak", std::move(pak), error), error);
  CHECK(!a.held_weapons(a, bare, held, why) && why.empty());
  // The whole set: eight models, each with its firing frames as the fire clip; the axe's, an even
  // run, as two takes of it (its two swings), one after the other.
  CHECK(ge::vfs::write_pak(listed, &pak, &error));
  cl::SearchPath files;
  CHECK_MSG(cl::add_archive(files, "pak0.pak", std::move(pak), error), error);
  CHECK_MSG(a.held_weapons(a, files, held, why), why);
  CHECK(held.size() == g.held_weapons.size() && g.held_weapons.size() == 8);
  for (size_t i = 0; i < held.size(); ++i) {
    const ge::viewmodel::HeldModel& h = held[i];
    CHECK(h.frames.size() == ge::morph::sample_model().frames.size() && !h.skin.image.rgba.empty());
    const int firing = int(h.frames.size()) - 1;
    if (firing < 1) continue;
    const int takes = i == 0 && firing % 2 == 0 ? 2 : 1;  // the axe first, in the keys' order
    CHECK(ge::viewmodel::count_held_clips(h, "fire") == takes);
    for (int k = 0; k < takes; ++k) {
      const ge::viewmodel::HeldClip& c =
          h.clips[size_t(ge::viewmodel::find_held_clip(h, "fire", k))];
      CHECK(c.first == 1 + k * (firing / takes) && c.count == firing / takes);
    }
  }
}

TEST(a_games_menu_modes_and_hud_are_its_own) {
  const cl::GameAdapter& a = cl::classic_adapter();
  // The first game: its classic pages, both modes.
  CHECK(&a.menu(a) == &shooters_edge::menu::classic_menu());
  const cl::GameModes m = a.modes(a);
  CHECK(m.single && m.multi);
  // A game's own table builds its own pages: one with no campaign has no SINGLE PLAYER.
  static constexpr shooters_edge::menu::PageTitle kPages[] = {{"main", ""}, {"multi", "MULTI"}};
  static constexpr shooters_edge::menu::Entry kRows[] = {
      {.page = "main", .id = "multi", .text = "MULTIPLAYER", .opens = "multi"},
      {.page = "multi", .id = "back", .text = "BACK", .kind = ge::ui::ItemKind::back}};
  const shooters_edge::menu::MenuTable table{kPages, kRows};
  ge::ui::Menu built = shooters_edge::menu::build_menu(table);
  CHECK(built.page("single") == nullptr && built.page("multi") != nullptr);
  CHECK(shooters_edge::menu::find_entry("main", "multi", table) != nullptr);
  CHECK(shooters_edge::menu::find_entry("main", "single", table) == nullptr);
  // The HUD's pictures: a classic game's own whenever its folder is open, even empty.
  shooters_edge::hud::HudArt game, own;
  own.density = 3;
  cl::SearchPath none, open;
  open.folder = "base";
  CHECK(&cl::hud_art_of(none, game, own) == &own);
  CHECK(&cl::hud_art_of(open, game, own) == &game);
}

TEST(the_sequel_s_row_has_its_own_adapter_which_names_its_native_rules_and_passes_them_over) {
  const pf::GameProfile* q = pf::find_game("sequel");
  CHECK(q != nullptr);
  const cl::GameAdapter& a = cl::adapter_of(*q);
  CHECK(&a == &cl::sequel_adapter() && a.game == q);
  CHECK(&cl::adapter_of(pf::classic_game()) == &cl::classic_adapter());
  std::vector<uint8_t> pak;
  std::string error;
  CHECK(ge::vfs::write_pak({{"gamex86.dll", {1, 2, 3}}, {"maps/demo1.bsp", {4}}}, &pak, &error));
  cl::SearchPath files;
  CHECK_MSG(cl::add_archive(files, "pak0.pak", std::move(pak), error), error);
  cl::FoundProgram found;
  std::string why;
  CHECK(!a.find_program(a, files, false, found, why));
  CHECK(why.find("gamex86.dll") != std::string::npos);
  CHECK(why.find("native game library") != std::string::npos);
  const std::vector<std::string> maps = a.list_maps(a, files);
  CHECK(maps.size() == 1 && maps[0] == "demo1");
  const cl::GameModes m = a.modes(a);
  CHECK(m.single && m.multi);
}
