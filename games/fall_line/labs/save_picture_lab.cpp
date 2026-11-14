// LAB `save-picture`: EVERY SLOT WITH ITS PICTURE (docs/saves.md § Pictures of a save). The
// pilot skis Fall Line's race on each of `--seeds`, and at `--saves` points down each run the
// run is saved with a picture (`play::save_run_pictured`: the game's CPU view, headless, shrunk
// to `--side` and kept as QOI in the `PICT` chunk first after the meta) into a slot of a store
// on the simulated disk. Then the store is opened again as a menu opens it and every slot's
// picture is read through the index (off the file's front, not the whole file) and laid on a
// contact sheet, each under its slot's title. Held: every slot has its picture, and it reads
// back to the pixels the save wrote.
//
//   make lab LAB=save-picture
//   make lab LAB=save-picture ARGS="--seeds=38,7 --saves=6 --side=256"
//
// ge:summary Lab: Fall Line's saves each with a picture, read by the index onto a contact sheet
// ge:keywords lab save-picture picture pict qoi thumbnail contact-sheet store index menu
// ge:skill save sim labs
// ge:test tests/save_picture_test.cpp
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "fall_line/sim/play.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "play/saves.hpp"
#include "save/disk.hpp"
#include "save/file.hpp"
#include "save/picture.hpp"
#include "save/qoi.hpp"
#include "save/store.hpp"

