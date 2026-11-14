// THE SAVE STORE (docs/saves.md § Slots and the store): a game's saves in one directory, a slot a
// file, written so a crash never costs the last good save, listed by one small read.
//
// THE SLOTS: `auto-<n>` the autosave ring (`StoreSpec::autosaves` of them; a new autosave takes an
// empty one, else the one saved longest ago), `quick` the quicksave, `slot-<n>` the player's
// numbered slots (1..`StoreSpec::slots`). Any other name is refused.
//
// A WRITE (`write`): the bytes (a whole save file, file.hpp) are checked to open as one of this
// game's; then they go to `<slot>.tmp`, flushed to the device; read back and checked again; the
// slot as it stood renamed to `<slot>.bak`; the temporary renamed over the slot; the directory
// flushed; the `.bak` removed; the index rewritten. Killed at any point, the files left hold the
// old save or the new one whole, and `open` puts them right: a `.tmp` is removed, a slot missing
// beside its `.bak` gets the `.bak` back, a slot beside a `.bak` is kept when it opens and replaced
// by the `.bak` when it does not.
//
// THE INDEX (`index`, a save file of kind `index`: one `SLOT` chunk an entry) is what `open`
// reads: the directory is listed (names and sizes, no file read) and only a slot the index does
// not name, or names at another size, is peeked (its header and meta); a slot gone is dropped.
// Missing or damaged, the index is rebuilt from the slots' headers. So a menu of a thousand saves
// costs one read.
//
// AUTOSAVES are spaced: `autosave_due(now)` is false within `StoreSpec::autosave_spacing_s` of the
// last one (by the game's play clock), so a burst of the game's triggers writes once.
//
// A Store is not shared between threads; `StoreWriter` (store_writer.hpp) runs one on a thread
// of its own. Every refusal is one line naming the slot, what failed and the next action.
//
// ge:summary The save store: slots in one directory, safe writes with the last good kept, the index
// ge:keywords save store slot autosave quicksave ring index tmp bak rename atomic recover rebuild
// ge:keywords delete copy rename list menu last-good crash power-loss autosave-spacing
// ge:skill save
// ge:test tests/save_store_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "save/disk.hpp"
#include "save/file.hpp"

namespace ge::save {

inline constexpr uint32_t kSlotTag = tag("SLOT");
inline constexpr uint16_t kSlotVersion = 1;
inline constexpr uint32_t kIndexVersion = 1;  // the index file's game version
inline constexpr const char* kIndexName = "index";

struct StoreSpec {
  std::string game;                // the game's name: another game's saves are refused
  int autosaves = 3;               // the autosave ring's size
  int slots = 100;                 // numbered slots, 1..slots
  double autosave_spacing_s = 30;  // the least play time between two autosaves, s
};

// A slot as the store lists it.
struct Entry {
  std::string slot;    // "auto-2", "quick", "slot-17"
  Meta meta;           // its meta chunk
  uint64_t bytes = 0;  // the file's size
};

template <class Io>
void describe(Io& io, Entry& e) {
  io(e.slot);
  io(e.meta);
  io(e.bytes);
}

// What `open` found and did.
struct StoreOpen {
  bool rebuilt = false;              // the index was missing or damaged: every slot was peeked
  int peeked = 0;                    // slots read for their header (not in the index, or changed)
  int recovered = 0;                 // slots given their `.bak` back
  int cleared = 0;                   // `.tmp` and stale `.bak` files removed
  std::vector<std::string> damaged;  // slots whose files do not open (left on the disk, unlisted)
};

class Store {
 public:
  Store(Disk& disk, StoreSpec spec);

  // Puts a killed write right, then reads the index (or rebuilds it). False when the directory
  // does not list or the index cannot be written; `problem()` says why.
  bool open(StoreOpen* report = nullptr);

  // Every slot, in the order `slot-1..`, `quick`, `auto-1..`; the slot `slot`, or null.
  const std::vector<Entry>& list() const { return entries_; }
  const Entry* find(std::string_view slot) const;

  // `file` (a whole save file) written into `slot` by the steps above.
  bool write(std::string_view slot, std::span<const uint8_t> file);
  // The slot's file, checked to open.
  bool read(std::string_view slot, std::vector<uint8_t>& out);
  bool remove(std::string_view slot);
  bool copy(std::string_view from, std::string_view to);
  bool rename(std::string_view from, std::string_view to);

  // The ring's slot the next autosave takes, and whether one is due at play time `now_s`;
  // `autosave` writes it there and starts the spacing.
  std::string autosave_slot() const;
  bool autosave_due(double now_s) const;
  bool autosave(std::span<const uint8_t> file, double now_s);

  const std::string& problem() const { return problem_; }
  const StoreSpec& spec() const { return spec_; }

  // True for the names a slot may have under `spec`.
  static bool valid_slot(const StoreSpec& spec, std::string_view slot);

 private:
  bool fail(std::string what, std::string_view next);
  bool check(std::string_view slot, std::span<const uint8_t> file, Entry* entry);
  bool save_index();
  void put(Entry e);
  Disk* disk_;
  StoreSpec spec_;
  std::vector<Entry> entries_;
  std::vector<uint8_t> scratch_;
  double last_autosave_s_ = -1e300;
  std::string problem_;
};

}  // namespace ge::save
