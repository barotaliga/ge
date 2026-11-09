// The folder browser (engine/vfs/browse.hpp): which names are shown (a game's extensions plain or
// under the packings unpack_one peels, never an archive with nothing named under it), the listing's
// order (`..`, folders by name, files newest first, hidden entries out), the places, and the
// presses: rows, ENTER into a folder and on a file, BACKSPACE up with the folder left selected,
// LEFT and RIGHT through the places, a folder that does not open said in the note.
//
// ge:summary Tests the folder browser: names shown, listing order, places and presses
// ge:keywords test vfs browse folder-browser named_as list_folder places parent packed-name
// ge:skill vfs testing
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "check.hpp"
#include "vfs/browse.hpp"

namespace fs = std::filesystem;
namespace vfs = ge::vfs;

namespace {

const std::vector<std::string> kDemos = {".mvd", ".rec", ".dem", ".mvdz"};

void touch(const fs::path& p, size_t bytes, int age_s) {
  std::ofstream(p, std::ios::binary) << std::string(bytes, 'x');
  fs::last_write_time(p, fs::file_time_type::clock::now() - std::chrono::seconds(age_s));
}

struct Tree {
  fs::path root;
  Tree() {
    root = fs::temp_directory_path() / "ge_vfs_browse_test";
    fs::remove_all(root);
    fs::create_directories(root / "Downloads" / "old");
    fs::create_directories(root / "Downloads" / "Match night");
    fs::create_directories(root / "Downloads" / ".cache");
    fs::create_directories(root / "Games");
    const fs::path d = root / "Downloads";
    touch(d / "duel.mvd.gz", 2000, 60);
    touch(d / "final.REC.zip", 3000, 7200);
    touch(d / "big.mvd.7z", 4000, 86400 * 3);
    touch(d / "team.mvd.tar.gz", 1000, 30);
    touch(d / "maps.zip", 100, 10);
    touch(d / "readme.txt", 100, 5);
    touch(d / "mine.mvdz", 500, 120);
    touch(d / ".hidden.mvd", 100, 1);
  }
  ~Tree() {
    std::error_code ec;
    fs::remove_all(root, ec);
  }
};

std::vector<std::string> names(const vfs::FolderBrowser& b) {
  std::vector<std::string> out;
  out.reserve(b.entries().size());
  for (const vfs::FolderEntry& e : b.entries()) out.push_back(e.name);
  return out;
}

}  // namespace

TEST(named_as_takes_the_games_names_plain_or_packed) {
  CHECK(vfs::named_as("match.mvd", kDemos));
  CHECK(vfs::named_as("MATCH.MVD", kDemos));
  CHECK(vfs::named_as("match.mvd.gz", kDemos));
  CHECK(vfs::named_as("match.rec.zip", kDemos));
  CHECK(vfs::named_as("match.mvd.7z", kDemos));
  CHECK(vfs::named_as("match.dem.xz", kDemos));
  CHECK(vfs::named_as("match.mvd.zst", kDemos));
  CHECK(vfs::named_as("match.mvd.tar.gz", kDemos));
  CHECK(vfs::named_as("match.mvd.tar", kDemos));
  CHECK(vfs::named_as("match.mvdz", kDemos));
  // An archive with no game name under its packing says nothing of what it holds.
  CHECK(!vfs::named_as("maps.zip", kDemos));
  CHECK(!vfs::named_as("match.tar.gz", kDemos));
  CHECK(!vfs::named_as("match.tgz", kDemos));
  CHECK(!vfs::named_as("match.mvd.txt", kDemos));
  CHECK(!vfs::named_as("match.txt.gz", kDemos));
  CHECK(!vfs::named_as(".mvd", kDemos));
  CHECK(!vfs::named_as(".gz", kDemos));
  CHECK(!vfs::named_as("mvd", kDemos));
  CHECK(vfs::wrapper_extension(".GZ"));
  CHECK(!vfs::wrapper_extension(".mvd"));
}

