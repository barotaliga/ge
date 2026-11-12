// LAB `se-sequel-pack`: THE SEQUEL'S RECORDINGS ON THE REPLAY FILE VERSION 2
// (games/shooters_edge/client/sequel_demo.hpp). A client recording of the sequel (the demo pack's
// own with `--pak`, else a made-up one off `--seed`) is packed into a version 2 file and back: it
// answers whether every block walked and the bytes came back exactly, and the packed size against
// the recording and against zstd at its chosen level. The bytes are charted a second at a time as
// the recording and as its packed chunks grow.
//
// ge:summary Lab: the sequel's recordings packed into .ged: walked, back exactly, size vs zstd
// ge:keywords lab sequel dm2 replay-v2 pack unpack zstd ratio round-trip field-codec
// ge:skill replay shooter labs
// ge:test tests/shooters_edge_sequel_demo_test.cpp
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/sequel_demo.hpp"
#include "vfs/pak.hpp"
#include "vfs/zstd.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

void run(gl::Lab& lab) {
  std::vector<uint8_t> rec;
  std::string source = "a made-up recording, seed " + lab.arg("seed");
  const std::string pak_path = lab.arg("pak");
  if (!pak_path.empty()) {
    std::ifstream in(pak_path, std::ios::binary);
    const std::vector<uint8_t> pak((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
    ge::vfs::Pak p;
    std::string error;
    const ge::vfs::PakEntry* e = nullptr;
    if (ge::vfs::read_pak(pak, &p, &error)) e = ge::vfs::pak_find(p, lab.arg("demo"));
    if (e == nullptr)
      return lab.fail(
          "no " + lab.arg("demo") + " in " + pak_path + (error.empty() ? "" : ": " + error),
          "next: pass --pak=<the sequel demo's baseq2/pak0.pak>; skill: replay.");
    const std::span<const uint8_t> b = ge::vfs::pak_file(pak, *e);
    rec.assign(b.begin(), b.end());
    source = lab.arg("demo") + " from the demo pack";
  } else {
    rec = cl::sequel_fixture(static_cast<int>(lab.arg_num("seconds")),
                             static_cast<uint32_t>(lab.arg_num("seed")));
  }
  if (!cl::is_sequel_recording(rec))
    return lab.fail("the recording is not framed as the sequel's",
                    "next: run tests/shooters_edge_sequel_demo_test.cpp; skill: replay.");

  std::vector<uint8_t> file, back;
  std::string error;
  cl::DemoPackReport rep;
  const bool packed = cl::pack_demo(rec, file, error, {}, &rep);
  const bool unpacked = packed && cl::unpack_demo(file, back, error);
  const bool same = unpacked && back == rec;
  const size_t zstd = ge::vfs::zstd_pack(rec).size();

  // A second at a time: the recording's bytes and its chunks' as they grow.
  const std::vector<std::span<const uint8_t>> blocks = cl::sequel_blocks(rec);
  cl::SequelCoder coder;
  gl::Trace raw{"the recording", {}, {}}, ged{"its chunks on the replay file", {}, {}};
  double raw_kb = 0, ged_kb = 0;
  for (size_t at = 0, n = 0; at < blocks.size(); at += 10, n++) {
    std::vector<std::span<const uint8_t>> some;
    for (size_t i = at; i < blocks.size() && i < at + 10; i++) {
      some.push_back(blocks[i]);
      raw_kb += static_cast<double>(blocks[i].size() + 4) / 1024.0;
    }
    std::vector<uint8_t> chunk;
    cl::pack_sequel_chunk(coder, some, n % 30 == 0, chunk, nullptr);
    ged_kb += static_cast<double>(chunk.size()) / 1024.0;
    raw.x.push_back(static_cast<double>(n + 1));
    raw.y.push_back(raw_kb);
    ged.x.push_back(static_cast<double>(n + 1));
    ged.y.push_back(ged_kb);
  }

  const double ratio = static_cast<double>(file.size()) / static_cast<double>(rec.size());
  const double vs_zstd = static_cast<double>(file.size()) / static_cast<double>(zstd);
  lab.metric("blocks", static_cast<double>(rep.records), "blocks");
  lab.metric("walked", static_cast<double>(rep.walked), "blocks");
  lab.metric("recording-bytes", static_cast<double>(rec.size()), "bytes");
  lab.metric("ged-bytes", static_cast<double>(file.size()), "bytes");
  lab.metric("zstd-bytes", static_cast<double>(zstd), "bytes");
  lab.metric("ged-over-zstd", vs_zstd, "ratio");
  lab.table({"the recording packed",
             {"source", "blocks", "walked", "kept", "bytes", ".ged", "zstd", ".ged / bytes",
              ".ged / zstd", "back exactly"},
             {{source, std::to_string(rep.records), std::to_string(rep.walked),
               std::to_string(rep.kept), std::to_string(rec.size()), std::to_string(file.size()),
               std::to_string(zstd), fmt(ratio, 3), fmt(vs_zstd, 3), same ? "yes" : "no"}}});
  lab.note(source + ": " + std::to_string(rec.size()) + " bytes pack to " +
           std::to_string(file.size()) + " (" + fmt(100 * ratio, 1) + "%), " +
           fmt(100 * vs_zstd, 0) + "% of zstd's " + std::to_string(zstd));
  const gl::ChartFrame frame{"THE SEQUEL'S RECORDING, PACKED", source, "seconds of play",
                             "bytes so far (KB)"};
  lab.shot(
      {"bytes",
       frame.title,
       "The recording's bytes and its packed chunks' as the play goes on, a second at a time.",
       "the packed line far under the recording's and rising evenly",
       {{"pak", pak_path}, {"seed", lab.arg("seed")}}},
      gl::to_picture(gl::line_chart({frame, {raw, ged}, {}, {}, {}, false, false, false}).plot));
  if (!same)
    lab.fail("the recording did not come back byte for byte" + (error.empty() ? "" : ": " + error),
             "next: run tests/shooters_edge_sequel_demo_test.cpp; skill: replay.");
  if (rep.walked != rep.records)
    lab.fail(std::to_string(rep.kept) + " blocks did not walk and were kept as their bytes",
             "next: read walk in games/shooters_edge/client/sequel_demo.cpp; skill: replay.");
  if (file.size() >= zstd)
    lab.fail("the replay file is not smaller than zstd",
             "next: read the predictions in games/shooters_edge/client/sequel_demo.cpp.");
}

const gl::LabSpec kSpec{
    "se-sequel-pack",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/client/sequel_demo: the sequel's client recordings on the replay file",
    "replay",
    "Does a recording of the sequel pack into the replay file and come back byte for byte, every "
    "block walked, and how much smaller than the recording and than zstd is it?",
    {
        {"pak", "", "the sequel demo's baseq2/pak0.pak (empty: a made-up recording)"},
        {"demo", "demos/q2demo1.dm2", "the recording's name in the pack"},
        {"seconds", "60", "the made-up recording's length"},
        {"seed", "7", "the made-up recording's stream"},
    },
    "--seconds=20",
    run,
};
GE_LAB(kSpec);

}  // namespace
