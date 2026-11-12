// LAB `se-sequel-tv`: THE SEQUEL'S MATCH WATCHED THROUGH OUR TV NODE
// (games/shooters_edge/client/sequel_tv.hpp, engine/tv/node.hpp). A client recording of the sequel
// (the demo pack's own with `--pak`, else a made-up one off `--seed`) is played as if live: a block
// every 100 ms into the broadcast, its bytes into a TV node held by `--delay`, and viewers who join
// at the given seconds. It answers what a late viewer is sent to start, the stream's bytes a second
// against the recording's, and whether every viewer's stream unpacks to the whole recording byte
// for byte. The bytes each viewer had are charted over the match beside the recording's.
//
// ge:summary Lab: the sequel's match broadcast live through a TV node to viewers who join late
// ge:keywords lab sequel broadcast tv-node late-viewer delay replay-v2 bytes-per-second dm2
// ge:skill replay shooter labs
// ge:test tests/shooters_edge_sequel_tv_test.cpp
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/sequel_demo.hpp"
#include "shooters_edge/client/sequel_tv.hpp"
#include "tv/node.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
namespace tv = ge::tv;

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

std::vector<double> numbers(const std::string& s) {
  std::vector<double> out;
  size_t at = 0;
  while (at < s.size()) {
    const size_t comma = s.find(',', at);
    out.push_back(std::atof(s.substr(at, comma == std::string::npos ? comma : comma - at).c_str()));
    if (comma == std::string::npos) break;
    at = comma + 1;
  }
  return out;
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
      return lab.fail("no " + lab.arg("demo") + " in " + pak_path,
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
  const std::vector<std::span<const uint8_t>> blocks = cl::sequel_blocks(rec);
  const std::vector<double> joins = numbers(lab.arg("joins"));

  tv::Node node;
  node.config.delay_ms = static_cast<int64_t>(lab.arg_num("delay") * 1000);
  std::vector<tv::Watch> watch(joins.size());
  std::vector<bool> joined(joins.size(), false);
  std::vector<gl::Trace> lines(joins.size() + 1);
  lines[0].label = "the recording as played";
  for (size_t i = 0; i < joins.size(); i++)
    lines[i + 1].label =
        "viewer " + std::to_string(i + 1) + " (joins at " + fmt(joins[i], 0) + " s)";
  const tv::Send send = [&](int link, std::span<const uint8_t> msg) {
    return tv::watch_receive(watch[static_cast<size_t>(link)], msg);
  };
  auto b = std::make_unique<cl::SequelBroadcast>();
  cl::sequel_broadcast_begin(*b, 0);
  double raw = 0;
  int64_t now = 0;
  const size_t tail = static_cast<size_t>(lab.arg_num("delay") * 10) + 20;
  for (size_t i = 0; i < blocks.size() + tail; i++, now += 100) {
    if (i < blocks.size()) {
      cl::sequel_broadcast_block(*b, blocks[i]);
      raw += static_cast<double>(blocks[i].size() + 4);
    } else if (i == blocks.size()) {
      cl::sequel_broadcast_end(*b);
    }
    const std::span<const uint8_t> fresh = cl::sequel_broadcast_fresh(*b);
    if (!fresh.empty()) tv::tv_feed(node, fresh, now);
    if (i == blocks.size()) tv::tv_end(node);
    const double at = static_cast<double>(now) / 1000.0;
    for (size_t v = 0; v < joins.size(); v++) {
      if (joined[v] || at < joins[v]) continue;
      joined[v] = true;
      tv::tv_join(node, static_cast<int>(v));
      tv::tv_receive(node, static_cast<int>(v),
                     tv::tv_hello("viewer " + std::to_string(v + 1), false));
    }
    tv::tv_update(node, now, send);
    if (i % 10 == 0) {
      lines[0].x.push_back(at);
      lines[0].y.push_back(raw / 1024.0);
      for (size_t v = 0; v < joins.size(); v++) {
        if (!joined[v]) continue;
        lines[v + 1].x.push_back(at);
        lines[v + 1].y.push_back(static_cast<double>(watch[v].bytes) / 1024.0);
      }
    }
  }
  const double seconds = static_cast<double>(blocks.size()) / 10.0;
  gl::LabTable table{"each viewer through the node",
                     {"viewer", "joins at s", "blocks", "bytes had", "back exactly"},
                     {}};
  bool all = true;
  std::string error;
  for (size_t v = 0; v < joins.size(); v++) {
    std::vector<uint8_t> back;
    const bool same =
        watch[v].ended && cl::unpack_demo(watch[v].stream, back, error) && back == rec;
    all = all && same;
    table.rows.push_back({std::to_string(v + 1), fmt(joins[v], 0), std::to_string(watch[v].blocks),
                          std::to_string(watch[v].bytes), same ? "yes" : "no"});
    lab.metric("viewer" + std::to_string(v + 1) + ".bytes", static_cast<double>(watch[v].bytes),
               "bytes");
  }
  const double stream = static_cast<double>(b->out.size());
  lab.metric("recording-bytes-per-s", static_cast<double>(rec.size()) / seconds, "bytes/s");
  lab.metric("stream-bytes-per-s", stream / seconds, "bytes/s");
  lab.table(table);
  lab.note(source + ": " + fmt(seconds, 0) + " s of play broadcast at " + fmt(stream / seconds, 0) +
           " bytes a second (the recording " + fmt(static_cast<double>(rec.size()) / seconds, 0) +
           "); a late viewer is sent the match from its start and reads through to now");
  const gl::ChartFrame frame{"THE SEQUEL'S MATCH THROUGH OUR TV NODE",
                             source + "; delay " + lab.arg("delay") + " s", "seconds of play",
                             "bytes so far (KB)"};
  lab.shot({"bytes",
            frame.title,
            "The recording's bytes as it is played, and the bytes each viewer had from the node: "
            "the first after the delay, a late one a step for the match so far, then all at the "
            "broadcast's rate.",
            "the viewers' lines far under the recording's; the late one jumping up to the first's",
            {{"pak", pak_path}, {"joins", lab.arg("joins")}}},
           gl::to_picture(gl::line_chart({frame, lines, {}, {}, {}, false, false, false}).plot));
  if (!all)
    lab.fail("a viewer's stream did not unpack to the recording",
             "next: run tests/shooters_edge_sequel_tv_test.cpp; skill: replay.");
}

const gl::LabSpec kSpec{
    "se-sequel-tv",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/client/sequel_tv: the sequel's match broadcast live through a TV node",
    "replay",
    "Broadcast through our TV node to viewers who join at different times, what does the sequel's "
    "match cost a second, and does every viewer get the whole recording back exactly?",
    {
        {"pak", "", "the sequel demo's baseq2/pak0.pak (empty: a made-up recording)"},
        {"demo", "demos/q2demo1.dm2", "the recording's name in the pack"},
        {"seconds", "60", "the made-up recording's length"},
        {"seed", "7", "the made-up recording's stream"},
        {"delay", "2", "the node's delay, s"},
        {"joins", "0,30", "the seconds the viewers join at, comma-separated"},
    },
    "--seconds=20 --joins=0,10",
    run,
};
GE_LAB(kSpec);

}  // namespace
