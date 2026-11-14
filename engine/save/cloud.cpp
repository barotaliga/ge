// A store's slots synced with the player's cloud (cloud.hpp): the base, the manifest, the three-way
// compare per slot and the conflict kept.
//
// ge:summary A store synced with a cloud: the base, the manifest, each slot compared three ways
// ge:keywords cloud sync manifest base conflict upload download mem-cloud
// ge:skill save
// ge:test tests/save_cloud_test.cpp
#include "save/cloud.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <set>

#include "save/xxh64.hpp"

namespace ge::save {

namespace {

constexpr std::string_view kConflict = ".conflict";

bool starts_with(std::string_view s, std::string_view start) {
  return s.size() >= start.size() && s.substr(0, start.size()) == start;
}
bool ends_with(std::string_view s, std::string_view end) {
  return s.size() >= end.size() && s.substr(s.size() - end.size()) == end;
}

uint64_t hash_of(std::span<const uint8_t> bytes) { return xxh64(bytes.data(), bytes.size()); }

using Marks = std::map<std::string, SyncMark>;

// A marks file (the base or the manifest): an index-kind save of SYNC chunks.
void write_marks(const Marks& marks, std::string_view game, std::string_view title,
                 std::vector<uint8_t>& out) {
  Meta m;
  m.game = std::string(game);
  m.game_version = kSyncVersion;
  m.kind = Kind::index;
  m.title = std::string(title);
  FileWriter w(out, m);
  uint32_t i = 0;
  for (const auto& [slot, mark] : marks) w.put(kSyncTag, i++, kSyncVersion, mark);
  w.finish();
}

// False when the bytes are not a marks file of `game`: the caller then knows nothing.
bool read_marks(std::span<const uint8_t> bytes, std::string_view name, std::string_view game,
                Marks& out) {
  out.clear();
  FileReader r;
  if (!r.open(bytes, name, game) || r.header().kind != Kind::index) return false;
  for (const ChunkInfo& c : r.chunks()) {
    if (c.tag != kSyncTag) continue;
    SyncMark mark;
    Reader in = r.reader(c);
    in(mark);
    if (c.version > kSyncVersion || !in.ok() || !in.at_end()) {
      out.clear();
      return false;
    }
    std::string slot = mark.slot;
    out.emplace(std::move(slot), std::move(mark));
  }
  return true;
}

// A mark for a whole save file's bytes, its play time and date from its meta.
bool mark_of(std::string_view slot, std::span<const uint8_t> bytes, std::string_view game,
             SyncMark& mark) {
  Header h;
  Meta m;
  std::string why;
  if (!peek(bytes, h, m, why) || m.game != game) return false;
  mark = {std::string(slot), hash_of(bytes), bytes.size(), m.play_s, m.saved_unix};
  return true;
}

// The one of two saves of a slot that wins a conflict: played longer, then saved later, then the
// greater checksum (so both devices pick the same one whatever the order they sync in).
bool local_wins(const SyncMark& local, const SyncMark& cloud) {
  if (local.play_s != cloud.play_s) return local.play_s > cloud.play_s;
  if (local.saved_unix != cloud.saved_unix) return local.saved_unix > cloud.saved_unix;
  return local.hash > cloud.hash;
}

bool same(const std::optional<SyncMark>& a, const std::optional<SyncMark>& b) {
  if (!a || !b) return !a && !b;
  return a->hash == b->hash;
}

class Sync {
 public:
  Sync(Store& store, Disk& disk, Cloud& cloud, std::string_view prefix, SyncReport& report,
       std::string& problem)
      : store_(store),
        disk_(disk),
        cloud_(cloud),
        prefix_(prefix),
        rep_(report),
        problem_(problem) {}

  bool run();

