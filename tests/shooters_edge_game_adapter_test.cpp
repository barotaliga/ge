// The first classic game's adapter (games/shooters_edge/client/game_adapter.hpp): its mods are the
// folders beside the base that hold archives or a program; each mode finds its program through a
// mod's folder over the base, passing over a program of the other mode's layout and a file of a
// form this build has no runner for, each with its reason; a map's info and textures are read off
// its file (the pictures the same views `read_map_pictures` gives, so the drawn map is unchanged);
// the demos are listed and found by a bare name with the game's extensions.
//
// ge:summary Tests the first game's adapter: mods, programs by form, map info, textures, demos
// ge:keywords test shooters-edge game-adapter list_mods find_program program-form map_info
// ge:keywords map_textures list_demos find_demo qwprogs
// ge:skill shooter testing
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "brush/pictures.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classicenhance/profile.hpp"
#include "progs/assembler.hpp"
#include "progs/layout.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "vfs/pak.hpp"

namespace cl = shooters_edge::client;
namespace pf = shooters_edge::profiles;
namespace pg = ge::progs;
namespace fs = std::filesystem;

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
  const std::vector<ge::brush::MapPicture> pics = a.map_textures(a, files, map, store);
  CHECK(pics.size() == info.textures);
  const std::vector<ge::brush::MapPicture> direct = ge::brush::read_map_pictures(map);
  CHECK(pics.size() == direct.size());
  for (size_t i = 0; i < pics.size() && i < direct.size(); ++i) {
    CHECK(pics[i].name == direct[i].name && pics[i].width == 64 && pics[i].height == 64);
    CHECK(pics[i].level[0] == direct[i].level[0] && pics[i].level[3] != nullptr);
  }
  // A family that keeps its pictures in other files gives none from the map.
  std::vector<uint8_t> v38;
  CHECK(ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v38, &v38, &error));
  CHECK(a.map_textures(a, files, v38, store).empty());
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
