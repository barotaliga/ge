// LAB `save-cloud`: saves on every device (engine/save/cloud.hpp, docs/saves.md § Saves on every
// device). Two devices of one player, each a save store on its own simulated disk, sync through
// the simulated store's cloud (`services::SaveCloud` over `SimStore`): a story step by step (the
// first upload, the second device's download, a save on each, both changing one slot, a slot
// removed, a sync with nothing to do), drawn as the three sides' slots after each step. Then
// `--rounds` rounds of three devices saving, overwriting and removing at random off `--seed` and
// syncing in a random order, each round ending with every device synced twice: do they converge
// on the same slots every round, and what do the syncs cost in cloud bytes?
//
//   make lab LAB=save-cloud
//   make lab LAB=save-cloud ARGS="--rounds=500 --seed=7"
//
// ge:summary Lab: two devices sync saves through the simulated store's cloud; a conflict kept
// ge:keywords lab save-cloud cloud sync conflict converge devices manifest base sim-store
// ge:skill save labs
// ge:test tests/save_cloud_test.cpp
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/prng.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "save/cloud.hpp"
#include "save/disk.hpp"
#include "save/file.hpp"
#include "save/store.hpp"
#include "services/catalog.hpp"
#include "services/save_cloud.hpp"
#include "services/sim_store.hpp"

