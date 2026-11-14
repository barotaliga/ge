// SAVES ON EVERY DEVICE (docs/saves.md § Saves on every device): a store's slots kept in step
// with the player's cloud, so a save made on one machine is there on the next. The cloud is any
// `Cloud` (the platform services' cloud files through `services::SaveCloud`, or a test's map);
// each slot is a cloud file of the same name under a prefix, written only after the local one is
// safe, beside a manifest (each slot's checksum, play time and date) so a sync reads one small
// file to know what changed.
//
// `sync` compares, slot by slot, the local file, the cloud's and the BASE (what both held when
// this device last synced, kept in the store's directory as `sync`): only the local one moved, it
// goes up; only the cloud's, it comes down (through `Store::write`, so it is as safe as any save);
// both moved, it is a CONFLICT: the one played longer (then the one saved later) wins on both
// sides, and the other is kept as `<slot>.conflict` (here and in the cloud) for the player to
// choose. A slot removed on one side since the base is removed on the other. The store's index
// is the store's own, rebuilt on each device from the slots it has.
//
// ge:summary A store's slots synced with the player's cloud: base, upload, download, conflict kept
// ge:keywords cloud sync cloud-save conflict base manifest upload download devices converge
// ge:skill save
// ge:test tests/save_cloud_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "save/disk.hpp"
#include "save/store.hpp"

namespace ge::save {

struct CloudFileInfo {
  std::string name;
  uint64_t bytes = 0;
};

// The player's cloud files, as a sync uses them. Each call false with `problem()` when the
// cloud refuses or cannot be reached.
class Cloud {
 public:
  virtual ~Cloud() = default;
  virtual bool list(std::vector<CloudFileInfo>& out) = 0;
  virtual bool read(const std::string& name, std::vector<uint8_t>& out) = 0;
  virtual bool write(const std::string& name, std::span<const uint8_t> bytes) = 0;
  virtual bool remove(const std::string& name) = 0;
  virtual const std::string& problem() const = 0;
};

// A cloud in memory (tests and labs): one map of files, shared by every device that syncs to it.
class MemCloud final : public Cloud {
 public:
  bool list(std::vector<CloudFileInfo>& out) override;
  bool read(const std::string& name, std::vector<uint8_t>& out) override;
  bool write(const std::string& name, std::span<const uint8_t> bytes) override;
  bool remove(const std::string& name) override;
  const std::string& problem() const override { return problem_; }
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
  bool offline = false;  // every call refused, as a cut connection is

 private:
  std::string problem_;
};

inline constexpr const char* kSyncName = "sync";          // the base, in the store's directory
inline constexpr const char* kManifestName = "manifest";  // in the cloud, under the prefix
inline constexpr uint32_t kSyncTag = tag("SYNC");
inline constexpr uint16_t kSyncVersion = 1;

// What a slot was at a sync: its checksum (xxh64 of the file), size, play time and date. A local
// slot whose size, play time and date are the base's is taken as unchanged without reading it.
struct SyncMark {
  std::string slot;
  uint64_t hash = 0;
  uint64_t bytes = 0;
  double play_s = 0;
  int64_t saved_unix = 0;
};

template <class Io>
void describe(Io& io, SyncMark& m) {
  io(m.slot);
  io(m.hash);
  io(m.bytes);
  io(m.play_s);
  io(m.saved_unix);
}

struct SyncReport {
  int uploaded = 0;
  int downloaded = 0;
  int removed_here = 0;                // slots removed here because the cloud no longer has them
  int removed_cloud = 0;               // and the other way
  std::vector<std::string> conflicts;  // slots both sides changed: the loser kept as .conflict
  int conflicts_fetched = 0;           // another device's .conflict files brought here
  int local_reads = 0;                 // local slots read to hash them (changed since the base)
  uint64_t cloud_bytes_written = 0;
  uint64_t cloud_bytes_read = 0;
};

// `store` (open, on `disk`) and the cloud's files under `prefix` ("saves/") brought to the same
// slots. False with `problem` when the cloud or the disk refuses; what was done before stays done
// and the next sync finishes it.
bool sync(Store& store, Disk& disk, Cloud& cloud, std::string_view prefix, SyncReport* report,
          std::string& problem);

}  // namespace ge::save
