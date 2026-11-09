// A FOLDER BROWSER'S LOGIC: the player's own folders walked from inside a game (a recording in
// Downloads, a map on a USB stick), listed with the files a game can open and nothing else, with
// no pixels drawn. A game draws its own page over it (its look, its words, its keys); the browser
// is the same in every game:
//
// - THE NAMES SHOWN (`named_as`): a file whose name ends in one of the game's extensions (any
//   case), or in one of them and then the extensions of what `unpack_one` peels (unpack.hpp:
//   `.gz`, `.zst`, `.xz`, `.tar`, `.tgz`, `.txz`, `.zip`, `.7z`, stacked: `match.mvd.gz`,
//   `final.dem.tar.gz`, `duel.mvd.7z`). A packed name with no game extension under its packing
//   (`maps.zip`) is not shown: an archive of anything says nothing of what it holds. The name only
//   chooses what is LISTED; what opens is told by the bytes (the game checks a file before it uses
//   it, so a misnamed one is refused with the reason, never misread).
// - THE LISTING (`list_folder`): `..` first (but at a root), then the folders by name, then the
//   files shown, newest first (by the file's time; then by name). Hidden entries (a leading dot)
//   are left out, and a folder the player may not read is said, never thrown.
// - THE PLACES (`user_places`): where a player keeps files: their home, Downloads, Desktop and
//   Documents (those that exist), and the machine's roots (`/` on macOS and Linux, each drive on
//   Windows). A game puts its own first (its folder, its recordings).
// - THE BROWSER (`FolderBrowser`): one folder open, the selection and the first row shown, a
//   frame's presses (`BrowseInput`) applied: up and down a row, a page, the ends; ENTER into a
//   folder or on a file (`chosen`); BACKSPACE (`parent`) up a folder, the folder left selected;
//   LEFT and RIGHT through the places; ESCAPE `leave`. A folder that does not open leaves the
//   browser where it was, the reason in `note`.
//
// ge:summary A folder browser's logic: places, folders and the files a game opens, packed or not
// ge:keywords browse browser file-browser folder-browser file-picker open-file downloads desktop
// ge:keywords documents home places drives list_folder named_as user_places folder-entry
// ge:keywords packed-name wrapper-extension gz zip 7z tar xz zst parent
// ge:skill vfs ui
// ge:test tests/vfs_browse_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ge::vfs {

// Whether `ext` (with its dot, any case) is a packing `unpack_one` peels, as a name ends in it:
// `.gz`, `.zst`, `.zstd`, `.xz`, `.tar`, `.tgz`, `.txz`, `.zip` or `.7z`.
bool wrapper_extension(std::string_view ext);

// Whether the file `name` is one of `extensions` (each with its dot), plain or packed (above).
bool named_as(std::string_view name, std::span<const std::string> extensions);

struct FolderEntry {
  std::string name;  // shown; ".." for the parent
  std::string path;  // the whole path, to open
  bool folder = false;
  uint64_t bytes = 0;  // a file's size
  int64_t age_s = 0;   // a file's age when listed, s (its last write; 0 for a folder)
};

// `dir`'s entries for `extensions` (above), into `out`. False with the reason and the next step
// in `error` when the folder does not open (`out` then empty).
bool list_folder(const std::string& dir, std::span<const std::string> extensions,
                 std::vector<FolderEntry>& out, std::string& error);

struct Place {
  std::string name;  // "HOME", "DOWNLOADS", "C:"
  std::string path;
};

// The player's places that exist on this machine (above), in that order.
std::vector<Place> user_places();

// `dir`'s parent; `dir` itself at a root.
std::string parent_folder(const std::string& dir);

// An age as a player reads it: "JUST NOW", "5 MIN AGO", "3 H AGO", "2 DAYS AGO", "4 MONTHS AGO".
std::string age_text(int64_t age_s);
// A size: "812 B", "12.4 KB", "20.8 MB", "1.2 GB".
std::string size_text(uint64_t bytes);

// One frame's presses on the browser.
struct BrowseInput {
  bool up = false, down = false, page_up = false, page_down = false, home = false, end = false;
  bool left = false, right = false;  // the previous or next place
  bool accept = false, parent = false, back = false;
  int pointed = -1;    // the entry under the pointer (-1 none)
  bool click = false;  // on `pointed`
};

enum class BrowseAction : uint8_t { none, moved, opened, chosen, leave };

class FolderBrowser {
 public:
  // The extensions shown (each with its dot).
  void set_extensions(std::vector<std::string> extensions) { extensions_ = std::move(extensions); }
  void set_places(std::vector<Place> places) { places_ = std::move(places); }
  const std::vector<Place>& places() const { return places_; }

  // `dir` listed and made the folder open, the selection on the first file (or on `select`, an
  // entry's name, when it is there). False with the reason in `note` when it does not open: the
  // browser stays where it was.
  bool open(const std::string& dir, std::string_view select = {});
  // The open folder listed again, the selection kept on the same name.
  void rescan();

  BrowseAction input(const BrowseInput& in);
  // The rows a page shows (a page up or down moves by it); the game's layout sets it.
  void set_page_rows(int rows) { page_rows_ = rows < 1 ? 1 : rows; }

  const std::string& dir() const { return dir_; }
  const std::vector<FolderEntry>& entries() const { return entries_; }
  int selected() const { return selected_; }
  const FolderEntry* selection() const;
  // The place the open folder is in (the longest place path it starts with), -1 none.
  int place() const;
  // The file ENTER chose (after BrowseAction::chosen).
  const std::string& chosen() const { return chosen_; }
  // Why the last folder did not open, or what a game said of the last file ("" none).
  const std::string& note() const { return note_; }
  void set_note(std::string note) { note_ = std::move(note); }
  // Files shown in the open folder.
  int files() const;

 private:
  void select_name(std::string_view name);

  std::vector<std::string> extensions_;
  std::vector<Place> places_;
  std::string dir_;
  std::vector<FolderEntry> entries_;
  int selected_ = 0;
  int page_rows_ = 10;
  std::string chosen_;
  std::string note_;
};

}  // namespace ge::vfs
