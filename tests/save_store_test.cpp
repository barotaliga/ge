// engine/save/store.hpp: slots written, listed, read, copied, renamed and deleted; a thousand
// slots listed in one read of the index; the index rebuilt when missing or damaged and a slot it
// does not name still found; the autosave ring and its spacing; a write killed at EVERY unit of the
// simulated disk leaves the last good save (or the new one) loadable; a real directory too; and
// the writer thread's posts answered in order.
//
// ge:summary Test: the save store's slots, index, ring, every killed write and the writer thread
// ge:keywords test save store slot index rebuild autosave ring kill crash bak tmp writer thread
// ge:skill save testing
#include "save/store.hpp"

#include <filesystem>
#include <string>
#include <vector>

#include "check.hpp"
#include "save/disk.hpp"
#include "save/store_writer.hpp"

namespace {

using namespace ge::save;

StoreSpec spec() {
  StoreSpec s;
  s.game = "demo";
  s.autosaves = 3;
  s.slots = 2000;
  s.autosave_spacing_s = 30;
  return s;
}

// A save of `n` bytes of state, titled `title`, saved at `when`.
std::vector<uint8_t> save_of(const std::string& title, int64_t when = 0, size_t n = 300) {
  Meta m;
  m.game = "demo";
  m.game_version = 1;
  m.title = title;
  m.saved_unix = when;
  std::vector<uint8_t> out;
  FileWriter w(out, m);
  std::vector<uint8_t> state(n);
  for (size_t i = 0; i < n; i++) state[i] = static_cast<uint8_t>(i * 7 + title.size());
  w.put(tag("STAT"), 0, 1, state);
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

}  // namespace

TEST(slots_are_written_listed_read_copied_renamed_and_deleted) {
  SimDisk disk;
  Store s(disk, spec());
  CHECK(s.open());
  CHECK(s.list().empty());
  CHECK(s.write("slot-2", save_of("two")));
  CHECK(s.write("quick", save_of("quick")));
  CHECK(s.write("slot-1", save_of("one")));
  CHECK(s.list().size() == 3);
  CHECK(s.list()[0].slot == "slot-1" && s.list()[1].slot == "slot-2" &&
        s.list()[2].slot == "quick");
  CHECK(s.find("slot-2")->meta.title == "two");
  CHECK(title_in(s, "slot-1") == "one");
  CHECK(s.write("slot-1", save_of("one again")));  // over a slot: the old one gone
  CHECK(title_in(s, "slot-1") == "one again");
  CHECK(disk.files.count("slot-1.bak") == 0 && disk.files.count("slot-1.tmp") == 0);
  CHECK(s.copy("slot-1", "slot-9"));
  CHECK(title_in(s, "slot-9") == "one again");
  CHECK(s.rename("slot-9", "slot-10"));
  CHECK(s.find("slot-9") == nullptr && title_in(s, "slot-10") == "one again");
  CHECK(s.remove("slot-2"));
  CHECK(s.find("slot-2") == nullptr && disk.files.count("slot-2") == 0);
  // Refused: a name no slot has, a file that is not a save, another game's save.
  CHECK(!s.write("slot-0", save_of("x")));
  CHECK(!s.write("../x", save_of("x")));
  CHECK(!s.write("slot-1", std::vector<uint8_t>(100, 7)));
  CHECK(s.problem().find("does not open") != std::string::npos);
  StoreSpec other = spec();
  other.game = "other";
  SimDisk d2;
  Store s2(d2, other);
  CHECK(s2.open());
  CHECK(!s2.write("slot-1", save_of("x")));
  CHECK(title_in(s, "slot-1") == "one again");  // nothing above touched it
}

TEST(a_thousand_slots_list_in_one_read_and_the_index_rebuilds) {
  SimDisk disk;
  {
    Store s(disk, spec());
    CHECK(s.open());
    for (int i = 1; i <= 1000; i++)
      CHECK(s.write("slot-" + std::to_string(i), save_of("s" + std::to_string(i), i, 64)));
  }
  SimDisk again = disk.restarted();
  Store s(again, spec());
  StoreOpen rep;
  CHECK(s.open(&rep));
  CHECK(s.list().size() == 1000);
  CHECK(again.reads == 1);  // the index alone
  CHECK(!rep.rebuilt && rep.peeked == 0);
  CHECK(s.find("slot-777")->meta.title == "s777");
  // The index gone: rebuilt from the slots' headers, a peek each.
  SimDisk lost = disk.restarted();
  lost.files.erase("index");
  Store s3(lost, spec());
  CHECK(s3.open(&rep));
  CHECK(rep.rebuilt && rep.peeked == 1000 && s3.list().size() == 1000);
  CHECK(s3.find("slot-5")->meta.title == "s5");
  // Damaged: rebuilt too.
  SimDisk bad = disk.restarted();
  bad.files["index"][100] ^= 1;
  Store s4(bad, spec());
  CHECK(s4.open(&rep));
  CHECK(rep.rebuilt && s4.list().size() == 1000);
  // A slot the index does not name (copied in by hand) is found; one it names but gone is dropped.
  SimDisk more = disk.restarted();
  more.files["slot-1500"] = save_of("by hand");
  more.files.erase("slot-3");
  Store s5(more, spec());
  CHECK(s5.open(&rep));
  CHECK(!rep.rebuilt && rep.peeked == 1);
  CHECK(s5.find("slot-1500") != nullptr && s5.find("slot-3") == nullptr);
  CHECK(s5.list().size() == 1000);
}

TEST(the_autosave_ring_takes_the_empty_then_the_oldest_and_is_spaced) {
  SimDisk disk;
  Store s(disk, spec());
  CHECK(s.open());
  CHECK(s.autosave_slot() == "auto-1");
  CHECK(s.autosave(save_of("a", 100), 0));
  CHECK(s.autosave(save_of("b", 110), 10));  // within 30 s of the last: not written
  CHECK(s.find("auto-2") == nullptr);
  CHECK(s.autosave(save_of("b", 140), 40));
  CHECK(s.autosave(save_of("c", 180), 80));
  CHECK(s.autosave_slot() == "auto-1");  // the oldest
  CHECK(s.autosave(save_of("d", 220), 120));
  CHECK(title_in(s, "auto-1") == "d");
  CHECK(s.autosave_slot() == "auto-2");
}

TEST(a_write_killed_at_every_unit_leaves_a_good_save) {
  SimDisk base;
  {
    Store s(base, spec());
    CHECK(s.open());
    CHECK(s.write("slot-1", save_of("old", 1, 200)));
  }
  const std::vector<uint8_t> fresh = save_of("new", 2, 260);
  // How many units the whole write spends.
  SimDisk count = base.restarted();
  {
    Store s(count, spec());
    CHECK(s.open());
    const int64_t before = count.spent;
    CHECK(s.write("slot-1", fresh));
    count.budget = count.spent - before;  // reused below as the write's cost
  }
  const int64_t cost = count.budget;
  CHECK(cost > 500);
  int64_t old_kept = 0, new_kept = 0;
  for (int64_t k = 0; k <= cost; k++) {
    SimDisk d = base.restarted();
    Store s(d, spec());
    CHECK(s.open());
    d.budget = d.spent + k;
    s.write("slot-1", fresh);
    SimDisk back = d.restarted();
    Store t(back, spec());
    CHECK(t.open());
    const std::string title = title_in(t, "slot-1");
    CHECK(title == "old" || title == "new");
    if (title == "old") old_kept++;
    if (title == "new") new_kept++;
    CHECK(t.list().size() == 1 && t.find("slot-1")->meta.title == title);
    for (const auto& [name, bytes] : back.files) CHECK(name.find(".tmp") == std::string::npos);
  }
  CHECK(old_kept > 0 && new_kept > 0);
}

TEST(a_real_directory_keeps_its_saves) {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "ge_save_store_test";
  std::filesystem::remove_all(dir);
  {
    DirDisk disk(dir);
    Store s(disk, spec());
    CHECK(s.open());
    CHECK(s.write("slot-3", save_of("three")));
    CHECK(s.write("slot-3", save_of("three again")));
    CHECK(s.write("quick", save_of("quick")));
  }
  {
    DirDisk disk(dir);
    Store s(disk, spec());
    StoreOpen rep;
    CHECK(s.open(&rep));
    CHECK(!rep.rebuilt && s.list().size() == 2);
    CHECK(title_in(s, "slot-3") == "three again");
    CHECK(s.remove("quick"));
  }
  CHECK(!std::filesystem::exists(dir / "quick"));
  CHECK(!std::filesystem::exists(dir / "slot-3.bak"));
  std::filesystem::remove_all(dir);
}

TEST(the_writer_thread_answers_each_post_in_order) {
  SimDisk disk;
  StoreWriter w(disk, spec());
  std::string problem;
  CHECK(w.open(nullptr, problem));
  const uint64_t a = w.post("slot-1", save_of("one"));
  const uint64_t b = w.post("slot-0", save_of("bad slot"));
  const uint64_t c = w.post_autosave(save_of("auto", 5), 0);
  const uint64_t d = w.post_autosave(save_of("auto too soon", 6), 1);
  w.flush();
  const std::vector<StoreDone> done = w.poll();
  CHECK(done.size() == 4);
  CHECK(done[0].id == a && done[0].ok);
  CHECK(done[1].id == b && !done[1].ok && !done[1].problem.empty());
  CHECK(done[2].id == c && done[2].ok && !done[2].skipped && done[2].slot == "auto-1");
  CHECK(done[3].id == d && done[3].ok && done[3].skipped);
  CHECK(w.with([](Store& s) { return s.list().size(); }) == 2);
  CHECK(w.poll().empty());
}