namespace {

namespace gl = ge::lab;
namespace sv = ge::save;
namespace gs = ge::services;

constexpr const char* kPrefix = "saves/";

sv::StoreSpec spec() {
  sv::StoreSpec s;
  s.game = "lab";
  s.slots = 9;
  return s;
}

std::vector<uint8_t> save_of(const std::string& title, double play_s, int64_t when) {
  sv::Meta m;
  m.game = "lab";
  m.game_version = 1;
  m.title = title;
  m.play_s = play_s;
  m.saved_unix = when;
  std::vector<uint8_t> out;
  sv::FileWriter w(out, m);
  std::vector<uint8_t> state(1500);
  for (size_t i = 0; i < state.size(); i++)
    state[i] = static_cast<uint8_t>((i * 13 + title.size() * 7 + static_cast<size_t>(when)) & 0xff);
  w.put(sv::tag("STAT"), 0, 1, state);
  w.finish();
  return out;
}

std::string title_of(std::span<const uint8_t> bytes) {
  sv::FileReader r;
  return r.open(bytes, "save", "lab") ? r.meta().title : "?";
}

struct Device {
  std::string name;
  sv::SimDisk disk;
  sv::Store store{disk, spec()};
  std::unique_ptr<gs::Backend> backend;
  std::unique_ptr<gs::SaveCloud> cloud;
};

// One side's slots as lines: "slot-1  first run".
std::vector<std::string> lines_of(Device& d) {
  std::vector<std::string> out;
  for (const sv::Entry& e : d.store.list()) out.push_back(e.slot + "  " + e.meta.title);
  for (const auto& [name, bytes] : d.disk.files)
    if (name.size() > 9 && name.substr(name.size() - 9) == ".conflict")
      out.push_back(name + "  " + title_of(bytes));
  return out;
}

std::vector<std::string> cloud_lines(gs::SaveCloud& cloud) {
  std::vector<sv::CloudFileInfo> files;
  cloud.list(files);
  std::vector<std::string> out;
  std::vector<uint8_t> bytes;
  for (const sv::CloudFileInfo& f : files) {
    const std::string name = f.name.substr(std::string(kPrefix).size());
    if (name == sv::kManifestName) continue;
    cloud.read(f.name, bytes);
    out.push_back(name + "  " + title_of(bytes));
  }
  return out;
}

struct Step {
  std::string what;
  std::vector<std::string> a, cloud, b;
  std::string report;
};

std::string report_of(const sv::SyncReport& r) {
  std::string s = std::to_string(r.uploaded) + " up, " + std::to_string(r.downloaded) + " down";
  if (r.removed_here + r.removed_cloud > 0)
    s += ", " + std::to_string(r.removed_here + r.removed_cloud) + " removed";
  if (!r.conflicts.empty()) s += ", conflict on " + r.conflicts[0];
  if (r.conflicts_fetched > 0) s += ", conflict copy fetched";
  s += ", " + std::to_string(r.cloud_bytes_written) + " B to the cloud";
  return s;
}

gl::Picture draw(const std::vector<Step>& steps) {
  constexpr int kCol = 300, kLine = 11, kPad = 10;
  int height = kPad + 18;
  for (const Step& s : steps)
    height +=
        30 +
        kLine * static_cast<int>(std::max({s.a.size(), s.cloud.size(), s.b.size(), size_t{1}})) +
        kPad;
  gl::Picture p = gl::create_picture(kPad * 2 + kCol * 3, height, {24, 27, 32});
  const gl::Rgba head{240, 200, 90}, ink{210, 214, 220}, dim{120, 128, 140}, cloud{120, 190, 255},
      conflict{255, 120, 110};
  const char* names[3] = {"DEVICE A", "THE CLOUD", "DEVICE B"};
  for (int c = 0; c < 3; c++) gl::draw_text(p, kPad + c * kCol, kPad, names[c], head, 2);
  int y = kPad + 24;
  for (const Step& s : steps) {
    gl::fill_rect(p, kPad, y, kCol * 3 - 8, 1, {60, 66, 76});
    gl::draw_text(p, kPad, y + 5, s.what, head);
    gl::draw_text(p, kPad + 8, y + 16, s.report, dim);
    const std::vector<std::string>* cols[3] = {&s.a, &s.cloud, &s.b};
    size_t most = 1;
    for (int c = 0; c < 3; c++) {
      most = std::max(most, cols[c]->size());
      for (size_t i = 0; i < cols[c]->size(); i++) {
        const std::string& line = (*cols[c])[i];
        const bool kept = line.find(".conflict") != std::string::npos;
        gl::draw_text(p, kPad + c * kCol + 8, y + 30 + static_cast<int>(i) * kLine, line,
                      kept     ? conflict
                      : c == 1 ? cloud
                               : ink);
      }
    }
    y += 30 + kLine * static_cast<int>(most) + kPad;
  }
  return p;
}

bool same_slots(Device& x, Device& y) {
  if (x.store.list().size() != y.store.list().size()) return false;
  std::vector<uint8_t> bx, by;
  for (const sv::Entry& e : x.store.list()) {
    if (!x.store.read(e.slot, bx) || !y.store.read(e.slot, by) || bx != by) return false;
  }
  return true;
}

void run(gl::Lab& lab) {
  const int rounds = std::max(1, static_cast<int>(lab.arg_num("rounds")));
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));

  const gs::Catalog catalog{};
  gs::SimStore store(catalog);
  const gs::UserId player = store.add_user("player");
  const auto device = [&](const std::string& name) {
    auto d = std::make_unique<Device>();
    d->name = name;
    d->store.open();
    d->backend = store.backend(player);
    d->cloud = std::make_unique<gs::SaveCloud>(*d->backend);
    return d;
  };
  bool failed = false;
  const auto sync = [&](Device& d, sv::SyncReport& rep) {
    std::string problem;
    if (!sv::sync(d.store, d.disk, *d.cloud, kPrefix, &rep, problem) && !failed) {
      failed = true;
      lab.fail("save-cloud: " + d.name + "'s sync failed: " + problem,
               "next: read engine/save/cloud.cpp; skill: save");
    }
  };

  // The story.
  std::unique_ptr<Device> a = device("device A"), b = device("device B");
  std::vector<Step> steps;
  sv::SyncReport rep;
  const auto shot = [&](const std::string& what) {
    steps.push_back({what, lines_of(*a), cloud_lines(*a->cloud), lines_of(*b), report_of(rep)});
  };
  a->store.write("slot-1", save_of("the north face", 600, 1000));
  a->store.write("slot-2", save_of("the long valley", 900, 1100));
  sync(*a, rep);
  shot("1. A saves two slots and syncs: both go up");
  sync(*b, rep);
  shot("2. B syncs for the first time: both come down");
  a->store.write("slot-3", save_of("the night run", 300, 2000));
  b->store.write("quick", save_of("B's quicksave", 1000, 2100));
  sync(*a, rep);
  sync(*b, rep);
  shot("3. A saves slot-3, B quicksaves; A syncs, then B");
  a->store.write("slot-1", save_of("A: the north face, further", 700, 3000));
  b->store.write("slot-1", save_of("B: the north face, much further", 1500, 2900));
  sync(*a, rep);
  sync(*b, rep);
  shot("4. both play slot-1 apart; A syncs, then B: B played longer, B's wins, A's kept");
  b->store.remove("slot-2");
  sync(*b, rep);
  sync(*a, rep);
  shot("5. B removes slot-2; A syncs: everything follows, A's conflict copy too");
  sync(*a, rep);
  const uint64_t idle_bytes = rep.cloud_bytes_written;
  const int idle_reads = rep.local_reads;
  shot("6. A syncs again with nothing changed");
  const bool story_converged = same_slots(*a, *b);
  std::vector<uint8_t> won;
  a->store.read("slot-1", won);
  const bool story_winner = title_of(won) == "B: the north face, much further" &&
                            a->disk.files.count("slot-1.conflict") == 1 &&
                            b->disk.files.count("slot-1.conflict") == 1;
  lab.metric("story.converged", story_converged ? 1 : 0, "bool");
  lab.metric("story.winner-kept", story_winner ? 1 : 0, "bool");
  lab.metric("idle.cloud-bytes", static_cast<double>(idle_bytes), "bytes");
  lab.metric("idle.local-reads", idle_reads, "count");
  if (!story_converged || !story_winner || idle_bytes != 0 || idle_reads != 0)
    lab.fail("save-cloud: the story ends with the devices " +
                 std::string(story_converged ? "in step" : "apart") + ", the winner " +
                 (story_winner ? "kept" : "lost") + ", an idle sync writing " +
                 std::to_string(idle_bytes) + " B",
             "next: run make test T=save_cloud; read engine/save/cloud.cpp; skill: save");

  // Three devices at random.
  gs::SimStore rstore(catalog);
  const gs::UserId rplayer = rstore.add_user("player");
  std::vector<std::unique_ptr<Device>> devs;
  for (const char* n : {"device 1", "device 2", "device 3"}) {
    auto d = std::make_unique<Device>();
    d->name = n;
    d->store.open();
    d->backend = rstore.backend(rplayer);
    d->cloud = std::make_unique<gs::SaveCloud>(*d->backend);
    devs.push_back(std::move(d));
  }
  ge::core::Rng rng = ge::core::make_rng(seed);
  int converged = 0, conflicts = 0, uploads = 0, downloads = 0, removals = 0;
  uint64_t bytes_up = 0, bytes_down = 0;
  int64_t clock = 0;
  for (int round = 0; round < rounds; round++) {
    for (auto& d : devs) {
      const int edits = ge::core::int_in(rng, 0, 2);
      for (int e = 0; e < edits; e++) {
        const std::string slot = "slot-" + std::to_string(ge::core::int_in(rng, 1, 5));
        clock += ge::core::int_in(rng, 1, 60);
        if (ge::core::chance(rng, 0.15)) {
          if (d->store.find(slot) != nullptr) d->store.remove(slot);
        } else {
          const double play = ge::core::int_in(rng, 0, 5000);
          d->store.write(slot, save_of(d->name + " round " + std::to_string(round), play, clock));
        }
      }
    }
    std::vector<size_t> order{0, 1, 2};
    for (size_t i = order.size() - 1; i > 0; i--)
      std::swap(order[i], order[ge::core::pick_index(rng, i + 1)]);
    for (int pass = 0; pass < 2; pass++)
      for (const size_t i : order) {
        sync(*devs[i], rep);
        conflicts += static_cast<int>(rep.conflicts.size());
        uploads += rep.uploaded;
        downloads += rep.downloaded;
        removals += rep.removed_here + rep.removed_cloud;
        bytes_up += rep.cloud_bytes_written;
        bytes_down += rep.cloud_bytes_read;
      }
    if (same_slots(*devs[0], *devs[1]) && same_slots(*devs[1], *devs[2])) converged++;
  }
  lab.metric("random.rounds", rounds, "count");
  lab.metric("random.converged", converged, "count");
  lab.metric("random.conflicts", conflicts, "count");
  lab.metric("random.uploads", uploads, "count");
  lab.metric("random.downloads", downloads, "count");
  lab.metric("random.removals", removals, "count");
  lab.metric("random.cloud-bytes-written", static_cast<double>(bytes_up), "bytes");
  lab.metric("random.cloud-bytes-read", static_cast<double>(bytes_down), "bytes");
  if (converged != rounds)
    lab.fail("save-cloud: three devices converged in " + std::to_string(converged) + " of " +
                 std::to_string(rounds) + " rounds",
             "next: run make test T=save_cloud; read sync in engine/save/cloud.cpp; skill: save");

  gl::LabTable t{
      "three devices, " + std::to_string(rounds) + " rounds off seed " + std::to_string(seed),
      {"rounds in step", "conflicts kept", "up", "down", "removed", "cloud bytes written",
       "cloud bytes read"},
      {}};
  t.rows.push_back({std::to_string(converged) + "/" + std::to_string(rounds),
                    std::to_string(conflicts), std::to_string(uploads), std::to_string(downloads),
                    std::to_string(removals), std::to_string(bytes_up),
                    std::to_string(bytes_down)});
  lab.table(t);

  lab.shot({"story",
            "TWO DEVICES, ONE CLOUD",
            "Device A and device B of one player sync their save stores through the simulated "
            "store's cloud, step by step; each block is the three sides' slots after the step, "
            "with what the last sync did.",
            "after step 5 both devices list the same slots; slot-1 holds B's longer run on all "
            "three sides and A's run is kept as slot-1.conflict on all three; step 6 writes 0 B",
            {{"rounds", std::to_string(rounds)}, {"seed", std::to_string(seed)}}},
           draw(steps));
}

const gl::LabSpec kSpec{
    "save-cloud",
    gl::LabKind::sim,
    "engine",
    "engine/save/cloud.hpp, engine/services/save_cloud.hpp",
    "save",
    "Do two devices syncing one player's cloud converge on their slots and keep a conflict?",
    {
        {"rounds", "200", "rounds of three devices saving at random, then syncing"},
        {"seed", "38", "the random rounds' seed"},
    },
    "--rounds=40",
    run,
};
GE_LAB(kSpec);

}  // namespace
