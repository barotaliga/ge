// The save store (store.hpp): slots, safe writes, recovery and the index.
//
// ge:summary The save store's slots written safely, a killed write put right, the index kept
// ge:keywords store write recover index rebuild autosave-ring
// ge:skill save
// ge:test tests/save_store_test.cpp
#include "save/store.hpp"

#include <algorithm>
#include <charconv>
#include <set>
#include <unordered_map>

namespace ge::save {

namespace {

constexpr std::string_view kTmp = ".tmp", kBak = ".bak";

bool ends_with(std::string_view s, std::string_view end) {
  return s.size() >= end.size() && s.substr(s.size() - end.size()) == end;
}

// "slot-17" -> 17 after `prefix`, else 0.
int number_after(std::string_view s, std::string_view prefix) {
  if (s.size() <= prefix.size() || s.substr(0, prefix.size()) != prefix) return 0;
  const std::string_view digits = s.substr(prefix.size());
  if (digits[0] == '0') return 0;
  int n = 0;
  const auto [end, ec] = std::from_chars(digits.data(), digits.data() + digits.size(), n);
  return ec == std::errc{} && end == digits.data() + digits.size() ? n : 0;
}

// The order a menu lists: the numbered slots, the quicksave, the autosaves.
std::pair<int, int> order(std::string_view slot) {
  if (const int n = number_after(slot, "slot-"); n > 0) return {0, n};
  if (slot == "quick") return {1, 0};
  return {2, number_after(slot, "auto-")};
}

std::string text(std::string_view s) { return std::string(s); }

}  // namespace

bool Store::valid_slot(const StoreSpec& spec, std::string_view slot) {
  if (slot == "quick") return true;
  const int n = number_after(slot, "slot-");
  if (n > 0) return n <= spec.slots;
  const int a = number_after(slot, "auto-");
  return a > 0 && a <= spec.autosaves;
}

Store::Store(Disk& disk, StoreSpec spec) : disk_(&disk), spec_(std::move(spec)) {}

bool Store::fail(std::string what, std::string_view next) {
  problem_ = std::move(what);
  if (!next.empty()) problem_ += "; next: " + text(next);
  return false;
}

const Entry* Store::find(std::string_view slot) const {
  for (const Entry& e : entries_)
    if (e.slot == slot) return &e;
  return nullptr;
}

void Store::put(Entry e) {
  for (Entry& have : entries_)
    if (have.slot == e.slot) {
      have = std::move(e);
      return;
    }
  entries_.push_back(std::move(e));
  std::sort(entries_.begin(), entries_.end(),
            [](const Entry& a, const Entry& b) { return order(a.slot) < order(b.slot); });
}

bool Store::check(std::string_view slot, std::span<const uint8_t> file, Entry* entry) {
  FileReader r;
  if (!r.open(file, slot, spec_.game))
    return fail("the save for " + text(slot) + " does not open: " + r.problem(),
                "write the save again from the run (Run::save)");
  if (r.header().kind == Kind::index)
    return fail("an index is not a save for " + text(slot), "write a run's save there");
  if (entry != nullptr) {
    entry->slot = text(slot);
    entry->meta = r.meta();
    entry->bytes = file.size();
  }
  return true;
}

bool Store::save_index() {
  Meta m;
  m.game = spec_.game;
  m.game_version = kIndexVersion;
  m.kind = Kind::index;
  m.title = "the store's index";
  FileWriter w(scratch_, m);
  for (size_t i = 0; i < entries_.size(); i++)
    w.put(kSlotTag, static_cast<uint32_t>(i), kSlotVersion, entries_[i]);
  std::string why;
  if (!w.finish(&why)) return fail("the index does not write: " + why, "report it as a bug");
  const std::string tmp = text(kIndexName) + text(kTmp);
  if (!disk_->write(tmp, scratch_) || !disk_->rename(tmp, kIndexName))
    return fail("the index is not written: " + disk_->problem(),
                "free space on the disk; the saves themselves are safe and the index is rebuilt "
                "on the next open");
  return true;
}

bool Store::open(StoreOpen* report) {
  StoreOpen local;
  StoreOpen& rep = report != nullptr ? *report : local;
  rep = {};
  problem_.clear();
  entries_.clear();
  std::vector<DiskFile> files = disk_->list();
  if (!disk_->problem().empty())
    return fail("the saves' directory does not list: " + disk_->problem(),
                "check that the directory exists and may be read");
  // A killed write put right: the temporaries go, a slot whose replacing failed gets its old file.
  std::set<std::string> names;
  for (const DiskFile& f : files) names.insert(f.name);
  for (const DiskFile& f : files) {
    if (ends_with(f.name, kTmp)) {
      if (disk_->remove(f.name)) rep.cleared++;
      names.erase(f.name);
    }
  }
  for (const DiskFile& f : files) {
    if (!ends_with(f.name, kBak)) continue;
    const std::string slot = f.name.substr(0, f.name.size() - kBak.size());
    if (!valid_slot(spec_, slot)) continue;
    // A slot beside its .bak that opens is the new one, whole: the old one goes. No slot (killed
    // between the two renames) or one that does not open: the old save is put back.
    const bool new_whole =
        names.count(slot) != 0 && disk_->read(slot, scratch_) && check(slot, scratch_, nullptr);
    if (new_whole) {
      if (disk_->remove(f.name)) rep.cleared++;
    } else {
      if (!disk_->rename(f.name, slot))
        return fail(f.name + " cannot be put back: " + disk_->problem(),
                    "rename it to " + slot + " by hand");
      rep.recovered++;
    }
  }
  problem_.clear();
  if (rep.cleared > 0 || rep.recovered > 0) {
    files = disk_->list();
    if (!disk_->problem().empty())
      return fail("the saves' directory does not list: " + disk_->problem(),
                  "check that the directory exists and may be read");
  }
  // The index, then whatever it does not know.
  std::unordered_map<std::string, Entry> indexed;
  bool have_index = false;
  if (disk_->read(kIndexName, scratch_)) {
    FileReader r;
    if (r.open(scratch_, kIndexName, spec_.game) && r.header().kind == Kind::index) {
      have_index = true;
      for (const ChunkInfo& c : r.chunks()) {
        if (c.tag != kSlotTag) continue;
        Entry e;
        Reader in = r.reader(c);
        in(e);
        if (c.version > kSlotVersion || !in.ok() || !in.at_end()) {
          have_index = false;
          break;
        }
        std::string slot = e.slot;
        indexed.emplace(std::move(slot), std::move(e));
      }
    }
  }
  rep.rebuilt = !have_index;
  if (!have_index) indexed.clear();
  bool changed = !have_index;
  for (const DiskFile& f : files) {
    if (!valid_slot(spec_, f.name)) continue;
    const auto known = indexed.find(f.name);
    if (known != indexed.end() && known->second.bytes == f.bytes) {
      entries_.push_back(std::move(known->second));
      continue;
    }
    changed = true;
    rep.peeked++;
    Header h;
    Meta m;
    std::string why;
    size_t needed = 0;
    bool ok = disk_->read(f.name, scratch_, kPeekBytes) &&
              (peek(scratch_, h, m, why, &needed) ||
               (needed > scratch_.size() && disk_->read(f.name, scratch_, needed) &&
                peek(scratch_, h, m, why)));
    ok = ok && m.game == spec_.game && h.kind != Kind::index;
    if (!ok) {
      rep.damaged.push_back(f.name);
      continue;
    }
    entries_.push_back({f.name, std::move(m), f.bytes});
  }
  if (entries_.size() != indexed.size()) changed = true;  // a slot the index named is gone
  std::sort(entries_.begin(), entries_.end(),
            [](const Entry& a, const Entry& b) { return order(a.slot) < order(b.slot); });
  return changed ? save_index() : true;
}

bool Store::write(std::string_view slot_in, std::span<const uint8_t> file) {
  problem_.clear();
  const std::string slot = text(slot_in);
  if (!valid_slot(spec_, slot))
    return fail("\"" + slot + "\" is not a slot of this store",
                "name one of slot-1..slot-" + std::to_string(spec_.slots) +
                    ", quick or auto-1..auto-" + std::to_string(spec_.autosaves));
  Entry e;
  if (!check(slot, file, &e)) return false;
  const std::string tmp = slot + text(kTmp), bak = slot + text(kBak);
  const auto gone = [&](const std::string& what) {
    return fail("the save into " + slot + " stopped: " + what + ": " + disk_->problem(),
                "free space on the disk and save again; the last good save is untouched");
  };
  if (!disk_->write(tmp, file)) return gone("writing " + tmp);
  if (!disk_->read(tmp, scratch_) || scratch_.size() != file.size() ||
      !std::equal(scratch_.begin(), scratch_.end(), file.begin())) {
    disk_->remove(tmp);
    return fail("the save into " + slot + " did not read back as written",
                "the disk is failing: save into another slot, or onto another disk");
  }
  // The slot as it stood kept beside it until the new one is in place.
  bool had = find(slot) != nullptr;
  if (!had)
    for (const DiskFile& f : disk_->list()) had = had || f.name == slot;
  if (had && !disk_->rename(slot, bak)) return gone("setting the old save aside");
  if (!disk_->rename(tmp, slot)) return gone("putting the new save in place");
  if (!disk_->sync()) return gone("flushing the directory");
  if (had && !disk_->remove(bak)) return gone("removing the old save");
  put(std::move(e));
  return save_index();
}

bool Store::read(std::string_view slot_in, std::vector<uint8_t>& out) {
  problem_.clear();
  const std::string slot = text(slot_in);
  if (!disk_->read(slot, out))
    return fail("there is no save in " + slot + ": " + disk_->problem(),
                "list the store's slots and load one of them");
  return check(slot, out, nullptr);
}

bool Store::remove(std::string_view slot_in) {
  problem_.clear();
  const std::string slot = text(slot_in);
  if (find(slot) == nullptr)
    return fail("there is no save in " + slot + " to delete", "list the store's slots");
  if (!disk_->remove(slot))
    return fail("the save in " + slot + " cannot be deleted: " + disk_->problem(),
                "check the directory may be written");
  entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                [&](const Entry& e) { return e.slot == slot; }),
                 entries_.end());
  return save_index();
}

bool Store::copy(std::string_view from, std::string_view to) {
  std::vector<uint8_t> bytes;
  return read(from, bytes) && write(to, bytes);
}

bool Store::rename(std::string_view from, std::string_view to) {
  if (from == to) return true;
  return copy(from, to) && remove(from);
}

std::string Store::autosave_slot() const {
  std::string pick;
  int64_t oldest = 0;
  for (int i = 1; i <= spec_.autosaves; i++) {
    const std::string slot = "auto-" + std::to_string(i);
    const Entry* e = find(slot);
    if (e == nullptr) return slot;
    if (pick.empty() || e->meta.saved_unix < oldest) {
      pick = slot;
      oldest = e->meta.saved_unix;
    }
  }
  return pick;
}

bool Store::autosave_due(double now_s) const {
  return now_s - last_autosave_s_ >= spec_.autosave_spacing_s;
}

bool Store::autosave(std::span<const uint8_t> file, double now_s) {
  if (!autosave_due(now_s)) return true;  // within the spacing: one save for the burst
  if (!write(autosave_slot(), file)) return false;
  last_autosave_s_ = now_s;
  return true;
}

}  // namespace ge::save