 private:
  bool fail(std::string what, std::string_view next) {
    problem_ = std::move(what) + "; next: " + std::string(next);
    return false;
  }
  bool cloud_fail(const std::string& doing) {
    return fail("the cloud refused " + doing + ": " + cloud_.problem(),
                "sync again when the cloud can be reached; the saves on this device are untouched");
  }
  bool cloud_read(const std::string& name, std::vector<uint8_t>& out) {
    if (!cloud_.read(name, out)) return cloud_fail("reading " + name);
    rep_.cloud_bytes_read += out.size();
    return true;
  }
  bool cloud_write(const std::string& name, std::span<const uint8_t> bytes) {
    if (!cloud_.write(name, bytes)) return cloud_fail("writing " + name);
    rep_.cloud_bytes_written += bytes.size();
    return true;
  }
  bool store_fail(const std::string& doing) {
    return fail("the store refused " + doing + ": " + store_.problem(),
                "free space on the disk and sync again");
  }
  bool disk_write(const std::string& name, std::span<const uint8_t> bytes) {
    const std::string tmp = name + ".tmp";
    if (!disk_.write(tmp, bytes) || !disk_.rename(tmp, name) || !disk_.sync())
      return fail("writing " + name + " here stopped: " + disk_.problem(),
                  "free space on the disk and sync again");
    return true;
  }
  bool read_cloud_marks();
  bool read_local_marks();
  bool settle(const std::string& slot);
  bool keep_conflict(const std::string& slot, std::span<const uint8_t> loser);
  bool fetch_conflicts();

