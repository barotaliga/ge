// The screenshot gallery (engine/capture/gallery.hpp): the folder listed newest first by the time
// in the names (a count after it, a name with no time last, a half-written `.part` and other
// files left out), the time and size as text, the grid's moves and pages, the viewer and the
// confirm, the delete that keeps the selection in place, the thumbnails and the full picture
// loaded off the game's thread, a file that does not read marked, the folder listed again when it
// changes, and the cells a page's area holds.
//
// ge:summary Tests: the gallery lists, pages, moves, views, loads, marks and deletes screenshots
// ge:keywords test gallery screenshot list newest taken size grid page viewer confirm delete
// ge:keywords thumbnail load settle rescan cells
// ge:skill replay ui testing
// ge:test tests/capture_gallery_test.cpp
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "capture/gallery.hpp"
#include "capture/still.hpp"
#include "check.hpp"
#include "lab/png.hpp"

namespace fs = std::filesystem;
namespace gc = ge::capture;

namespace {

fs::path fresh_dir(const char* name) {
  const fs::path dir = fs::temp_directory_path() / name;
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  return dir;
}

// A `w` x `h` picture of one colour, `shade`, written as `name` (its format by the extension).
void still(const fs::path& dir, const std::string& name, int w, int h, uint8_t shade) {
  std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4, shade);
  for (size_t i = 3; i < rgba.size(); i += 4) rgba[i] = 255;
  gc::StillOptions o;
  std::string ext = fs::path(name).extension().string().substr(1);
  for (char& c : ext) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
  CHECK(gc::still_format_of(ext, o.format));
  std::vector<uint8_t> bytes;
  gc::StillFormat used{};
  std::string note;
  CHECK(gc::encode_still(o, w, h, rgba, bytes, used, note));
  CHECK(ge::lab::write_file((dir / name).string(), bytes));
}

// Seven screenshots, a name with no time, and files that are not stills.
fs::path seven(const char* name) {
  const fs::path dir = fresh_dir(name);
  still(dir, "game-2026-11-05-101500.jpg", 64, 36, 40);
  still(dir, "game-2026-11-05-101500-2.png", 64, 36, 60);
  still(dir, "game-2026-11-05-101500-10.png", 64, 36, 80);
  still(dir, "game-2026-11-04-235959.jpg", 64, 36, 100);
  still(dir, "game-2026-11-05-090000.png", 640, 360, 120);
  still(dir, "game-2026-11-05-120000.JPG", 64, 36, 140);
  still(dir, "holiday.png", 64, 36, 160);
  ge::lab::write_file((dir / "game-2026-11-05-130000.jpg.part").string(), std::vector<uint8_t>{1});
  ge::lab::write_file((dir / "notes.txt").string(), std::vector<uint8_t>{'h', 'i'});
  return dir;
}

}  // namespace

TEST(the_time_and_size_read_for_a_person) {
  CHECK(gc::taken_of("shooters_edge-2026-11-05-041233") == "2026-11-05 04:12:33");
  CHECK(gc::taken_of("fall_line-2026-01-02-030405-3") == "2026-01-02 03:04:05");
  CHECK(gc::taken_of("holiday").empty());
  CHECK(gc::size_text(812) == "812 B");
  CHECK(gc::size_text(uint64_t{412} * 1024) == "412 KB");
  CHECK(gc::size_text(1468006) == "1.4 MB");
}

TEST(the_folder_lists_newest_first_and_only_stills) {
  const fs::path dir = seven("ge_gallery_list");
  const std::vector<gc::GalleryEntry> list = gc::list_stills(dir.string());
  std::vector<std::string> got;
  got.reserve(list.size());
  for (const auto& e : list) got.push_back(e.name);
  const std::vector<std::string> want = {"game-2026-11-05-120000.JPG",
                                         "game-2026-11-05-101500-10.png",
                                         "game-2026-11-05-101500-2.png",
                                         "game-2026-11-05-101500.jpg",
                                         "game-2026-11-05-090000.png",
                                         "game-2026-11-04-235959.jpg",
                                         "holiday.png"};
  CHECK(got == want);
  CHECK(list.size() == 7 && list[0].format == gc::StillFormat::kJpg && list[0].bytes > 0);
  CHECK(list.size() == 7 && list[4].taken == "2026-11-05 09:00:00" && list[6].taken.empty());
  CHECK(gc::list_stills((dir / "missing").string()).empty());
}

