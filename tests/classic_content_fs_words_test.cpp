// The classic file words (plugins/classic/content/fs_words.hpp) over a Quake II folder written
// here: a game path made normal; the filter's `*`, `?` and `\`; `dir` merging a pack and the folder
// with each name once, its extensions; `fdir` by filter; `whereis` first and every place, a path
// not found, a path too long for the packs; `link` and `softlink` making a file readable under
// another name, their list, `unlink`'s forms; the links kept when the path is opened again;
// `fs_restart`'s banner with the pack totals; the game folder's name checked.
//
// ge:summary Tests the classic file words: dir, fdir, whereis, links, fs_restart, the game check
// ge:keywords test dir fdir whereis link softlink unlink fs_restart game-folder
// ge:skill classic vfs testing
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "check.hpp"
#include "classic/content/files.hpp"
#include "classic/content/fs_words.hpp"
#include "classicenhance/profile.hpp"
#include "vfs/pak.hpp"

namespace cl = ge::classic::content;
namespace fs = std::filesystem;

namespace {

using Files = std::vector<std::pair<std::string, std::vector<uint8_t>>>;

void write(const fs::path& p, const std::vector<uint8_t>& b) {
  fs::create_directories(p.parent_path());
  std::ofstream f(p, std::ios::binary);
  f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}

// <root>/baseq2: pak0 (maps/q2dm1.bsp, maps/base1.bsp, sound/x.wav); loose maps/q2dm1.bsp,
// maps/notes.txt and a hidden file.
fs::path make_tree() {
  const fs::path root = fs::temp_directory_path() / "ge_classic_fs_words_test";
  fs::remove_all(root);
  std::vector<uint8_t> pack;
  std::string error;
  const Files files = {
      {"maps/q2dm1.bsp", {1, 2, 3}}, {"maps/base1.bsp", {4, 5}}, {"sound/x.wav", {6, 7, 8, 9}}};
  CHECK_MSG(ge::vfs::write_pak(files, &pack, &error), error);
  write(root / "baseq2" / "pak0.pak", pack);
  write(root / "baseq2" / "maps" / "q2dm1.bsp", {1, 2, 3, 4, 5, 6});
  write(root / "baseq2" / "maps" / "notes.txt", {'h', 'i'});
  write(root / "baseq2" / "maps" / ".hidden", {0});
  return root;
}

std::string run(const cl::SearchPath& sp, const std::vector<std::string>& w) {
  std::string out;
  CHECK(cl::fs_command(sp, w, out));
  return out;
}

bool has(const std::string& text, const std::string& part) {
  const bool yes = text.find(part) != std::string::npos;
  if (!yes) std::fprintf(stderr, "missing \"%s\" in:\n%s\n", part.c_str(), text.c_str());
  return yes;
}

}  // namespace

TEST(a_game_path_is_made_normal_and_matched_by_a_filter) {
  // A game path made normal; never above the root.
  CHECK(cl::normal_game_path("\\maps//./x/../q2dm1.bsp/") == "maps/q2dm1.bsp");
  CHECK(cl::normal_game_path("../../a") == "a");
  CHECK(cl::normal_game_path("/./").empty());

  // The filter: `*` across folders, `?` one character, `\` the next as itself, ignoring case.
  CHECK(cl::filter_match("maps/*.bsp", "MAPS/Q2DM1.BSP"));
  CHECK(cl::filter_match("*.wav", "sound/x.wav"));
  CHECK(cl::filter_match("?aps/*", "maps/a"));
  CHECK(!cl::filter_match("?aps/*", "aps/a"));
  CHECK(cl::filter_match("a\\*b", "a*b"));
  CHECK(!cl::filter_match("a\\*b", "axb"));
  CHECK(cl::filter_match("*", ""));
}