namespace {

namespace fs = fall_line::sim;
namespace gl = ge::lab;
namespace gp = ge::play;
namespace sv = ge::save;

std::string fmt(double v, int places) { return ge::core::num::to_fixed(v, places); }

double ms_since(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

void run(gl::Lab& lab) {
  const std::vector<double> seeds = lab.arg_nums("seeds");
  const int per_run = std::max(1, static_cast<int>(lab.arg_num("saves")));
  const int side = std::max(32, static_cast<int>(lab.arg_num("side")));
  const auto every = static_cast<int64_t>(lab.arg_num("every") * fs::kGame.hz);

  sv::SimDisk disk;
  sv::StoreSpec spec;
  spec.game = "fall_line";
  spec.slots = 100;
  int slot = 0;
  double shoot_ms = 0;
  size_t raw_bytes = 0, picture_bytes = 0, file_bytes = 0;
  std::vector<std::vector<uint8_t>> written;  // each slot's picture as the save holds it
  {
    sv::Store store(disk, spec);
    store.open();
    for (const double seed : seeds) {
      gp::Setup setup;
      std::string error;
      gp::make_setup(fs::kGame, seed, {{"rivals", "3"}, {"hands", "human"}}, setup, error);
      std::unique_ptr<gp::Run> r = fs::kGame.open(setup, error);
      if (!r) {
        lab.fail("save-picture: seed " + fmt(seed, 0) + " does not open: " + error,
                 "next: run make test T=save_picture; read engine/save/picture.cpp; skill: save");
        return;
      }
      std::vector<double> in(fs::kGame.axes.size(), 0);
      int64_t step = 0;
      for (int k = 0; k < per_run; k++) {
        for (const int64_t until = (k + 1) * every; step < until && !r->over(); step++) {
          std::fill(in.begin(), in.end(), 0.0);
          r->pilot(in);
          r->step(in);
        }
        sv::Meta meta;
        meta.title = "Seed " + fmt(seed, 0) + ", " +
                     fmt(static_cast<double>(step) / fs::kGame.hz, 0) + " s down";
        std::vector<uint8_t> file;
        const auto t0 = std::chrono::steady_clock::now();
        if (!gp::save_run_pictured(fs::kGame, setup, *r, step, meta, file, error, {}, side)) {
          lab.fail("save-picture: the save does not write: " + error,
                   "next: run make test T=save_picture; read engine/save/picture.cpp; skill: save");
          return;
        }
        shoot_ms += ms_since(t0);
        sv::FileReader fr;
        int w = 0, h = 0;
        std::vector<uint8_t> px;
        std::string why;
        if (!fr.open(file) || !sv::read_picture(fr, w, h, px, why)) {
          lab.fail("save-picture: the save holds no picture: " + why,
                   "next: run make test T=save_picture; read engine/save/picture.cpp; skill: save");
          return;
        }
        raw_bytes += px.size();
        picture_bytes += fr.find(sv::kPictTag)->stored;
        file_bytes += file.size();
        written.push_back(px);
        slot++;
        if (!store.write("slot-" + std::to_string(slot), file)) {
          lab.fail("save-picture: " + store.problem(),
                   "next: run make test T=save_picture; read engine/save/picture.cpp; skill: save");
          return;
        }
      }
    }
  }

  // Opened again as a menu: the index, then each slot's picture off its file's front.
  sv::SimDisk menu = disk.restarted();
  sv::Store store(menu, spec);
  store.open();
  const int64_t index_reads = menu.reads, index_bytes = menu.bytes_read;
  const int cols = 4;
  const int cell_w = side + 16, cell_h = side + 34;  // the widest side either way
  const int rows = (slot + cols - 1) / cols;
  gl::Picture sheet = gl::create_picture(cols * cell_w + 8, rows * cell_h + 8, {24, 27, 32});
  int shown = 0, same = 0;
  const auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < slot; i++) {
    const std::string name = "slot-" + std::to_string(i + 1);
    gl::Picture p;
    if (!store.picture(name, p.width, p.height, p.rgba)) continue;
    shown++;
    same += p.rgba == written[static_cast<size_t>(i)] ? 1 : 0;
    const int x = 8 + (i % cols) * cell_w, y = 8 + (i / cols) * cell_h;
    gl::blit(sheet, p, x, y);
    const sv::Entry* e = store.find(name);
    gl::draw_text(sheet, x, y + p.height + 6, name, {240, 200, 90});
    gl::draw_text(sheet, x, y + p.height + 16, e->meta.title, {210, 214, 220});
  }
  const double read_ms = ms_since(t0);
  const int64_t picture_reads = menu.reads - index_reads;
  const int64_t picture_read_bytes = menu.bytes_read - index_bytes;

  lab.metric("slots", slot, "count");
  lab.metric("pictured", shown, "count");
  lab.metric("same-pixels", same, "count");
  lab.metric("index.reads", static_cast<double>(index_reads), "count");
  lab.metric("picture.reads", static_cast<double>(picture_reads), "count");
  lab.metric("picture.qoi-bytes", static_cast<double>(picture_bytes) / std::max(slot, 1), "bytes");
  lab.metric("picture.raw-bytes", static_cast<double>(raw_bytes) / std::max(slot, 1), "bytes");
  lab.metric("read.share",
             static_cast<double>(picture_read_bytes) /
                 static_cast<double>(std::max<size_t>(file_bytes, 1)),
             "ratio");
  lab.metric("save-with-picture.ms", shoot_ms / std::max(slot, 1), "ms", gl::Det::measured);
  lab.metric("sheet.read-ms", read_ms, "ms", gl::Det::measured);
  if (shown != slot || same != slot)
    lab.fail("save-picture: " + std::to_string(shown) + " of " + std::to_string(slot) +
                 " slots showed a picture, " + std::to_string(same) + " to the pixel",
             "next: run make test T=save_picture; read engine/save/picture.cpp; skill: save");

  gl::LabTable t{"a slot's picture",
                 {"slots", "QOI bytes each", "raw RGBA each", "menu read of the files"},
                 {{std::to_string(slot), std::to_string(picture_bytes / std::max(slot, 1)),
                   std::to_string(raw_bytes / std::max(slot, 1)),
                   fmt(100.0 * static_cast<double>(picture_read_bytes) /
                           static_cast<double>(std::max<size_t>(file_bytes, 1)),
                       1) +
                       "%"}}};
  lab.table(t);
  lab.shot({"sheet",
            "EVERY SLOT WITH ITS PICTURE",
            "A store of " + std::to_string(slot) +
                " Fall Line saves opened as a menu opens it: "
                "the index read once, then each slot's picture (the race from above round the "
                "player, shot on the CPU when it was saved) read off its file's front.",
            "a picture in every cell, each a different moment of its seed's race",
            {{"seeds", lab.arg("seeds")},
             {"saves", std::to_string(per_run)},
             {"side", std::to_string(side)}}},
           sheet);
}

const gl::LabSpec kSpec{
    "save-picture",
    gl::LabKind::sim,
    "game:fall_line",
    "engine/save/picture.hpp, engine/save/qoi.hpp, engine/save/store.hpp on Fall Line's CPU view",
    "save",
    "Does every slot of a store get a picture headless, and does a menu read them all through "
    "the index onto a contact sheet?",
    {
        {"seeds", "38,7,12", "the race seeds the saves are made on"},
        {"saves", "4", "saves down each run"},
        {"every", "20", "seconds between saves"},
        {"side", "200", "a picture's widest side, pixels"},
    },
    "--seeds=38,7 --saves=2 --every=5 --side=96",
    run,
};
GE_LAB(kSpec);

}  // namespace
