// The classic installs around the game (games/shooters_edge/client/install_search.hpp) over folders
// written here: an install found from inside its base folder, its multiplayer folder and a
// client's folder beside them; the installs under a parent found by their folder names in any
// case and by what they hold, named ones first; the releases told by their packs' sizes; the folder
// an app bundle was placed in; and the installs near the game listed, never played on their own.
//
// ge:summary Tests the classic installs around the game: started inside, placed above, releases
// ge:keywords test shooters-edge installs install_around installs_below release_of installs_near
// ge:keywords placed_folder nquake parent-folder shareware registered demo full-game
// ge:skill shooter classicenhance testing
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "classicenhance/profile.hpp"
#include "shooters_edge/client/game_folder.hpp"
#include "shooters_edge/client/install_search.hpp"

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
const ge::classicenhance::Profile& q2() { return ge::classicenhance::kProfiles[1]; }

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
  pack(parent / "nQuake" / "ROGUE" / "PAK0.PAK");
  pack(parent / "nQuake" / "hipnotic" / "pak0.pak");
  fs::create_directories(parent / "nQuake" / "xatrix");  // another game's pack's name, empty
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
  CHECK(found[2].content == &q2() && found[2].release == "demo" && !found[2].full);
  CHECK(fs::path(found[3].basedir).filename() == "Other Folder");
  CHECK(found[3].release == "your own copy" && found[3].full);
  CHECK(cl::install_line(found[1], 2).find("(registered)") != std::string::npos);
  // Its mission packs beside its base folder, in the profile's order.
  CHECK(found[1].packs.size() == 2 && found[1].packs[0]->name == "Scourge of Armagon");
  CHECK_MSG(cl::install_line(found[1], 2)
                    .find("(registered) with Scourge of Armagon and Dissolution of Eternity") !=
                std::string::npos,
            cl::install_line(found[1], 2));
  CHECK(found[0].packs.empty());
  // A file system's root is never searched.
  CHECK(cl::installs_below(fs::path(parent).root_path().string()).empty());
}

TEST(q2_full_game_is_told_by_its_first_pack) {
  const fs::path root = fresh("q2-full");
  pack(root / "baseq2" / "pak0.pak", 183997730);
  pack(root / "baseq2" / "pak1.pak", 12992754);
  cl::FoundInstall f;
  CHECK(cl::install_around(root.string(), f));
  CHECK(f.content == &q2() && f.release == "full game" && f.full);
}

TEST(the_folder_the_game_was_placed_in_is_the_one_holding_its_app) {
  // In the platform's own separators (a backslash on Windows).
  const fs::path folder = fs::path("/games/nquake").make_preferred();
  CHECK(fs::path(cl::placed_folder("/games/nquake/Shooter's Edge.app/Contents/MacOS/")) == folder);
  CHECK(fs::path(cl::placed_folder("/games/nquake/")) == folder);
  CHECK(cl::placed_folder("").empty());
}

TEST(the_installs_near_the_game_are_listed_and_never_played_on_their_own) {
  const fs::path parent = fresh("placed");
  pack(parent / kOneLower / "id1" / "pak0.pak", 18689235);
  pack(parent / kTwo / "baseq2" / "pak0.pak", 183997730);
  // Placed above two installs (and inside one of them twice over): each listed once.
  const std::vector<cl::FoundInstall> found =
      cl::installs_near({parent.string(), (parent / kTwo / "baseq2").string()});
  CHECK_MSG(found.size() == 2, std::to_string(found.size()));
  // The game played is the settings' alone: none set, Shooter's Edge's own game; Quake II picked,
  // no install set for it, its folder asked for though one lies beside the game.
  cl::BaseGame base;
  cl::Args a;
  cl::choose_folder(a, base);
  CHECK(a.basedir == "." && !a.ask_install);
  base.game = std::string(q2().id);
  cl::Args b;
  cl::choose_folder(b, base);
  CHECK(b.ask_install && b.profile == q2().id);
}