TEST(the_file_words_list_find_and_link_over_a_folder) {
  const fs::path root = make_tree();
  const ge::classicenhance::Profile* q2 = ge::classicenhance::find_profile("q2");
  CHECK(q2 != nullptr);
  cl::SearchPath sp;
  std::string error;
  CHECK_MSG(cl::open_game_dir(root.string(), "baseq2", sp, error, *q2), error);

  // `dir`: the pack and the folder merged, each name once, sorted; no hidden file; extensions.
  CHECK(run(sp, {"dir"}) == "Usage: dir <directory> [.extension]\n");
  CHECK(run(sp, {"dir", "maps"}) == "base1.bsp\nnotes.txt\nq2dm1.bsp\n3 files listed\n");
  CHECK(run(sp, {"dir", "maps/", "BSP"}) == "base1.bsp\nq2dm1.bsp\n2 files listed\n");
  CHECK(run(sp, {"dir", "maps", ".txt;wav"}) == "notes.txt\n1 files listed\n");
  CHECK(run(sp, {"dir", "nothing"}) == "0 files listed\n");

  // `fdir`: whole paths by filter, several by `;`.
  CHECK(run(sp, {"fdir"}) == "Usage: fdir <filter>\n");
  CHECK(run(sp, {"fdir", "*.wav"}) == "sound/x.wav\n1 files listed\n");
  CHECK(run(sp, {"fdir", "*.wav;maps/base?.bsp"}) ==
        "maps/base1.bsp\nsound/x.wav\n2 files listed\n");

  // `whereis`: the first place, every place with a second word, and the refusals.
  CHECK(run(sp, {"whereis"}) == "Usage: whereis <path> [all]\n");
  CHECK(run(sp, {"whereis", "/./"}) == "Refusing to lookup empty path.\n");
  CHECK(run(sp, {"whereis", std::string(300, 'a')}) == "Refusing to lookup oversize path.\n");
  const std::string first = run(sp, {"whereis", "maps/q2dm1.bsp"});
  CHECK(first.find("instances") == std::string::npos);
  CHECK(first.find('\n') == first.size() - 1);  // one place
  const std::string every = run(sp, {"whereis", "maps/q2dm1.bsp", "all"});
  CHECK(has(every, "pak0.pak/maps/q2dm1.bsp (3 bytes)\n"));
  CHECK(has(every, "maps/q2dm1.bsp (6 bytes)\n"));
  CHECK(has(every, "2 instances of maps/q2dm1.bsp\n"));
  CHECK(run(sp, {"whereis", "maps/none.bsp"}) == "maps/none.bsp was not found\n");
  CHECK(has(run(sp, {"whereis", "maps/" + std::string(70, 'b')}),
            "in pack files since path length exceedes 63 characters.\n"));

  // `link`: a file read under another name; the list; moved; `unlink`.
  std::vector<uint8_t> store;
  CHECK(cl::find_file(sp, "maps/q2dm9.bsp", store).empty());
  CHECK(run(sp, {"link", "maps/q2dm9", "maps/q2dm1"}).empty());
  CHECK(!cl::find_file(sp, "maps/q2dm9.bsp", store).empty());
  CHECK(run(sp, {"link"}) ==
        "maps/q2dm9 --> maps/q2dm1\n------------------\n1 symbolic link listed.\n");
  CHECK(has(run(sp, {"whereis", "maps/q2dm9.bsp"}),
            "Symbolic link ('maps/q2dm9' --> 'maps/q2dm1') in effect.\n"));
  CHECK(run(sp, {"link", "a"}).starts_with("Usage: link <name> <target>\n"));
  CHECK(run(sp, {"link", "/", "b"}) == "Invalid symbolic link name.\n");
  CHECK(run(sp, {"link", "b", "/"}) == "Invalid symbolic link target.\n");
  // The links kept when the path is opened again; fs_restart's banner with the pack's total.
  CHECK_MSG(cl::open_game_dir(root.string(), "baseq2", sp, error, *q2), error);
  CHECK(!cl::find_file(sp, "maps/q2dm9.bsp", store).empty());
  const std::string banner = cl::fs_restart_text(sp);
  CHECK(banner.starts_with("----- FS_Restart -----\nCurrent search path:\n"));
  CHECK(has(banner, "pak0.pak (3 files)\n"));
  CHECK(banner.ends_with("baseq2\n3 files in PAK files\n----------------------\n"));
  CHECK(run(sp, {"unlink", "MAPS/Q2DM9"}).empty());
  CHECK(cl::find_file(sp, "maps/q2dm9.bsp", store).empty());
  CHECK(run(sp, {"unlink", "maps/q2dm9"}) == "Symbolic link 'maps/q2dm9' does not exist.\n");
  CHECK(run(sp, {"unlink"}) ==
        "Missing name argument.\nTry 'unlink --help' for more information.\n");
  CHECK(run(sp, {"unlink", "-x"}) ==
        "Unknown option: -x.\nTry 'unlink --help' for more information.\n");
  CHECK(run(sp, {"unlink", "-h"}).starts_with("Usage: unlink [-ah] <name>\n"));
  CHECK(run(sp, {"link"}) == "------------------\n0 symbolic links listed.\n");

  // `softlink`: tried only when the name itself is not found.
  CHECK(run(sp, {"softlink", "sound/y", "sound/x"}).empty());
  CHECK(cl::find_file(sp, "sound/y.wav", store).size() == 4);
  CHECK(run(sp, {"softlink", "maps/q2dm1", "maps/base1"}).empty());
  CHECK(cl::find_file(sp, "maps/q2dm1.bsp", store).size() == 3);  // its own, not the link's
  CHECK(has(run(sp, {"whereis", "sound/y.wav"}),
            "Symbolic link ('sound/y' --> 'sound/x') in effect.\n"));
  CHECK(run(sp, {"softunlink", "--all"}) == "Deleted all symbolic links.\n");
  CHECK(cl::find_file(sp, "sound/y.wav", store).empty());

  // The game folder's name.
  std::string said;
  std::string game = "BaseQ2";
  CHECK(!cl::game_folder_check(game, "baseq2", said) && game.empty() && said.empty());
  game = "my mod";
  CHECK(!cl::game_folder_check(game, "baseq2", said) && game.empty());
  CHECK(said == "'game' should contain characters [A-Za-z0-9_-] only.\n");
  game = "ctf-2_x";
  CHECK(cl::game_folder_check(game, "baseq2", said) && game == "ctf-2_x");

  fs::remove_all(root);
}
