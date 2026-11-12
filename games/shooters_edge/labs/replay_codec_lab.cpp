// LAB `replay-codec`: THE REPLAY FILE VERSION 2 AND THE FIELD CODEC ON CLASSIC RECORDINGS. A team
// match of bots on the fixture halls (net/classic_team.hpp), recorded as a multiview recording,
// and every recording `--corpus` and `--file` name (any wrapper, unpacked first) are each packed by
// this game's face into a version 2 file (`.ged`, client/demo_v2.hpp) and unpacked again: each must
// come back byte for byte. Beside it the same recording packed as today's packed demo (zstd 19,
// `.mvdz`) and by xz 6, with the sizes, the ratios and the face's pack and unpack times. The row's
// bar (docs/goals/M57a.md): a multiview recording at most half its `.mvdz`, a 20-minute 4on4
// unpacked in under a second.
//
// The round trips and the `.ged` sizes are deterministic (the coder writes the same bytes
// everywhere); zstd's and xz's sizes are the libraries' and the times are measured.
//
// ge:summary Lab: classic recordings packed into replay v2 by the field codec, sizes and speeds
// ge:keywords lab replay-codec replay-v2 ged field-codec mvdz zstd xz ratio round-trip corpus
// ge:keywords pack unpack speed multiview
// ge:skill replay shooter labs
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_pack.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_team.hpp"
#include "vfs/lzma.hpp"
#include "vfs/unpack.hpp"
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

struct Input {
  std::string name;
  std::vector<uint8_t> bytes;
};

bool read_file(const std::string& path, Input& in, std::string& error) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    error = path + " does not open";
    return false;
  }
  vfs::Unpacked u;
  if (!vfs::unpack_one(std::vector<uint8_t>(std::istreambuf_iterator<char>(f), {}), u, error,
                       cl::demo_name))
    return false;
  in = {path.substr(path.find_last_of("/\\") + 1), std::move(u.bytes)};
  return true;
}

