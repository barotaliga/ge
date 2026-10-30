// The pipeline (plugins/classicenhance/pipeline.hpp, cache.hpp, lightmaps.hpp) on a fixture pack
// made here: two fixture maps of the brush plugin's writer, a palette, a looping 8-bit sound, the
// morph plugin's fixture model and sprite, a wad with a picture, the font sheet and a lump that is
// neither, and a picture under gfx/ with the colour map beside it.
// Every stage writes what its header says into a temporary cache; a rerun skips every stage; a
// changed option reruns only the textures; --force reruns all; `only` runs the one asked; and a
// source that was never verified is refused before anything is written; every stage reports its
// start, its items and its end, a cancel stops the stage running, and the job runs on its thread.
//
// ge:summary Tests the pipeline's stages, cache layout, reruns and refusals on a fixture pack
// ge:keywords test classicenhance pipeline cache stages rerun skip force lightmaps textures sounds
// ge:keywords models sprites replaced glb pictures wad
// ge:skill classicenhance testing
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "assets/gltf.hpp"
#include "assets/sha256.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classicenhance/cache.hpp"
#include "classicenhance/pipeline.hpp"
#include "classicenhance/progress.hpp"
#include "classicenhance/sound.hpp"
#include "lab/png.hpp"
#include "morph/write.hpp"
#include "vfs/pak.hpp"
#include "vfs/wad.hpp"

namespace ce = ge::classicenhance;
namespace fs = std::filesystem;

namespace {

std::vector<uint8_t> fixture_pack() {
  std::vector<uint8_t> pal(768);
  for (size_t i = 0; i < pal.size(); i++) pal[i] = uint8_t((i * 37) >> 3);
  std::vector<uint8_t> map;
  std::string error;
  CHECK_MSG(ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &map, &error),
            error);
  // A tone at the classic 11 025 Hz, 8 bits, looping from its 1000th sample.
  ce::Pcm tone;
  tone.rate = 11025;
  tone.loop_start = 1000;
  for (int i = 0; i < 4000; i++) tone.samples.push_back(float((i % 50) - 25) / 50.0f);
  // The status bar's pictures: a 4x2 picture with a see-through pixel, the font sheet with one,
  // and a lump of another kind the stage leaves alone.
  ge::vfs::IndexedPicture bar;
  bar.width = 4, bar.height = 2, bar.indices = {1, 2, 255, 4, 5, 6, 7, 8};
  std::vector<uint8_t> sheet(size_t{128} * 128, 9);
  sheet[0] = 0;
  std::vector<uint8_t> wad;
  CHECK_MSG(ge::vfs::write_wad({{"SBAR", ge::vfs::kPictureLump, ge::vfs::write_picture(bar)},
                                {"CONCHARS", ge::vfs::kRawLump, sheet},
                                {"PALETTE", 0x40, pal}},
                               &wad, &error),
            error);
  std::vector<uint8_t> out;
  CHECK_MSG(ge::vfs::write_pak(
                {{"gfx/palette.lmp", pal},
                 {"gfx.wad", wad},
                 {"gfx/ranking.lmp", ge::vfs::write_picture(bar)},
                 {"gfx/colormap.lmp", std::vector<uint8_t>(16385, 3)},
                 {"maps/b_hall.bsp", map},
                 {"maps/a_copy.bsp", map},
                 {"sound/ambience/hum1.wav", ce::write_wav(tone)},
                 {"progs/thing.mdl", ge::morph::write_model(ge::morph::sample_model())},
                 {"progs/s_light.spr", ge::morph::write_sprite(ge::morph::sample_sprite())}},
                &out, &error),
            error);
  return out;
}

std::string read_text(const fs::path& p) {
  std::ifstream f(p);
  return {std::istreambuf_iterator<char>(f), {}};
}

}  // namespace