  Store& store_;
  Disk& disk_;
  Cloud& cloud_;
  std::string prefix_;
  SyncReport& rep_;
  std::string& problem_;
  std::set<std::string> cloud_names_;  // under the prefix, the prefix taken off
  std::set<std::string> unreadable_;   // cloud slots that are not this game's saves
  Marks base_, cloud_marks_, local_marks_, next_;
  bool cloud_changed_ = false;
  std::vector<uint8_t> bytes_, other_;
};

bool Sync::read_cloud_marks() {
  std::vector<CloudFileInfo> files;
  if (!cloud_.list(files)) return cloud_fail("listing its files");
  for (const CloudFileInfo& f : files)
    if (starts_with(f.name, prefix_)) cloud_names_.insert(f.name.substr(prefix_.size()));
  Marks listed;
  if (cloud_names_.count(kManifestName) != 0) {
    if (!cloud_read(prefix_ + kManifestName, bytes_)) return false;
    if (!read_marks(bytes_, kManifestName, store_.spec().game, listed)) listed.clear();
  }
  // The manifest's word for each slot the cloud holds; a slot it does not name (a device stopped
  // between the upload and the manifest) is read and hashed.
  for (const std::string& name : cloud_names_) {
    if (!Store::valid_slot(store_.spec(), name)) continue;
    if (const auto it = listed.find(name); it != listed.end()) {
      cloud_marks_.emplace(name, it->second);
      continue;
    }
    if (!cloud_read(prefix_ + name, bytes_)) return false;
    SyncMark mark;
    if (mark_of(name, bytes_, store_.spec().game, mark))
      cloud_marks_.emplace(name, mark);
    else
      unreadable_.insert(name);  // left as it is on both sides: never read as a removal
    cloud_changed_ = true;
  }
  if (listed.size() != cloud_marks_.size()) cloud_changed_ = true;  // it named a slot now gone
  return true;
}

bool Sync::read_local_marks() {
  if (disk_.read(kSyncName, bytes_) && !read_marks(bytes_, kSyncName, store_.spec().game, base_))
    base_.clear();
  for (const Entry& e : store_.list()) {
    const auto b = base_.find(e.slot);
    if (b != base_.end() && b->second.bytes == e.bytes && b->second.play_s == e.meta.play_s &&
        b->second.saved_unix == e.meta.saved_unix) {
      local_marks_.emplace(e.slot, b->second);  // as it was at the last sync
      continue;
    }
    if (!store_.read(e.slot, bytes_)) return store_fail("reading " + e.slot);
    rep_.local_reads++;
    local_marks_.emplace(
        e.slot, SyncMark{e.slot, hash_of(bytes_), bytes_.size(), e.meta.play_s, e.meta.saved_unix});
  }
  return true;
}

bool Sync::keep_conflict(const std::string& slot, std::span<const uint8_t> loser) {
  const std::string name = slot + std::string(kConflict);
  if (!disk_write(name, loser) || !cloud_write(prefix_ + name, loser)) return false;
  rep_.conflicts.push_back(slot);
  return true;
}

bool Sync::settle(const std::string& slot) {
  const auto at = [&](const Marks& m) -> std::optional<SyncMark> {
    const auto it = m.find(slot);
    return it == m.end() ? std::nullopt : std::optional<SyncMark>(it->second);
  };
  const std::optional<SyncMark> l = at(local_marks_), c = at(cloud_marks_), b = at(base_);
  const auto keep = [&](const std::optional<SyncMark>& m) {
    if (m) next_[slot] = *m;
  };
  const auto upload = [&]() {
    if (!store_.read(slot, bytes_)) return store_fail("reading " + slot);
    if (!cloud_write(prefix_ + slot, bytes_)) return false;
    rep_.uploaded++;
    cloud_changed_ = true;
    keep(l);
    return true;
  };
  const auto download = [&]() {
    if (!cloud_read(prefix_ + slot, bytes_)) return false;
    if (!store_.write(slot, bytes_)) return store_fail("the cloud's save for " + slot);
    rep_.downloaded++;
    keep(c);
    return true;
  };
  if (same(l, c)) {  // in step already (or gone from both)
    keep(l);
    return true;
  }
  if (same(l, b)) {  // only the cloud moved
    if (c) return download();
    if (!store_.remove(slot)) return store_fail("removing " + slot);
    rep_.removed_here++;
    return true;
  }
  if (same(c, b)) {  // only this device moved
    if (l) return upload();
    if (!cloud_.remove(prefix_ + slot)) return cloud_fail("removing " + slot);
    rep_.removed_cloud++;
    cloud_changed_ = true;
    return true;
  }
  // Both moved. A save beats a removal; two saves: the winner on both sides, the loser kept.
  if (!c) return upload();
  if (!l) return download();
  if (local_wins(*l, *c)) {
    if (!cloud_read(prefix_ + slot, other_) || !keep_conflict(slot, other_)) return false;
    return upload();
  }
  if (!store_.read(slot, other_)) return store_fail("reading " + slot);
  if (!keep_conflict(slot, other_)) return false;
  return download();
}

bool Sync::fetch_conflicts() {
  std::set<std::string> here;
  for (const DiskFile& f : disk_.list()) here.insert(f.name);
  for (const std::string& name : cloud_names_) {
    if (!ends_with(name, kConflict) || here.count(name) != 0) continue;
    if (!cloud_read(prefix_ + name, bytes_) || !disk_write(name, bytes_)) return false;
    rep_.conflicts_fetched++;
  }
  return true;
}

bool Sync::run() {
  if (!read_cloud_marks() || !read_local_marks()) return false;
  std::set<std::string> slots;
  for (const Marks* m : {&local_marks_, &cloud_marks_, &base_})
    for (const auto& [slot, mark] : *m) slots.insert(slot);
  for (const std::string& slot : slots)
    if (unreadable_.count(slot) == 0 && !settle(slot)) return false;
  if (!fetch_conflicts()) return false;
  // The cloud's manifest, then this device's base: both now the slots as they stand.
  write_marks(next_, store_.spec().game, "the cloud's saves", bytes_);
  if ((cloud_changed_ || cloud_names_.count(kManifestName) == 0) &&
      !cloud_write(prefix_ + kManifestName, bytes_))
    return false;
  return disk_write(kSyncName, bytes_);
}

}  // namespace

bool MemCloud::list(std::vector<CloudFileInfo>& out) {
  out.clear();
  if (offline) {
    problem_ = "the cloud cannot be reached";
    return false;
  }
  for (const auto& [name, bytes] : files) out.push_back({name, bytes.size()});
  return true;
}

bool MemCloud::read(const std::string& name, std::vector<uint8_t>& out) {
  if (offline) {
    problem_ = "the cloud cannot be reached";
    return false;
  }
  for (const auto& [n, bytes] : files)
    if (n == name) {
      out = bytes;
      return true;
    }
  problem_ = "there is no file " + name;
  return false;
}

bool MemCloud::write(const std::string& name, std::span<const uint8_t> bytes) {
  if (offline) {
    problem_ = "the cloud cannot be reached";
    return false;
  }
  for (auto& [n, b] : files)
    if (n == name) {
      b.assign(bytes.begin(), bytes.end());
      return true;
    }
  files.emplace_back(name, std::vector<uint8_t>(bytes.begin(), bytes.end()));
  return true;
}

bool MemCloud::remove(const std::string& name) {
  if (offline) {
    problem_ = "the cloud cannot be reached";
    return false;
  }
  const auto it =
      std::find_if(files.begin(), files.end(), [&](const auto& f) { return f.first == name; });
  if (it == files.end()) {
    problem_ = "there is no file " + name;
    return false;
  }
  files.erase(it);
  return true;
}

bool sync(Store& store, Disk& disk, Cloud& cloud, std::string_view prefix, SyncReport* report,
          std::string& problem) {
  SyncReport local;
  SyncReport& rep = report != nullptr ? *report : local;
  rep = {};
  problem.clear();
  return Sync(store, disk, cloud, prefix, rep, problem).run();
}

}  // namespace ge::save