TEST(a_folder_lists_up_folders_then_files_newest_first) {
  const Tree t;
  std::vector<vfs::FolderEntry> out;
  std::string error;
  CHECK(vfs::list_folder((t.root / "Downloads").string(), kDemos, out, error));
  std::vector<std::string> got;
  got.reserve(out.size());
  for (const vfs::FolderEntry& e : out) got.push_back(e.name);
  const std::vector<std::string> want = {
      "..",          "Match night", "old",           "team.mvd.tar.gz",
      "duel.mvd.gz", "mine.mvdz",   "final.REC.zip", "big.mvd.7z"};
  CHECK(got == want);
  CHECK(out[0].folder && out[0].path == t.root.string());
  CHECK(out[3].bytes == 1000 && !out[3].folder);
  CHECK(out[4].age_s >= 59 && out[4].age_s < 600);
}

TEST(a_folder_that_does_not_open_is_said) {
  std::vector<vfs::FolderEntry> out;
  std::string error;
  CHECK(!vfs::list_folder("/no/such/folder/here", kDemos, out, error));
  CHECK(error.find("/no/such/folder/here") != std::string::npos);
  CHECK(error.find("next:") != std::string::npos);
  CHECK(out.empty());
}

TEST(the_browser_walks_folders_and_chooses_a_file) {
  const Tree t;
  vfs::FolderBrowser b;
  b.set_extensions(kDemos);
  b.set_places(
      {{"GAME", (t.root / "Games").string()}, {"DOWNLOADS", (t.root / "Downloads").string()}});
  CHECK(b.open((t.root / "Downloads").string()));
  CHECK(b.place() == 1);
  CHECK(b.files() == 5);
  // Opens on the first file, the newest.
  CHECK(b.selection() != nullptr && b.selection()->name == "team.mvd.tar.gz");
  vfs::BrowseInput down;
  down.down = true;
  CHECK(b.input(down) == vfs::BrowseAction::moved);
  vfs::BrowseInput enter;
  enter.accept = true;
  CHECK(b.input(enter) == vfs::BrowseAction::chosen);
  CHECK(b.chosen() == (t.root / "Downloads" / "duel.mvd.gz").string());

  // Into a folder and back up: the folder left selected.
  vfs::BrowseInput click;
  click.pointed = 1;
  click.click = true;
  CHECK(b.input(click) == vfs::BrowseAction::opened);
  CHECK(fs::path(b.dir()).filename() == "Match night");
  CHECK(names(b) == std::vector<std::string>{".."});
  vfs::BrowseInput up;
  up.parent = true;
  CHECK(b.input(up) == vfs::BrowseAction::opened);
  CHECK(b.selection()->name == "Match night");

  // The places: right from Downloads wraps to the game's; left comes back.
  vfs::BrowseInput right;
  right.right = true;
  CHECK(b.input(right) == vfs::BrowseAction::opened);
  CHECK(b.place() == 0 && b.files() == 0);
  vfs::BrowseInput left;
  left.left = true;
  CHECK(b.input(left) == vfs::BrowseAction::opened);
  CHECK(b.place() == 1);

  // A folder gone is said and the browser stays where it was.
  const std::string here = b.dir();
  CHECK(!b.open((t.root / "gone").string()));
  CHECK(b.dir() == here && !b.note().empty());
  vfs::BrowseInput back;
  back.back = true;
  CHECK(b.input(back) == vfs::BrowseAction::leave);
}

TEST(the_player_places_end_with_the_machines_roots) {
  const std::vector<vfs::Place> p = vfs::user_places();
  CHECK(!p.empty());
#ifndef _WIN32
  CHECK(p.back().name == "COMPUTER" && p.back().path == "/");
#endif
  CHECK(vfs::parent_folder("/") == "/");
  CHECK(vfs::parent_folder("/a/b") == "/a");
  CHECK(vfs::parent_folder("/a/b/") == "/a");
}

TEST(ages_and_sizes_read_as_a_player_reads_them) {
  CHECK(vfs::age_text(5) == "JUST NOW");
  CHECK(vfs::age_text(300) == "5 MIN AGO");
  CHECK(vfs::age_text(int64_t{3} * 3600) == "3 H AGO");
  CHECK(vfs::age_text(86400) == "1 DAY AGO");
  CHECK(vfs::age_text(int64_t{86400} * 90) == "3 MONTHS AGO");
  CHECK(vfs::size_text(812) == "812 B");
  CHECK(vfs::size_text(21800000) == "20.8 MB");
  CHECK(vfs::size_text(1024 * 1024 * 300) == "300 MB");
}
