// The pipeline (plugins/classicenhance/pipeline.hpp, cache.hpp, lightmaps.hpp) on a fixture pack
// made here: two fixture maps of the brush plugin's writer and a palette. Every stage writes what
// its header says into a temporary cache; a rerun skips every stage; a changed option reruns only
// the textures; --force reruns all; `only` runs the one asked; and a source that was never
// verified is refused before anything is written.
//
// ge:summary Tests the pipeline's stages, cache layout, reruns and refusals on a fixture pack
// ge:keywords test classicenhance pipeline cache stages rerun skip force lightmaps textures
// ge:skill classicenhance testing
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "assets/sha256.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classicenhance/cache.hpp"
#include "classicenhance/pipeline.hpp"
#include "vfs/pak.hpp"

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
  std::vector<uint8_t> out;
  CHECK_MSG(ge::vfs::write_pak({{"gfx/palette.lmp", pal}, {"maps/b_hall.bsp", map},
                                {"maps/a_copy.bsp", map}},
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
  ce::RunReport report;
  CHECK_MSG(ce::run_pipeline(src, root, opts, &report, &error), error);
  const fs::path dir = ce::pack_dir(root, sha);
  CHECK(report.dir == dir);
  CHECK(report.stages.size() == 3);
  for (const ce::StageResult& r : report.stages) CHECK(r.ran && r.files > 0);
  CHECK(fs::file_size(dir / "pack" / "pak0.pak") == src.pack.size());
  CHECK(read_text(dir / "NOTICE.txt").find("do not share") != std::string::npos);
  CHECK(read_text(dir / "source.txt").find(sha) != std::string::npos);
  CHECK(fs::exists(dir / "lightmaps" / "a_copy.png") && fs::exists(dir / "lightmaps" / "b_hall.png"));

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
  CHECK(!report.stages[0].ran && report.stages[1].ran && !report.stages[2].ran);
  opts.force = true;
  opts.only = {ce::Stage::lightmaps};
  CHECK(ce::run_pipeline(src, root, opts, &report, &error));
  CHECK(!report.stages[0].ran && !report.stages[1].ran && report.stages[2].ran);
  CHECK(ce::read_records(dir).size() == 3);
  fs::remove_all(root);
}

TEST(pipeline_refuses_an_unverified_source) {
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