void run(gl::Lab& lab) {
  const double minutes = lab.arg_num("minutes");
  std::string error;
  std::vector<Input> inputs;

  // The match, recorded.
  std::vector<uint8_t> halls;
  if (!ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &halls, &error)) {
    lab.fail("replay-codec: the fixture halls: " + error, "next: run tests/brush_write_test.cpp");
    return;
  }
  ge::progs::Progs progs;
  if (!cl::baseline_progs(progs, error)) {
    lab.fail("replay-codec: the baseline mod: " + error,
             "next: run tests/shooters_edge_baseline_mod_test.cpp");
    return;
  }
  cn::TeamMatchOptions o;
  o.map = halls;
  o.map_name = "halls";
  o.entities = cn::kTeamHallsEntities;
  o.progs = &progs;
  o.frames = static_cast<int>(std::lround(minutes * 60 * 1000 / shooters_edge::bots::kPilotMsec));
  const cn::TeamMatchReport match = cn::run_team_match(o);
  if (!match.ok) {
    lab.fail("replay-codec: the match: " + match.error,
             "next: run tests/shooters_edge_team_test.cpp");
    return;
  }
  inputs.push_back({"bots' match (" + to_fixed(minutes, 1) + " min)", match.mvd});

  // The corpus and the files.
  std::vector<std::string> paths;
  if (const std::string dir = lab.arg("corpus"); !dir.empty()) {
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(dir, ec))
      if (e.is_regular_file()) paths.push_back(e.path().string());
    if (ec) {
      lab.fail("replay-codec: the corpus " + dir + " does not list: " + ec.message(),
               "next: pass --corpus=<a folder of recordings>");
      return;
    }
    std::sort(paths.begin(), paths.end());
  }
  for (std::string rest = lab.arg("file"); !rest.empty();) {
    const size_t comma = rest.find(',');
    paths.push_back(rest.substr(0, comma));
    rest = comma == std::string::npos ? "" : rest.substr(comma + 1);
  }
  for (const std::string& path : paths) {
    Input in;
    if (!read_file(path, in, error)) {
      lab.fail("replay-codec: " + error, "next: name recordings the demo reader opens");
      return;
    }
    inputs.push_back(std::move(in));
  }

  gl::LabTable table{"each recording as a replay file version 2, against today's packed demo",
                     {"recording", "format", "MB plain", "MB .mvdz", "MB xz", "MB .ged",
                      ".ged / .mvdz", "ratio", "kept as bytes", "pack s", "unpack s", "same"},
                     {}};
  gl::LineChart chart;
  chart.frame = {"HOW MANY TIMES SMALLER", "each recording by each packer", "plain size (MB)",
                 "ratio (plain / packed)"};
  chart.points = true;
  chart.spikes = false;
  std::vector<gl::Trace> traces = {
      {"zstd 19 (.mvdz)", {}, {}}, {"xz 6", {}, {}}, {"field codec (.ged)", {}, {}}};
  std::array<double, 256> spent{};  // the packed bytes by message id, every multiview recording
  double spent_total = 0;
  std::array<double, static_cast<size_t>(cl::DemoPart::count)> parts{};
  int same_count = 0;
  double worst_half = 0, slowest_unpack_per_mb = 0;
  for (size_t k = 0; k < inputs.size(); k++) {
    const Input& in = inputs[k];
    const double mb = static_cast<double>(in.bytes.size()) / 1e6;
    std::vector<uint8_t> ged, back;
    cl::DemoPackReport rep;
    const auto t0 = std::chrono::steady_clock::now();
    cl::DemoPackOptions options;
    options.keyframe_seconds = static_cast<int>(lab.arg_num("keyframe-seconds"));
    const bool packed = cl::pack_demo(in.bytes, ged, error, options, &rep);
    const double pack_s = seconds_since(t0);
    const auto t1 = std::chrono::steady_clock::now();
    const bool unpacked = packed && cl::unpack_demo(ged, back, error) && error.empty();
    const double unpack_s = seconds_since(t1);
    const bool same = unpacked && back == in.bytes;
    if (same) same_count++;
    if (!same)
      lab.fail("replay-codec: " + in.name + " did not come back byte for byte" +
                   (error.empty() ? std::string() : ": " + error),
               "next: run tests/shooters_edge_demo_v2_test.cpp; find the first message the walk "
               "writes differently in client/demo_v2.cpp. Skill: replay.");
    const size_t mvdz = vfs::zstd_pack(in.bytes).size();
    const size_t xz = vfs::xz_pack(in.bytes, 6).size();
    const double half = static_cast<double>(ged.size()) / static_cast<double>(mvdz);
    if (rep.format == "multiview") {
      for (size_t id = 0; id < 256; id++) spent[id] += rep.message_bytes[id];
      for (size_t p = 0; p < parts.size(); p++) parts[p] += rep.part_bytes[p];
      spent_total += static_cast<double>(ged.size());
      worst_half = std::max(worst_half, half);
      slowest_unpack_per_mb = std::max(slowest_unpack_per_mb, unpack_s / mb);
    }
    const auto ratio = [&](size_t n) {
      return static_cast<double>(in.bytes.size()) / static_cast<double>(n);
    };
    table.rows.push_back({in.name, rep.format, to_fixed(mb, 2),
                          to_fixed(static_cast<double>(mvdz) / 1e6, 3),
                          to_fixed(static_cast<double>(xz) / 1e6, 3),
                          to_fixed(static_cast<double>(ged.size()) / 1e6, 3), to_fixed(half, 2),
                          to_fixed(ratio(ged.size()), 2),
                          std::to_string(rep.kept) + " of " + std::to_string(rep.records),
                          to_fixed(pack_s, 2), to_fixed(unpack_s, 2), same ? "yes" : "NO"});
    traces[0].x.push_back(mb);
    traces[0].y.push_back(ratio(mvdz));
    traces[1].x.push_back(mb);
    traces[1].y.push_back(ratio(xz));
    traces[2].x.push_back(mb);
    traces[2].y.push_back(ratio(ged.size()));
    if (k == 0) {
      lab.metric("match.plain-bytes", static_cast<double>(in.bytes.size()), "bytes");
      lab.metric("match.ged-bytes", static_cast<double>(ged.size()), "bytes");
      lab.metric("match.records-kept", static_cast<double>(rep.kept), "count");
      lab.metric("match.ged-per-mvdz", half, "x", gl::Det::measured);
    }
  }
  lab.table(table, gl::Det::measured);
  gl::LabTable where{"where the packed bytes go: each message id's share of every multiview .ged",
                     {"message id", "KB packed", "share"},
                     {}};
  std::vector<size_t> ids;
  for (size_t id = 0; id < 256; id++)
    if (spent[id] > 0) ids.push_back(id);
  std::sort(ids.begin(), ids.end(), [&](size_t a, size_t b) { return spent[a] > spent[b]; });
  double listed = 0;
  for (const size_t id : ids) {
    listed += spent[id];
    where.rows.push_back({std::to_string(id), to_fixed(spent[id] / 1e3, 1),
                          to_fixed(100 * spent[id] / std::max(1.0, spent_total), 1) + "%"});
  }
  where.rows.push_back(
      {"records' heads, kept bodies, framing", to_fixed((spent_total - listed) / 1e3, 1),
       to_fixed(100 * (spent_total - listed) / std::max(1.0, spent_total), 1) + "%"});
  lab.table(where, gl::Det::measured);
  gl::LabTable by_part{"where the packed bytes go: the parts of every multiview .ged",
                       {"part", "KB packed", "share"},
                       {}};
  double parted = 0;
  for (size_t k = 0; k < parts.size(); k++) {
    parted += parts[k];
    by_part.rows.push_back({cl::demo_part_name(static_cast<cl::DemoPart>(k)),
                            to_fixed(parts[k] / 1e3, 1),
                            to_fixed(100 * parts[k] / std::max(1.0, spent_total), 1) + "%"});
  }
  by_part.rows.push_back(
      {"the rest (record heads, sounds, stats, effects, framing)",
       to_fixed((spent_total - parted) / 1e3, 1),
       to_fixed(100 * (spent_total - parted) / std::max(1.0, spent_total), 1) + "%"});
  lab.table(by_part, gl::Det::measured);
  lab.metric("round-trips", same_count, "count");
  lab.metric("recordings", static_cast<double>(inputs.size()), "count");
  lab.metric("worst.ged-per-mvdz", worst_half, "x", gl::Det::measured);
  lab.metric("slowest.unpack-s-per-mb", slowest_unpack_per_mb, "s/MB", gl::Det::measured);
  chart.traces = traces;
  chart.rules = {};
  const std::vector<std::pair<std::string, std::string>> args = {{"minutes", lab.arg("minutes")},
                                                                 {"corpus", lab.arg("corpus")}};
  lab.shot(
      {"ratios", chart.frame.title,
       "Each recording packed three ways: today's packed demo (zstd 19), xz, and the replay "
       "file version 2 with the field codec. Higher is smaller.",
       "the field codec's points well above zstd's and xz's for every multiview recording", args},
      gl::to_picture(gl::line_chart(chart).plot), gl::Det::measured);
}

const gl::LabSpec kSpec{
    "replay-codec",
    gl::LabKind::sim,
    "game:shooters_edge",
    "engine/replay (the replay file version 2, the field codec) and client/demo_v2.hpp (the "
    "classic recordings' face): packed, unpacked, measured against today's packed demos",
    "replay",
    "Does every classic recording come back byte for byte from a replay file version 2, how much "
    "smaller is it than today's packed demo, and how fast does it unpack?",
    {
        {"minutes", "2", "the bots' match's length"},
        {"keyframe-seconds", "30", "a keyframe block every this many seconds"},
        {"corpus", "", "a folder of recordings to measure too (any wrapper)"},
        {"file", "", "recordings to measure too (comma-separated paths; any wrapper)"},
    },
    "--minutes=0.1",
    run,
};
GE_LAB(kSpec);

}  // namespace