TEST(pipeline_writes_every_stage_and_skips_what_is_current) {
  ce::Source src;
  src.path = "fixture";
  src.pack = fixture_pack();
  const std::string sha = ge::assets::sha256_hex(src.pack);
  const ce::KnownFile known[] = {{sha.c_str(), uint32_t(src.pack.size()), "the fixture pack"}};
  std::string error;
  CHECK_MSG(ce::verify_pack(&src, &error, known), error);

  const fs::path root = fs::temp_directory_path() / "ge_classicenhance_pipeline";
  fs::remove_all(root);
  ce::RunOptions opts;
  opts.textures.scale = 2;
  opts.sounds.extend = 0;  // keep the fixture quick
  ce::RunReport report;
  CHECK_MSG(ce::run_pipeline(src, root, opts, &report, &error), error);
  const fs::path dir = ce::pack_dir(root, sha);
  CHECK(report.dir == dir);
  CHECK(report.stages.size() == 6);
  for (const ce::StageResult& r : report.stages) CHECK(r.ran && r.files > 0);
  CHECK(fs::file_size(dir / "pack" / "pak0.pak") == src.pack.size());
  CHECK(read_text(dir / "NOTICE.txt").find("do not share") != std::string::npos);
  CHECK(read_text(dir / "source.txt").find(sha) != std::string::npos);
  CHECK(fs::exists(dir / "lightmaps" / "a_copy.png") &&
        fs::exists(dir / "lightmaps" / "b_hall.png"));

  // The sound at 48 kHz, its loop start carried to the new rate, named in the index.
  ce::Pcm hum;
  {
    std::ifstream f(dir / "sounds" / "ambience" / "hum1.wav", std::ios::binary);
    const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(f), {}};
    CHECK_MSG(ce::read_wav(bytes, &hum, &error), error);
  }
  CHECK(hum.rate == 48000 && hum.loop_start == 4354);
  CHECK(read_text(dir / "sounds" / "index.txt").find("ambience/hum1.wav ") != std::string::npos);
  // The model as a glTF any loader reads, the sprite's frames, and what the engine draws instead.
  const ge::assets::LoadResult thing =
      ge::assets::load_gltf_file((dir / "models" / "thing.glb").string());
  CHECK_MSG(thing.ok, thing.error);
  CHECK(fs::exists(dir / "sprites" / "s_light" / "00.png") &&
        fs::exists(dir / "sprites" / "s_light" / "02.png"));
  const std::string models = read_text(dir / "models" / "index.txt");
  CHECK(models.find("model thing progs/thing.mdl ") != std::string::npos);
  CHECK(models.find("sprite s_light progs/s_light.spr ") != std::string::npos);
  CHECK(read_text(dir / "replaced.txt").find("progs/s_light.spr flare yes") != std::string::npos);

  // The wad's picture and font sheet and the gfx/ picture, in the palette's colours, the
  // see-through index clear; the colour map and the other lump left out.
  const std::string pictures = read_text(dir / "pictures" / "index.txt");
  CHECK(pictures.find("sbar 4 2 255 gfx.wad") != std::string::npos);
  CHECK(pictures.find("conchars 128 128 0 gfx.wad") != std::string::npos);
  CHECK(pictures.find("gfx/ranking 4 2 255 gfx/ranking.lmp") != std::string::npos);
  CHECK(pictures.find("colormap") == std::string::npos &&
        pictures.find("palette") == std::string::npos);
  {
    const auto read = ge::lab::read_png((dir / "pictures" / "sbar.png").string());
    CHECK(read.has_value());
    const ge::lab::Picture png = read.value_or(ge::lab::Picture{});
    CHECK(png.width == 4 && png.height == 2);
    if (png.rgba.size() == 32) {
      CHECK(png.rgba[0] == uint8_t((3 * 37) >> 3) && png.rgba[3] == 255);  // index 1's red
      CHECK(png.rgba[11] == 0);                                            // index 255
    }
    CHECK(fs::exists(dir / "pictures" / "gfx" / "ranking.png"));
  }

  // Every drawn texture of the fixture once, its maps beside it, named in the index.
  std::vector<ce::PackTexture> textures;
  ce::Palette pal;
  CHECK_MSG(ce::pack_textures(src.pack, &pal, &textures, &error), error);
  CHECK(!textures.empty());
  const std::string index = read_text(dir / "textures" / "index.txt");
  for (const ce::PackTexture& t : textures) {
    CHECK(t.map == "a_copy");  // the first map, by path, that carries it
    const fs::path at = dir / "textures" / ce::safe_name(t.name);
    CHECK(fs::exists(at / "albedo.png"));
    CHECK(index.find(t.name + " ") != std::string::npos);
  }

  // A rerun skips every stage; a new scale reruns the textures alone; --force reruns all.
  CHECK(ce::run_pipeline(src, root, opts, &report, &error));
  for (const ce::StageResult& r : report.stages) CHECK(!r.ran && r.note == "up to date");
  opts.textures.scale = 3;
  CHECK(ce::run_pipeline(src, root, opts, &report, &error));
  CHECK(!report.stages[0].ran && report.stages[1].ran && !report.stages[2].ran &&
        !report.stages[3].ran && !report.stages[4].ran && !report.stages[5].ran);
  opts.force = true;
  opts.only = {ce::Stage::lightmaps};
  CHECK(ce::run_pipeline(src, root, opts, &report, &error));
  CHECK(!report.stages[0].ran && !report.stages[1].ran && report.stages[2].ran &&
        !report.stages[3].ran && !report.stages[4].ran && !report.stages[5].ran);
  CHECK(ce::read_records(dir).size() == 6);
  fs::remove_all(root);
}

