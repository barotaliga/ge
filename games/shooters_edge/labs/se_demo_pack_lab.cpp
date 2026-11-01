// LAB `se-demo-pack`: RECORDINGS PACKED, AND PLAYED OUT OF ARCHIVES. A team match of bots on the
// fixture halls (net/classic_team.hpp) is recorded twice, once plainly as a multiview recording and
// once packed as it goes (`.mvdz`: net/classic_record.hpp over engine/vfs/zstd.hpp); the packed
// file must unpack to the plain one's bytes exactly. The plain recording is then wrapped every way
// a player may hand it over (gzip, zstd, xz, tar.gz, tar.xz, zip, zip of zstd, 7z, each with a
// readme beside it where an archive can hold two) and played by the demo reader to its end out of
// each (client/demo.hpp over engine/vfs/unpack.hpp). Last, the codec table: each recording (the
// match's, and every `--file`, unpacked first) packed by zstd at several levels with and without
// the long window and by xz, with the size, the ratio, the pack and unpack speeds and the packer's
// memory. That table is how the packed formats' setting (ZstdLevel's default) was chosen:
// docs/goals/M23j.md.
//
// The round trip and the frames read are deterministic; the sizes, the ratios and the speeds are
// measured (the sizes are the library's and the same everywhere, but kept out of the digest so a
// moved pin does not fail it).
//
// ge:summary Lab: recordings packed as they go, played out of every archive, the codecs measured
// ge:keywords lab se-demo-pack packed-demo mvdz qwdz zstd xz 7z zip tar-gz codec ratio speed
// ge:keywords pack unpack long-window level choose archive
// ge:skill shooter vfs labs
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_pack.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_team.hpp"
#include "vfs/lzma.hpp"
#include "vfs/tar.hpp"
#include "vfs/unpack.hpp"
#include "vfs/zip.hpp"
#include "vfs/zstd.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
namespace cn = shooters_edge::net;
namespace vfs = ge::vfs;
using ge::core::num::to_fixed;

