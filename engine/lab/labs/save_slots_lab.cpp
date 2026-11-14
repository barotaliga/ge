// LAB `save-slots`: the save store (engine/save/store.hpp) with many slots and a write killed at
// every point. A store on the simulated disk (disk.hpp's `SimDisk`) is filled with `--slots`
// saves; then it is opened again as a menu would open it, with the index and without it: how many
// files are read, how many bytes, and how long the open takes. Then one save is written over a
// slot and the disk is killed at EVERY unit of that write (each byte, each rename, remove and
// flush), the files left are opened by a new store, and the slot is read: does it hold the old
// save or the new one, whole, every time? Last, the same store is timed on a real directory (the
// system's temporary one, removed after). The counts are deterministic; the times are measured.
//
// ge:summary Lab: a thousand save slots listed from the index, and a write killed at every point
// ge:keywords lab save-slots save store slot index rebuild list kill crash power-loss last-good
// ge:keywords autosave sim-disk dir-disk fsync
// ge:skill save labs
// ge:test tests/save_store_test.cpp
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "save/disk.hpp"
#include "save/file.hpp"
#include "save/store.hpp"

namespace {

namespace gl = ge::lab;
namespace sv = ge::save;

sv::StoreSpec spec(int slots) {
  sv::StoreSpec s;
  s.game = "lab";
  s.slots = std::max(slots, 1);
  return s;
}

std::vector<uint8_t> save_of(const std::string& title, int64_t when, size_t bytes) {
  sv::Meta m;
  m.game = "lab";
  m.game_version = 1;
  m.title = title;
  m.place = "the lab's bench";
  m.play_s = static_cast<double>(when);
  m.saved_unix = when;
  std::vector<uint8_t> out;
  sv::FileWriter w(out, m);
  std::vector<uint8_t> state(bytes);
  for (size_t i = 0; i < bytes; i++) state[i] = static_cast<uint8_t>((i * 31 + when) & 0xff);
  w.put(sv::tag("STAT"), 0, 1, state);
  w.finish();
  return out;
}

double ms_since(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

std::string title_in(sv::Store& s, const std::string& slot) {
  std::vector<uint8_t> bytes;
  sv::FileReader r;
  if (!s.read(slot, bytes) || !r.open(bytes, slot, "lab")) return "";
  return r.meta().title;
}

void run(gl::Lab& lab) {
  const int slots = std::max(1, static_cast<int>(lab.arg_num("slots")));
  const size_t bytes = static_cast<size_t>(std::max(64.0, lab.arg_num("bytes")));
  const size_t kill_bytes = static_cast<size_t>(std::max(64.0, lab.arg_num("kill-bytes")));

  // Filled.
  sv::SimDisk filled;
  {
    sv::Store s(filled, spec(slots));
    if (!s.open()) {
      lab.fail("save-slots: " + s.problem(), "next: read engine/save/store.cpp; skill: save");
      return;
    }
    for (int i = 1; i <= slots; i++)
      if (!s.write("slot-" + std::to_string(i), save_of("save " + std::to_string(i), i, bytes))) {
        lab.fail("save-slots: " + s.problem(), "next: read engine/save/store.cpp; skill: save");
        return;
      }
  }
  // Opened as a menu opens it: with the index, then with it gone.
  const auto open = [&](bool keep_index, sv::StoreOpen& rep, sv::SimDisk& d, double& ms) {
    d = filled.restarted();
    if (!keep_index) d.files.erase(sv::kIndexName);
    sv::Store s(d, spec(slots));
    const auto t0 = std::chrono::steady_clock::now();
    const bool ok = s.open(&rep);
    ms = ms_since(t0);
    return ok ? static_cast<int64_t>(s.list().size()) : -1;
  };
  sv::StoreOpen with, without;
  sv::SimDisk dw, dn;
  double ms_with = 0, ms_without = 0;
  const int64_t listed = open(true, with, dw, ms_with);
  const int64_t listed_rebuilt = open(false, without, dn, ms_without);
  lab.metric("slots.listed", static_cast<double>(listed), "count");
  lab.metric("open.reads", static_cast<double>(dw.reads), "count");
  lab.metric("open.bytes-read", static_cast<double>(dw.bytes_read), "bytes");
  lab.metric("rebuild.listed", static_cast<double>(listed_rebuilt), "count");
  lab.metric("rebuild.reads", static_cast<double>(dn.reads), "count");
  lab.metric("rebuild.bytes-read", static_cast<double>(dn.bytes_read), "bytes");
  lab.metric("index.bytes", static_cast<double>(filled.files[sv::kIndexName].size()), "bytes");
  lab.metric("open.ms", ms_with, "ms", gl::Det::measured);
  lab.metric("rebuild.ms", ms_without, "ms", gl::Det::measured);
  if (listed != slots || dw.reads != 1 || with.rebuilt)
    lab.fail("save-slots: the open listed " + std::to_string(listed) + " of " +
                 std::to_string(slots) + " slots in " + std::to_string(dw.reads) + " reads",
             "next: read Store::open in engine/save/store.cpp; skill: save");
  if (listed_rebuilt != slots)
    lab.fail("save-slots: the rebuilt index lists " + std::to_string(listed_rebuilt) + " of " +
                 std::to_string(slots),
             "next: read Store::open in engine/save/store.cpp; skill: save");

  // A write killed at every unit.
  sv::SimDisk base;
  {
    sv::Store s(base, spec(4));
    s.open();
    s.write("slot-1", save_of("the old save", 1, kill_bytes));
    s.write("slot-2", save_of("a neighbour", 2, kill_bytes));
  }
  const std::vector<uint8_t> fresh = save_of("the new save", 3, kill_bytes + 40);
  int64_t cost = 0;
  {
    sv::SimDisk d = base.restarted();
    sv::Store s(d, spec(4));
    s.open();
    const int64_t before = d.spent;
    s.write("slot-1", fresh);
    cost = d.spent - before;
  }
  int64_t old_kept = 0, new_kept = 0, lost = 0, neighbour_lost = 0, first_new = -1;
  gl::Trace kept{"the save the slot holds (0 the old, 1 the new)", {}, {}};
  for (int64_t k = 0; k <= cost; k++) {
    sv::SimDisk d = base.restarted();
    sv::Store s(d, spec(4));
    s.open();
    d.budget = d.spent + k;
    s.write("slot-1", fresh);
    sv::SimDisk back = d.restarted();
    sv::Store t(back, spec(4));
    t.open();
    const std::string title = title_in(t, "slot-1");
    if (title == "the old save")
      old_kept++;
    else if (title == "the new save") {
      new_kept++;
      if (first_new < 0) first_new = k;
    } else
      lost++;
    if (title_in(t, "slot-2") != "a neighbour") neighbour_lost++;
    kept.x.push_back(static_cast<double>(k));
    kept.y.push_back(title == "the new save" ? 1.0 : title == "the old save" ? 0.0 : -1.0);
  }
  lab.metric("kill.points", static_cast<double>(cost + 1), "count");
  lab.metric("kill.old-kept", static_cast<double>(old_kept), "count");
  lab.metric("kill.new-kept", static_cast<double>(new_kept), "count");
  lab.metric("kill.lost", static_cast<double>(lost + neighbour_lost), "count");
  lab.metric("kill.first-new", static_cast<double>(first_new), "unit");
  if (lost + neighbour_lost > 0)
    lab.fail("save-slots: " + std::to_string(lost) + " killed writes lost the slot and " +
                 std::to_string(neighbour_lost) + " its neighbour",
             "next: read Store::write and Store::open's recovery in engine/save/store.cpp; "
             "skill: save");

  // On a real directory.
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "ge_save_slots_lab";
  std::filesystem::remove_all(dir);
  const int real = std::min(slots, static_cast<int>(lab.arg_num("real-slots")));
  double write_ms = 0, real_open_ms = 0;
  int64_t real_listed = 0;
  {
    sv::DirDisk disk(dir);
    sv::Store s(disk, spec(slots));
    s.open();
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 1; i <= real; i++)
      s.write("slot-" + std::to_string(i), save_of("save " + std::to_string(i), i, bytes));
    write_ms = ms_since(t0) / std::max(real, 1);
  }
  {
    sv::DirDisk disk(dir);
    sv::Store s(disk, spec(slots));
    const auto t0 = std::chrono::steady_clock::now();
    s.open();
    real_open_ms = ms_since(t0);
    real_listed = static_cast<int64_t>(s.list().size());
  }
  std::filesystem::remove_all(dir);
  lab.metric("dir.listed", static_cast<double>(real_listed), "count", gl::Det::measured);
  lab.metric("dir.write-ms", write_ms, "ms", gl::Det::measured);
  lab.metric("dir.open-ms", real_open_ms, "ms", gl::Det::measured);

  gl::LabTable t{"opening a store of " + std::to_string(slots) + " slots",
                 {"opened", "listed", "files read", "bytes read"},
                 {}};
  t.rows.push_back({"with its index", std::to_string(listed), std::to_string(dw.reads),
                    std::to_string(dw.bytes_read)});
  t.rows.push_back({"index gone: rebuilt", std::to_string(listed_rebuilt), std::to_string(dn.reads),
                    std::to_string(dn.bytes_read)});
  lab.table(t);
  gl::LabTable k{"one write killed at every unit of " + std::to_string(cost),
                 {"kept the old", "kept the new", "lost", "new from unit"},
                 {}};
  k.rows.push_back({std::to_string(old_kept), std::to_string(new_kept),
                    std::to_string(lost + neighbour_lost), std::to_string(first_new)});
  lab.table(k);

  gl::LineChart c;
  c.spikes = false;
  c.points = true;
  c.frame = {"WHAT A KILLED WRITE LEAVES",
             "a " + std::to_string(fresh.size()) +
                 "-byte save written over a slot, the disk "
                 "killed at each unit (a byte, a rename, a remove, a flush)",
             "unit the disk died at",
             "the slot holds (0 old, 1 new, -1 nothing)",
             960,
             360};
  c.traces = {kept};
  const gl::Chart drawn = gl::line_chart(c);
  lab.shot({"killed",
            drawn.title,
            "Each dot is one write killed at that unit and the store opened again: the slot "
            "holds the old save until the new one has been renamed into place, and the new one "
            "from then on; never nothing, never a mix.",
            "dots at 0, then at 1 from one unit on; none at -1",
            {{"slots", std::to_string(slots)}}},
           gl::to_picture(drawn.plot));
}

const gl::LabSpec kSpec{
    "save-slots",
    gl::LabKind::sim,
    "engine",
    "engine/save/store.hpp, engine/save/disk.hpp",
    "save",
    "Are a thousand save slots listed with one read, and does a write killed at every point "
    "leave the last good save loadable?",
    {
        {"slots", "1000", "slots the store is filled with"},
        {"bytes", "2000", "each save's state, bytes"},
        {"kill-bytes", "600", "the killed write's state, bytes (a unit a byte)"},
        {"real-slots", "50", "slots timed on a real directory"},
    },
    "--slots=200 --real-slots=10",
    run,
};
GE_LAB(kSpec);

}  // namespace