TEST(pipeline_refuses_an_unopened_source) {
  ce::Source src;
  src.pack = {1, 2, 3};
  ce::RunReport report;
  std::string error;
  const fs::path root = fs::temp_directory_path() / "ge_classicenhance_unverified";
  fs::remove_all(root);
  CHECK(!ce::run_pipeline(src, root, {}, &report, &error));
  CHECK(!fs::exists(root));
  CHECK(ce::safe_name("*Water0") == "liq_water0" && ce::safe_name("+0Slip") == "anim_0slip");
  CHECK(ce::safe_name("a/b c") == "a_b_c");
}

TEST(pipeline_reports_every_stage_and_stops_when_cancelled) {
  ce::Source src;
  src.path = "fixture";
  src.pack = fixture_pack();
  const std::string sha = ge::assets::sha256_hex(src.pack);
  const ce::KnownFile known[] = {{sha.c_str(), uint32_t(src.pack.size()), "the fixture pack"}};
  std::string error;
  CHECK_MSG(ce::verify_pack(&src, &error, known), error);
  const fs::path root = fs::temp_directory_path() / "ge_classicenhance_reports";
  fs::remove_all(root);

  // Every stage starts and finishes in order, the textures with a picture before and after.
  std::vector<ce::Progress> seen;
  int pictures = 0;
  ce::RunOptions opts;
  opts.textures.scale = 2;
  opts.sounds.extend = 0;
  opts.progress = [&](const ce::Progress& p) {
    if (p.before && p.after && p.after->width == 2 * p.before->width) pictures++;
    seen.push_back(p);
    seen.back().before = seen.back().after = nullptr;
  };
  ce::RunReport report;
  CHECK_MSG(ce::run_pipeline(src, root, opts, &report, &error), error);
  size_t at = 0;
  for (const ce::Stage s : ce::kStages) {
    while (at < seen.size() && seen[at].stage != s) at++;
    CHECK(at < seen.size() && seen[at].kind == ce::Progress::Kind::started);
    const uint32_t total = at < seen.size() ? seen[at].total : 0;
    uint32_t items = 0;
    for (at++; at < seen.size() && seen[at].kind == ce::Progress::Kind::item; at++) items++;
    CHECK_MSG(at < seen.size() && seen[at].kind == ce::Progress::Kind::finished &&
                  seen[at].stage == s && items == total,
              std::string(ce::stage_name(s)) + ": " + std::to_string(items) + " items of " +
                  std::to_string(total));
  }
  CHECK(pictures > 0);
  // A rerun reports every stage up to date.
  seen.clear();
  CHECK(ce::run_pipeline(src, root, opts, &report, &error));
  CHECK(seen.size() == 6);
  for (const ce::Progress& p : seen) CHECK(p.kind == ce::Progress::Kind::skipped);

  // Cancelled on the first texture: the stage fails as cancelled and nothing after it runs.
  std::atomic<bool> stop{false};
  opts.force = true;
  opts.cancel = &stop;
  seen.clear();
  opts.progress = [&](const ce::Progress& p) {
    if (p.stage == ce::Stage::textures && p.kind == ce::Progress::Kind::item) stop.store(true);
    seen.push_back(p);
  };
  CHECK(!ce::run_pipeline(src, root, opts, &report, &error));
  CHECK(error.find("cancelled") != std::string::npos);
  CHECK(seen.back().kind == ce::Progress::Kind::failed && seen.back().stage == ce::Stage::textures);

  // The job runs it on its own thread and its board ends ready.
  fs::remove_all(root);
  ce::EnhanceJob job;
  ce::RunOptions quick;
  quick.textures.scale = 2;
  quick.sounds.extend = 0;
  CHECK(job.start(src, root, quick));
  while (job.running()) std::this_thread::sleep_for(std::chrono::milliseconds(5));
  const ce::BoardView v = job.view();
  CHECK_MSG(v.finished && !v.failed, v.error);
  CHECK(job.report().stages.size() == 6);
  fs::remove_all(root);
}
