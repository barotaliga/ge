// The classic installs around the game (games/shooters_edge/client/installs.hpp) over folders
// written here: an install found from inside its base folder, its multiplayer folder and a
// client's folder beside them; the installs under a parent found by their folder names in any
// case and by what they hold, named ones first; the releases told by their packs' sizes; the one
// played: the game kept when it is among them, else the first full game; the folder an app bundle
// was placed in; and choose_folder playing the install placed above.
//
// ge:summary Tests the classic installs around the game: started inside, placed above, releases
// ge:keywords test shooters-edge installs install_around installs_below release_of pick_install
// ge:keywords placed_folder nquake parent-folder shareware registered demo full-game
// ge:skill shooter classicenhance testing
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "classicenhance/profile.hpp"
#include "shooters_edge/client/game_folder.hpp"
#include "shooters_edge/client/installs.hpp"

namespace cl = shooters_edge::client;
namespace fs = std::filesystem;

namespace {

// The folder names, spelled in pieces as installs.hpp's are.
const std::string kOne =
    "Qu"
    "ake";
const std::string kTwoSpaced =
    "QU"
    "AKE 2";
const std::string kTwo =
    "qu"
    "ake2";
const std::string kOneLower =
    "qu"
    "ake";

fs::path fresh(const std::string& name) {
  const fs::path root = fs::temp_directory_path() / ("ge_se_installs_" + name);
  fs::remove_all(root);
  fs::create_directories(root);
  return root;
}

// A pack of `size` bytes (sparse: only its size is read).
void pack(const fs::path& p, uint64_t size = 64) {
  fs::create_directories(p.parent_path());
  { std::ofstream f(p, std::ios::binary); }
  fs::resize_file(p, size);
}

const ge::classicenhance::Profile& first() { return ge::classicenhance::kProfiles[0]; }
const ge::classicenhance::Profile& sequel() { return ge::classicenhance::kProfiles[1]; }

}  // namespace

TEST(an_install_is_found_from_inside_its_folders) {
  const fs::path root = fresh("inside");
  pack(root / "ID1" / "PAK0.PAK");
  fs::create_directories(root / std::string(first().multiplayer_dir));
  fs::create_directories(root / "client" / "configs");
  cl::FoundInstall f;
  for (const fs::path& at : {root, root / "ID1", root / std::string(first().multiplayer_dir),
                             root / "client" / "configs"}) {
    CHECK_MSG(cl::install_around(at.string(), f), at.string());
    CHECK(fs::equivalent(f.basedir, root));
    CHECK(f.content == &first());
  }
  // A folder in no install: none.
  CHECK(!cl::install_around(fresh("nowhere").string(), f));
}

TEST(installs_under_a_parent_are_found_by_name_in_any_case_and_by_what_they_hold) {
  const fs::path parent = fresh("parent");
  pack(parent / "Other Folder" / "baseq2" / "pak0.pak");
  pack(parent / kTwoSpaced / "baseq2" / "pak0.pak", 49951322);
  pack(parent / "nQuake" / "id1" / "pak0.pak", 18689235);
  pack(parent / "nQuake" / "id1" / "pak1.pak", 34257856);
  pack(parent / kOne / "id1" / "pak0.pak", 18689235);
  fs::create_directories(parent / "q2");  // named, but holds no game
  fs::create_directories(parent / "music");
  const std::vector<cl::FoundInstall> found = cl::installs_below(parent.string());
  CHECK_MSG(found.size() == 4, std::to_string(found.size()));
  if (found.size() != 4) return;
  // The named folders first, in the names' order, then the others.
  CHECK(fs::path(found[0].basedir).filename() == kOne);
  CHECK(found[0].content == &first() && found[0].release == "shareware" && !found[0].full);
  CHECK(fs::path(found[1].basedir).filename() == "nQuake");
  CHECK(found[1].release == "registered" && found[1].full);
  CHECK(fs::path(found[2].basedir).filename() == kTwoSpaced);
  CHECK(found[2].content == &sequel() && found[2].release == "demo" && !found[2].full);
  CHECK(fs::path(found[3].basedir).filename() == "Other Folder");
  CHECK(found[3].release == "your own copy" && found[3].full);
  // The one played: the first full game, unless the game kept is among them.
  CHECK(cl::pick_install(found, "") == 1);
  CHECK(cl::pick_install(found, found[2].basedir) == 2);
  CHECK(cl::pick_install({}, "") == -1);
  CHECK(cl::install_line(found[1], 2).find("(registered)") != std::string::npos);
  // A file system's root is never searched.
  CHECK(cl::installs_below(fs::path(parent).root_path().string()).empty());
}

TEST(the_sequel_full_game_is_told_by_its_first_pack) {
  const fs::path root = fresh("sequel-full");
  pack(root / "baseq2" / "pak0.pak", 183997730);
  pack(root / "baseq2" / "pak1.pak", 12992754);
  cl::FoundInstall f;
  CHECK(cl::install_around(root.string(), f));
  CHECK(f.content == &sequel() && f.release == "full game" && f.full);
}

TEST(the_folder_the_game_was_placed_in_is_the_one_holding_its_app) {
  CHECK(cl::placed_folder("/games/nquake/Shooter's Edge.app/Contents/MacOS/") == "/games/nquake");
  CHECK(cl::placed_folder("/games/nquake/") == "/games/nquake");
  CHECK(cl::placed_folder("").empty());
}

TEST(the_game_placed_above_installs_plays_one_of_them_ahead_of_the_kept_game) {
  const fs::path parent = fresh("placed");
  pack(parent / kOneLower / "id1" / "pak0.pak", 18689235);
  pack(parent / kTwo / "baseq2" / "pak0.pak", 183997730);
  const fs::path prefs = fresh("placed-prefs");
  const fs::path kept = fresh("placed-kept");
  pack(kept / "id1" / "pak0.pak");
  CHECK(cl::remember_game(prefs.string(), kept.string()));
  cl::Args a;
  a.basedir = fresh("placed-cwd").string();
  std::vector<cl::FoundInstall> found;
  cl::choose_folder(a, prefs.string(), {parent.string()}, &found);
  CHECK(found.size() == 2);
  CHECK(fs::equivalent(a.basedir, parent / kTwo));  // the first full game
  CHECK(a.profile == sequel().id);
  CHECK(a.game == sequel().base_dir);
  // The game kept is one of them: it is played.
  CHECK(cl::remember_game(prefs.string(), (parent / kOneLower).string()));
  cl::Args b;
  b.basedir = fresh("placed-cwd2").string();
  cl::choose_folder(b, prefs.string(), {parent.string()});
  CHECK(fs::equivalent(b.basedir, parent / kOneLower));
}
