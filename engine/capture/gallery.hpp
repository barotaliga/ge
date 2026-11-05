// THE SCREENSHOT GALLERY'S LOGIC (docs/replays.md § The gallery): the screenshots a game took
// (capture/screenshot.hpp) listed, looked through, loaded and deleted, with no pixels drawn. Each
// game draws its own page over it (its look, its words, its keys); the gallery is the same in
// every game:
//
// - THE LIST: every still in the folder (png, jpg, avif, heic; a `.part` being written is not one)
//   newest first, by the time in its name (`<prefix>-YYYY-MM-DD-HHMMSS[-n]`, `taken_of`), the
//   rest after by name. Each entry carries its file's name, format, size in bytes and the time it
//   was taken; its pixel size once a load has read it. `poll` lists the folder again when its
//   modification time moves (F12 pressed with the gallery up, a file deleted outside), keeping the
//   selection on the same file.
// - THE MODES: `grid` (pages of `columns` x `rows` thumbnails the game's layout chose with
//   `set_grid`), `viewer` (one picture at full size) and `confirm` (asked whether to delete the
//   selected one). `input` takes one frame's presses (arrows, accept, back, delete, the cell under
//   the pointer and a click) and says what they did (`GalleryAction`), so the game plays a sound
//   or leaves its page on `leave`.
// - THE LOADS: a worker thread of its own reads and decodes the files the page shows
//   (`decode_still`), the grid's page and the pages either side of it as thumbnails shrunk to
//   `thumb_px` across, the viewer's picture and its neighbours at full size, most wanted first.
//   `poll` (once a frame, on the game's thread) takes in what came back and asks for what the
//   page shows now; what is far from the selection is let go. A picture's `stamp` changes each
//   time its pixels do, so a game copies it into its texture only then. A file that does not read
//   is marked with the reason (`GalleryEntry::error`) and not asked for again.
// - DELETING removes the file, never anything that is not a still in the folder, and keeps the
//   selection at the same place (the next one; the last when it was the last).
//
// ge:summary The screenshot gallery's logic: list, page, view, load thumbnails off-thread, delete
// ge:keywords gallery screenshot screenshots-folder thumbnail thumb viewer grid page browse
// ge:keywords delete confirm navigate select list_stills taken_of size_text gallery_cells
// ge:keywords gallery_entry gallery-input poll settle stamp worker decode
// ge:keywords photo-mode album pictures-taken
// ge:skill replay ui
// ge:test tests/capture_gallery_test.cpp
#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "capture/still.hpp"
#include "lab/raster.hpp"

namespace ge::capture {

struct GalleryEntry {
  std::string name;  // the file's name, with its extension
  std::string path;
  StillFormat format = StillFormat::kJpg;
  uint64_t bytes = 0;
  std::string taken;          // "YYYY-MM-DD HH:MM:SS" from the name; "" when it holds no time
  int width = 0, height = 0;  // the picture's, once a load has read it; 0 before
  std::string error;          // why the file does not read; "" while it does or is not read yet
};

// The stills in `dir`, newest first; empty when the folder is missing.
std::vector<GalleryEntry> list_stills(const std::string& dir);
// The time a screenshot's name holds (`<prefix>-YYYY-MM-DD-HHMMSS`, a `-<n>` count after it) as
// "YYYY-MM-DD HH:MM:SS"; "" when the stem holds none.
std::string taken_of(std::string_view stem);
// A file's size for a person: "812 B", "412 KB", "1.4 MB".
std::string size_text(uint64_t bytes);

// A grid of cells of `aspect` (width / height) in an area: as many columns as fit cells at least
// `min_cell_w` px wide (one at the least), as many rows as fit their height, `gap` px between.
struct GalleryCells {
  int columns = 1, rows = 1;
  float cell_w = 0, cell_h = 0, gap = 0;
};
GalleryCells gallery_cells(float area_w, float area_h, float min_cell_w, float aspect, float gap);

enum class GalleryMode : uint8_t { grid, viewer, confirm };

// One frame's presses, each an edge (true on the frame it was pressed).
struct GalleryInput {
  bool up = false, down = false, left = false, right = false;
  bool accept = false;  // grid: view the selected; viewer: back to the grid; confirm: delete
  bool back = false;    // grid: leave the gallery; viewer: back to the grid; confirm: keep it
  bool del = false;     // grid or viewer: ask to delete the selected
  int pointed = -1;     // the entry under the pointer when it moved this frame; -1 none
  bool click = false;   // the pointer's button released over `pointed`: viewed (grid)
};

// What one frame's presses did.
enum class GalleryAction : uint8_t {
  none,
  moved,    // the selection moved (or the viewer stepped)
  opened,   // the viewer opened on the selected
  closed,   // the viewer closed, back to the grid
  asked,    // asked whether to delete the selected
  deleted,  // the file was deleted
  kept,     // the delete was called off
  failed,   // the delete failed: `error` says why
  leave,    // back on the grid: the game leaves the gallery
};

// A picture the gallery holds, and a stamp that changes each time its pixels do.
struct GalleryPicture {
  lab::Picture picture;
  uint64_t stamp = 0;
};

class Gallery {
 public:
  static constexpr int kDefaultThumbPx = 320;

