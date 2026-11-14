// engine/save/cloud.hpp and services/save_cloud.hpp: two devices syncing one cloud converge on
// their slots; a save made on one comes down on the other; both changed keeps the one played
// longer on both and the other as .conflict on both; a removal follows; a sync with nothing
// changed reads no local slot and writes nothing to the cloud; an offline cloud leaves the saves
// alone and the next sync finishes; a damaged cloud file is never taken as a removal; the
// simulated store's cloud through SaveCloud does the same.
//
// ge:summary Test: two devices synced through one cloud: up, down, conflict, removal, offline
// ge:keywords test cloud sync conflict converge devices manifest base save-cloud sim-store
// ge:skill save testing
#include "save/cloud.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "check.hpp"
#include "save/disk.hpp"
#include "save/store.hpp"
#include "services/catalog.hpp"
#include "services/save_cloud.hpp"
#include "services/sim_store.hpp"

namespace {

using namespace ge::save;

StoreSpec spec() {
  StoreSpec s;
  s.game = "demo";
  return s;
}

std::vector<uint8_t> save_of(const std::string& title, double play_s, int64_t when) {
  Meta m;
  m.game = "demo";
  m.game_version = 1;
  m.title = title;
  m.play_s = play_s;
  m.saved_unix = when;
  std::vector<uint8_t> out;
  FileWriter w(out, m);
  w.put(tag("STAT"), 0, 1, std::vector<uint8_t>(500, static_cast<uint8_t>(title.size())));
  CHECK(w.finish());
  return out;
}

std::string title_in(Store& s, const std::string& slot) {
  std::vector<uint8_t> bytes;
  if (!s.read(slot, bytes)) return "";
  FileReader r;
  CHECK(r.open(bytes, slot, "demo"));
  return r.meta().title;
}

std::string titles(Store& s) {
  std::string out;
  for (const Entry& e : s.list()) out += e.slot + "=" + e.meta.title + " ";
  return out;
}

// One device: its disk and its store, synced with `cloud`.
struct Device {
  SimDisk disk;
  Store store{disk, spec()};
  Device() { CHECK(store.open()); }
  SyncReport sync(Cloud& cloud) {
    SyncReport rep;
    std::string problem;
    CHECK_MSG(ge::save::sync(store, disk, cloud, "saves/", &rep, problem), problem);
    return rep;
  }
};

}  // namespace

TEST(two_devices_converge_and_keep_a_conflict) {
  MemCloud cloud;
  Device a, b;
  CHECK(a.store.write("slot-1", save_of("first", 100, 1000)));
  CHECK(a.store.write("slot-2", save_of("second", 150, 1100)));
  SyncReport r = a.sync(cloud);
  CHECK(r.uploaded == 2 && r.downloaded == 0);
  r = b.sync(cloud);
  CHECK(r.downloaded == 2 && r.uploaded == 0);
  CHECK(titles(a.store) == titles(b.store));

  // Nothing changed: no local slot read, nothing written to the cloud.
  r = a.sync(cloud);
  CHECK(r.local_reads == 0 && r.cloud_bytes_written == 0 && r.uploaded == 0);

  // Each device moves its own slot: both arrive on the other.
  CHECK(a.store.write("slot-3", save_of("a's new", 50, 2000)));
  CHECK(b.store.write("quick", save_of("b's quick", 60, 2001)));
  a.sync(cloud);
  b.sync(cloud);
  a.sync(cloud);
  CHECK(titles(a.store) == titles(b.store));
  CHECK(title_in(a.store, "quick") == "b's quick" && title_in(b.store, "slot-3") == "a's new");

  // Both change slot-1: b's played longer, so b's wins on both and a's is kept as .conflict.
  CHECK(a.store.write("slot-1", save_of("a's slot-1", 200, 3000)));
  CHECK(b.store.write("slot-1", save_of("b's slot-1", 300, 2500)));
  a.sync(cloud);
  r = b.sync(cloud);
  CHECK(r.conflicts.size() == 1 && r.conflicts[0] == "slot-1");
  r = a.sync(cloud);
  CHECK(r.downloaded == 1 && r.conflicts_fetched == 1);  // b's comes down, a's own kept
  CHECK(title_in(a.store, "slot-1") == "b's slot-1" && title_in(b.store, "slot-1") == "b's slot-1");
  CHECK(a.disk.files.count("slot-1.conflict") == 1 && b.disk.files.count("slot-1.conflict") == 1);
  FileReader kept;
  CHECK(kept.open(a.disk.files["slot-1.conflict"], "conflict", "demo") &&
        kept.meta().title == "a's slot-1");
  CHECK(titles(a.store) == titles(b.store));

  // A removal on one device follows to the other.
  CHECK(b.store.remove("slot-2"));
  b.sync(cloud);
  r = a.sync(cloud);
  CHECK(r.removed_here == 1 && a.store.find("slot-2") == nullptr);
  CHECK(titles(a.store) == titles(b.store));

  // The store, reopened, lists the slots and never the sync record or the kept conflict.
  SimDisk again = a.disk.restarted();
  Store s(again, spec());
  CHECK(s.open());
  CHECK(titles(s) == titles(a.store));
}