double seconds_since(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

// The seconds `f` takes, the best of as many runs as fit in about a quarter second (at least one).
template <typename F>
double best_time(F&& f) {
  double best = 1e30, spent = 0;
  for (int i = 0; i < 20 && (i == 0 || spent < 0.25); i++) {
    const auto t0 = std::chrono::steady_clock::now();
    f();
    const double s = seconds_since(t0);
    best = std::min(best, s);
    spent += s;
  }
  return best;
}

// "zstd 19 + long 128 MiB" -> "zstd-19-long-128-mib": a metric key's form.
std::string kebab(const std::string& s) {
  std::string out;
  for (const char ch : s) {
    const char c = static_cast<char>(ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch);
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
      out += c;
    else if (!out.empty() && out.back() != '-')
      out += '-';
  }
  while (!out.empty() && out.back() == '-') out.pop_back();
  return out;
}

struct Codec {
  std::string name;  // the table's
  std::string family;
  vfs::ZstdLevel z;  // zstd's setting
  int xz = -1;       // xz's preset (-1: zstd)
};

// The frames a demo reads to its end, or -1 with the reason.
int64_t frames_of(std::vector<uint8_t> bytes, std::string& error) {
  cl::Demo d;
  if (!cl::open_demo(std::move(bytes), d, error)) return -1;
  for (;;) {
    const cl::DemoRead r = cl::next_frame(d, error);
    if (r == cl::DemoRead::error) return -1;
    if (r == cl::DemoRead::end) return d.state.frames;
  }
}

void run(gl::Lab& lab) {
  const double minutes = lab.arg_num("minutes");
  const bool full = lab.arg("codecs") == "all";
  std::string error;

  // The match, recorded plainly and packed.
  std::vector<uint8_t> halls;
  if (!ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &halls, &error)) {
    lab.fail("se-demo-pack: the fixture halls: " + error, "next: run tests/brush_write_test.cpp");
    return;
  }
  ge::progs::Progs progs;
  if (!cl::baseline_progs(progs, error)) {
    lab.fail("se-demo-pack: the baseline mod: " + error,
             "next: run tests/shooters_edge_baseline_mod_test.cpp");
    return;
  }
  cn::TeamMatchOptions o;
  o.map = halls;
  o.map_name = "halls";
  o.entities = cn::kTeamHallsEntities;
  o.progs = &progs;
  o.frames = static_cast<int>(std::lround(minutes * 60 * 1000 / shooters_edge::bots::kPilotMsec));
  const cn::TeamMatchReport plain = cn::run_team_match(o);
  o.packed = true;
  const auto t_packed = std::chrono::steady_clock::now();
  const cn::TeamMatchReport packed = cn::run_team_match(o);
  const double match_s = seconds_since(t_packed);
  if (!plain.ok || !packed.ok) {
    lab.fail("se-demo-pack: the match: " + (plain.ok ? packed.error : plain.error),
             "next: run tests/shooters_edge_team_test.cpp");
    return;
  }
  std::vector<uint8_t> back;
  bool cut = true;
  const bool unpacked = vfs::zstd_unpack(packed.mvd, back, cut, error) && !cut;
  const bool same = unpacked && back == plain.mvd;
  lab.metric("pack.round-trip", same ? 1 : 0, "bool");
  lab.metric("pack.plain-bytes", static_cast<double>(plain.mvd.size()), "bytes");
  lab.metric("pack.packed-bytes", static_cast<double>(packed.mvd.size()), "bytes",
             gl::Det::measured);
  lab.metric("pack.match-seconds-packed", match_s, "s", gl::Det::measured);
  if (!same)
    lab.fail("se-demo-pack: the packed recording does not unpack to the plain one's bytes" +
                 (unpacked ? std::string() : " (" + error + ")"),
             "next: run tests/shooters_edge_classic_record_test.cpp; check feed() in "
             "net/classic_record.cpp. Skill: net.");

  // Played out of every wrapper.
  const std::vector<uint8_t>& mvd = plain.mvd;
  const std::vector<uint8_t> readme = {'n', 'o', 't', 'e', 's'};
  const std::vector<std::pair<std::string, std::vector<uint8_t>>> wrapped = {
      {"plain .mvd", mvd},
      {".mvdz (packed as recorded)", packed.mvd},
      {".mvd.gz", vfs::write_gzip(mvd, "match.mvd")},
      {".mvd.zst", vfs::zstd_pack(mvd, {3, 0, false})},
      {".mvd.xz", vfs::xz_pack(mvd, 6)},
      {".tar.gz", vfs::write_gzip(vfs::write_tar({{"README", readme}, {"match.mvd", mvd}}))},
      {".tar.xz", vfs::xz_pack(vfs::write_tar({{"match.mvd", mvd}}), 6)},
      {".zip", vfs::write_stored_zip({{"readme.txt", readme}, {"demos/match.mvd", mvd}})},
      {".zip of .mvdz", vfs::write_stored_zip({{"match.mvdz", packed.mvd}})},
      {".7z", vfs::write_7z("match.mvd", mvd, 6)},
  };
  gl::LabTable opened{"the match's recording played out of every wrapper",
                      {"file", "bytes", "layers peeled", "frames read"},
                      {}};
  int64_t want = -1;
  int opened_ok = 0;
  for (const auto& [name, bytes] : wrapped) {
    cl::Demo d;
    std::string why;
    std::string layers = "none";
    if (cl::open_demo(bytes, d, why) && !d.layers.empty()) {
      layers.clear();
      for (const vfs::Wrapping w : d.layers)
        layers += (layers.empty() ? "" : " > ") + std::string(vfs::wrapping_name(w));
    }
    const int64_t frames = frames_of(bytes, why);
    if (want < 0) want = frames;
    if (frames >= 0 && frames == want) opened_ok++;
    opened.rows.push_back({name, std::to_string(bytes.size()), layers,
                           frames < 0 ? "refused: " + why : std::to_string(frames)});
  }
  // The table's byte sizes are the codecs' and kept out of the digest with the codec table; the
  // frames read are the demo reader's.
  lab.table(opened, gl::Det::measured);
  lab.metric("archive.frames", static_cast<double>(want), "count");
  lab.metric("archive.opened", opened_ok, "count");
  if (opened_ok != static_cast<int>(wrapped.size()))
    lab.fail("se-demo-pack: " + std::to_string(wrapped.size() - opened_ok) +
                 " wrapper(s) did not play to the same frame count",
             "next: read the table's refusal; run tests/vfs_unpack_test.cpp. Skill: vfs.");

  // The codec table over the match and every --file.
  std::vector<std::pair<std::string, std::vector<uint8_t>>> inputs = {
      {"the match (" + to_fixed(minutes, 1) + " min, multiview)", mvd}};
  std::vector<std::string> paths;
  for (std::string rest = lab.arg("file"); !rest.empty();) {
    const size_t comma = rest.find(',');
    paths.push_back(rest.substr(0, comma));
    rest = comma == std::string::npos ? "" : rest.substr(comma + 1);
  }
  for (const std::string& path : paths) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
      lab.fail("se-demo-pack: " + path + " does not open", "next: pass --file=<a demo's path>");
      return;
    }
    vfs::Unpacked u;
    if (!vfs::unpack_one(std::vector<uint8_t>(std::istreambuf_iterator<char>(f), {}), u, error,
                         cl::demo_name)) {
      lab.fail("se-demo-pack: " + path + ": " + error, "next: name a demo file");
      return;
    }
    inputs.push_back({path.substr(path.find_last_of("/\\") + 1), std::move(u.bytes)});
  }
  std::vector<Codec> codecs = {
      {"zstd 3", "zstd", {3, 0, false}},
      {"zstd 19 (the packed formats')", "zstd", {}},
      {"xz 6", "xz", {}, 6},
  };
  if (full)
    codecs = {
        {"zstd 1", "zstd", {1, 0, false}},
        {"zstd 3", "zstd", {3, 0, false}},
        {"zstd 9", "zstd", {9, 0, false}},
        {"zstd 15", "zstd", {15, 0, false}},
        {"zstd 19", "zstd", {19, 0, false}},
        {"zstd 22", "zstd", {22, 0, false}},
        {"zstd 9 + long 128 MiB", "zstd long", {9, 27, true}},
        {"zstd 15 + long 128 MiB", "zstd long", {15, 27, true}},
        {"zstd 19 + long 128 MiB", "zstd long", {19, 27, true}},
        {"zstd 22 + long 128 MiB", "zstd long", {22, 27, true}},
        {"xz 6", "xz", {}, 6},
        {"xz 9", "xz", {}, 9},
    };
  gl::LabTable table{"each recording packed by each codec",
                     {"recording", "codec", "MB plain", "MB packed", "ratio", "pack MB/s",
                      "unpack MB/s", "pack MiB held"},
                     {}};
  gl::LineChart scatter;
  scatter.frame = {"RATIO AGAINST UNPACK SPEED", "each recording by each codec", "unpack (MB/s)",
                   "ratio (plain / packed)"};
  scatter.points = true;
  scatter.spikes = false;
  std::vector<gl::Trace> traces;
  for (size_t k = 0; k < inputs.size(); k++) {
    const std::string& name = inputs[k].first;
    const std::vector<uint8_t>& plain_bytes = inputs[k].second;
    const double mb = static_cast<double>(plain_bytes.size()) / 1e6;
    for (const Codec& c : codecs) {
      std::vector<uint8_t> out;
      const double pack_s = c.xz >= 0 ? best_time([&] { out = vfs::xz_pack(plain_bytes, c.xz); })
                                      : best_time([&] { out = vfs::zstd_pack(plain_bytes, c.z); });
      std::vector<uint8_t> again;
      bool truncated = false;
      std::string why;
      const double unpack_s = best_time([&] {
        if (c.xz >= 0)
          vfs::xz_unpack(out, again, why);
        else
          vfs::zstd_unpack(out, again, truncated, why);
      });
      if (again != plain_bytes)
        lab.fail("se-demo-pack: " + c.name + " did not give " + name + " back",
                 "next: run tests/vfs_zstd_test.cpp and tests/vfs_unpack_test.cpp");
      const double ratio =
          static_cast<double>(plain_bytes.size()) / static_cast<double>(out.size());
      table.rows.push_back(
          {name, c.name, to_fixed(mb, 2), to_fixed(static_cast<double>(out.size()) / 1e6, 3),
           to_fixed(ratio, 2), to_fixed(mb / pack_s, 1), to_fixed(mb / unpack_s, 0),
           c.xz >= 0 ? "-"
                     : to_fixed(static_cast<double>(vfs::zstd_pack_memory(c.z)) / 1048576.0, 0)});
      const std::string key = "codec." + std::to_string(k) + "." + kebab(c.name);
      lab.metric(key + ".ratio", ratio, "x", gl::Det::measured);
      lab.metric(key + ".unpack-mbps", mb / unpack_s, "MB/s", gl::Det::measured);
      auto it = std::find_if(traces.begin(), traces.end(),
                             [&](const gl::Trace& t) { return t.label == c.family; });
      if (it == traces.end()) {
        traces.push_back({c.family, {}, {}});
        it = traces.end() - 1;
      }
      it->x.push_back(mb / unpack_s);
      it->y.push_back(ratio);
    }
  }
  lab.table(table, gl::Det::measured);
  scatter.traces = traces;
  const std::vector<std::pair<std::string, std::string>> in = {{"minutes", lab.arg("minutes")},
                                                               {"codecs", lab.arg("codecs")}};
  lab.shot(
      {"ratio-speed", scatter.frame.title,
       "Each recording packed by each codec: how many times smaller it got against how fast "
       "it unpacks.",
       "zstd long up and to the right of xz: as small or nearly, many times faster to unpack", in},
      gl::to_picture(gl::line_chart(scatter).plot), gl::Det::measured);
  lab.attach("match.mvdz", packed.mvd, gl::Det::measured);
}

const gl::LabSpec kSpec{
    "se-demo-pack",
    gl::LabKind::sim,
    "game:shooters_edge",
    "net/classic_record.hpp (recordings packed as they go) and client/demo.hpp's open_demo over "
    "engine/vfs/unpack.hpp and zstd.hpp: packed recordings, archives, the codec's setting",
    "vfs",
    "Does a recording packed as it is written give back the plain one's bytes, does a recording "
    "play out of every archive a player may hand over, and which codec setting packs recordings "
    "smallest while unpacking fast?",
    {
        {"minutes", "2", "the match's length"},
        {"file", "", "demos to measure the codecs on too (comma-separated paths; any wrapper)"},
        {"codecs", "quick",
         "quick (three) or all (zstd 1..22 with and without the long window, xz)"},
    },
    "--minutes=0.1",
    run,
};
GE_LAB(kSpec);

}  // namespace