  // The stills in `dir`, thumbnails shrunk to `thumb_px` across. Nothing is listed until `scan`
  // or `poll`.
  explicit Gallery(std::string dir = {}, int thumb_px = kDefaultThumbPx);
  Gallery(const Gallery&) = delete;
  Gallery& operator=(const Gallery&) = delete;
  ~Gallery();  // the load in hand finished, the rest dropped

  // Another folder: the list and every picture dropped, listed again by the next `poll`.
  void set_dir(std::string dir);
  const std::string& dir() const { return dir_; }

  // The folder listed again now; the selection stays on the same file when it is still there.
  void scan();
  const std::vector<GalleryEntry>& entries() const { return entries_; }
  int count() const { return static_cast<int>(entries_.size()); }
  // The selected entry's index, -1 when there is none; its entry, null then.
  int selected() const { return selected_; }
  const GalleryEntry* current() const;
  // The selection set (clamped to the list) or moved by `step` (clamped, no wrap).
  void select(int index);
  void step(int by);

  // The grid the game's layout shows: `columns` x `rows` cells a page (each at least 1).
  void set_grid(int columns, int rows);
  int columns() const { return columns_; }
  int rows() const { return rows_; }
  int per_page() const { return columns_ * rows_; }
  // The page the selection is on, how many there are (1 when the list is empty), its first entry.
  int page() const;
  int pages() const;
  int first_on_page() const { return page() * per_page(); }

  GalleryMode mode() const { return mode_; }
  // The grid on the first page, the selection on the newest (opening the gallery's page).
  void reset();
  // One frame's presses: see GalleryInput and GalleryAction.
  GalleryAction input(const GalleryInput& in);
  // The selected file deleted now, with no question asked (the confirm's accept); false with
  // the reason in `error`.
  bool remove_selected();
  // Why the last delete failed; "" after one that worked.
  const std::string& error() const { return error_; }

  // Once a frame on the game's thread: the folder listed again when it changed, the loads that
  // came back taken in, the loads the page shows now asked for. True when a picture arrived.
  bool poll();
  // `poll` until every picture the page shows is loaded or failed (a lab, a test).
  void settle();
  // The entry's thumbnail, or the selected entry's full picture; null while it is loading (or
  // when it failed: its entry's `error`).
  const GalleryPicture* thumb(int index) const;
  const GalleryPicture* picture() const;

 private:
  struct Load {
    std::string path;
    bool full = false;
  };
  struct Loaded {
    std::string path;
    bool full = false;
    lab::Picture picture;
    int width = 0, height = 0;
    std::string error;
  };
  void work();
  void want(std::vector<Load>& out, int index, bool full) const;
  bool held(const std::string& path, bool full) const;
  void forget_far();
  std::filesystem::file_time_type dir_time() const;

  std::string dir_;
  int thumb_px_ = kDefaultThumbPx;
  std::vector<GalleryEntry> entries_;
  bool listed_ = false;
  std::filesystem::file_time_type listed_time_{};
  int selected_ = -1;
  int columns_ = 1, rows_ = 1;
  GalleryMode mode_ = GalleryMode::grid;
  GalleryMode before_confirm_ = GalleryMode::grid;
  std::string error_;
  uint64_t stamp_ = 0;
  std::map<std::string, GalleryPicture> thumbs_;  // by path
  std::map<std::string, GalleryPicture> fulls_;   // by path

  std::mutex mutex_;
  std::condition_variable wake_, idle_;
  std::deque<Load> queue_;    // most wanted first; replaced by every poll
  std::vector<Loaded> done_;  // back from the worker, not taken in yet
  std::string busy_path_;     // the load in hand ("" none)
  bool busy_full_ = false;
  bool stop_ = false;
  std::thread worker_;  // started by the first load asked for
};

}  // namespace ge::capture