TEST(the_winner_is_the_same_whichever_device_syncs_first) {
  MemCloud cloud;
  Device a, b;
  CHECK(a.store.write("slot-1", save_of("base", 10, 100)));
  a.sync(cloud);
  b.sync(cloud);
  CHECK(a.store.write("slot-1", save_of("a longer", 90, 200)));
  CHECK(b.store.write("slot-1", save_of("b shorter", 40, 300)));
  b.sync(cloud);  // b first this time: its save is up, then a's longer one beats it
  a.sync(cloud);
  b.sync(cloud);
  CHECK(title_in(a.store, "slot-1") == "a longer" && title_in(b.store, "slot-1") == "a longer");
}

TEST(an_offline_cloud_leaves_the_saves_and_the_next_sync_finishes) {
  MemCloud cloud;
  Device a;
  CHECK(a.store.write("slot-1", save_of("one", 10, 100)));
  cloud.offline = true;
  SyncReport rep;
  std::string problem;
  CHECK(!sync(a.store, a.disk, cloud, "saves/", &rep, problem));
  CHECK(problem.find("next:") != std::string::npos);
  CHECK(title_in(a.store, "slot-1") == "one");
  cloud.offline = false;
  CHECK(a.sync(cloud).uploaded == 1);

  // A cloud slot that is not a save of this game is left alone, never taken as removed.
  Device b;
  b.sync(cloud);
  for (auto& [name, bytes] : cloud.files)
    if (name == "saves/slot-1") bytes[0] ^= 0xff;
  cloud.files.erase(std::remove_if(cloud.files.begin(), cloud.files.end(),
                                   [](const auto& f) { return f.first == "saves/manifest"; }),
                    cloud.files.end());
  b.sync(cloud);
  CHECK(title_in(b.store, "slot-1") == "one");
}

TEST(the_simulated_store_s_cloud_carries_saves_between_devices) {
  const ge::services::Catalog catalog{};
  ge::services::SimStore store(catalog);
  const ge::services::UserId player = store.add_user("player");
  auto on_a = store.backend(player), on_b = store.backend(player);
  ge::services::SaveCloud cloud_a(*on_a), cloud_b(*on_b);
  Device a, b;
  CHECK(a.store.write("slot-7", save_of("from a", 30, 100)));
  CHECK(a.sync(cloud_a).uploaded == 1);
  CHECK(b.sync(cloud_b).downloaded == 1);
  CHECK(title_in(b.store, "slot-7") == "from a");
  std::vector<uint8_t> none;
  CHECK(!cloud_b.read("saves/slot-9", none) &&
        cloud_b.problem().find("not_found") != std::string::npos);
}