TEST(the_grid_moves_by_cells_and_pages) {
  gc::Gallery g(seven("ge_gallery_grid").string());
  g.scan();
  g.set_grid(3, 2);
  g.reset();
  CHECK(g.selected() == 0 && g.page() == 0 && g.pages() == 2 && g.per_page() == 6);
  CHECK(g.input({.left = true}) == gc::GalleryAction::none);    // the row's first cell
  CHECK(g.input({.right = true}) == gc::GalleryAction::moved);  // 1
  CHECK(g.input({.right = true}) == gc::GalleryAction::moved);  // 2
  CHECK(g.input({.right = true}) == gc::GalleryAction::none);   // the row's last cell
  CHECK(g.input({.down = true}) == gc::GalleryAction::moved && g.selected() == 5);
  // Below 5 is nothing; the last row holds 6 alone, so down lands on it.
  CHECK(g.input({.down = true}) == gc::GalleryAction::moved && g.selected() == 6);
  CHECK(g.page() == 1 && g.first_on_page() == 6);
  CHECK(g.input({.down = true}) == gc::GalleryAction::none);
  CHECK(g.input({.up = true}) == gc::GalleryAction::moved && g.selected() == 3);
  CHECK(g.input({.pointed = 1}) == gc::GalleryAction::moved && g.selected() == 1);
  CHECK(g.input({.pointed = 4, .click = true}) == gc::GalleryAction::opened);
  CHECK(g.mode() == gc::GalleryMode::viewer && g.selected() == 4);
  CHECK(g.input({.back = true}) == gc::GalleryAction::closed);
  CHECK(g.input({.back = true}) == gc::GalleryAction::leave);
}

TEST(the_viewer_steps_and_the_cells_fit_the_area) {
  gc::Gallery g(seven("ge_gallery_viewer").string());
  g.scan();
  g.reset();
  CHECK(g.input({.accept = true}) == gc::GalleryAction::opened);
  CHECK(g.input({.left = true}) == gc::GalleryAction::none);
  CHECK(g.input({.right = true}) == gc::GalleryAction::moved && g.selected() == 1);
  CHECK(g.input({.accept = true}) == gc::GalleryAction::closed);
  const gc::GalleryCells c = gc::gallery_cells(1000, 500, 220, 16.0f / 9, 20);
  CHECK(c.columns == 4 && c.rows == 3);
  CHECK(c.cell_w * 4 + 20 * 3 <= 1000.5f && c.cell_h * 3 + 20 * 2 <= 500.5f);
  const gc::GalleryCells one = gc::gallery_cells(300, 80, 220, 16.0f / 9, 10);
  CHECK(one.columns == 1 && one.rows == 1 && one.cell_h <= 80.5f);
}

TEST(a_delete_asks_first_and_keeps_the_place) {
  const fs::path dir = seven("ge_gallery_delete");
  gc::Gallery g(dir.string());
  g.scan();
  g.reset();
  g.select(2);
  const std::string doomed = g.current()->path;
  CHECK(g.input({.del = true}) == gc::GalleryAction::asked);
  CHECK(g.mode() == gc::GalleryMode::confirm);
  CHECK(g.input({.right = true}) == gc::GalleryAction::none);  // the question waits
  CHECK(g.input({.back = true}) == gc::GalleryAction::kept && fs::exists(doomed));
  CHECK(g.input({.del = true}) == gc::GalleryAction::asked);
  CHECK(g.input({.accept = true}) == gc::GalleryAction::deleted);
  CHECK(!fs::exists(doomed) && g.count() == 6 && g.selected() == 2);
  CHECK(g.mode() == gc::GalleryMode::grid && g.error().empty());
  // From the viewer, on the last: the selection steps back and the viewer stays.
  g.select(5);
  g.input({.accept = true});
  g.input({.del = true});
  CHECK(g.input({.accept = true}) == gc::GalleryAction::deleted);
  CHECK(g.selected() == 4 && g.mode() == gc::GalleryMode::viewer);
  // A file gone from under it: the delete fails and says so.
  fs::remove(g.current()->path);
  g.input({.del = true});
  CHECK(g.input({.accept = true}) == gc::GalleryAction::failed);
  CHECK_MSG(g.error().find("could not be deleted") != std::string::npos, g.error());
  CHECK(g.count() == 4);  // listed again
}

TEST(the_pictures_load_off_the_thread_and_a_bad_file_is_marked) {
  const fs::path dir = seven("ge_gallery_load");
  ge::lab::write_file((dir / "game-2026-11-06-000000.png").string(),
                      std::vector<uint8_t>{0x89, 'P', 'N', 'G', 0, 0, 0, 0, 1, 2});
  gc::Gallery g(dir.string(), 32);
  g.set_grid(4, 2);
  g.settle();  // listed by the first poll
  CHECK(g.count() == 8);
  CHECK(!g.entries()[0].error.empty() && g.thumb(0) == nullptr);
  for (int i = 1; i < 8; ++i) {
    const gc::GalleryPicture* t = g.thumb(i);
    CHECK(t != nullptr);
    if (t != nullptr) CHECK(t->picture.width <= 32 && t->stamp > 0);
  }
  CHECK(g.entries()[5].width == 640 && g.entries()[5].height == 360);  // the 09:00 one
  CHECK(g.picture() == nullptr);                                       // the grid holds none
  g.select(5);
  g.input({.accept = true});
  g.settle();
  CHECK(g.picture() != nullptr && g.picture()->picture.width == 640);
  // A screenshot taken meanwhile is listed by the next poll, the selection kept on its file.
  const std::string on = g.current()->name;
  still(dir, "game-2026-11-07-000000.jpg", 32, 18, 200);
  fs::last_write_time(dir, fs::last_write_time(dir) + std::chrono::seconds(2));
  g.poll();
  CHECK(g.count() == 9 && g.current()->name == on);
}
