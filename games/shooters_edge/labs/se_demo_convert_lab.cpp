// LAB `se-demo-convert`: DOES A DEMO CONVERTED TO ANOTHER FORMAT SHOW THE SAME GAME, without a GPU.
// Each recording is converted (client/demo_convert.hpp: a single-player demo to a multiview
// recording and to a client recording, a multiview recording to a client recording of the lowest
// playing slot's view and to a single-player demo of the same view, a client recording back to a
// multiview one and to a single-player demo of its recorder's view, a sequel recording to
// protocol 34) and both are read to
// their ends by the window's own reader (client/demo.hpp), frame by frame: the eye, the view's
// angles, the status (health, armour, ammunition, the weapon) and, for a pair of one format, the
// entities in view, compared frame for frame (a frame a reading of the demo's clock). By default it
// converts recordings written in code (demo_write.hpp's scripted walk and match, sequel_demo.hpp's
// fixture);
// `--file=<a,b,...>` adds real ones (any wrapper).
//
// What it shows: a table a recording (the pair, the frames each side, the largest eye and angle
// difference, the frames whose status or entities differ, the bytes each side, what was dropped
// and rewritten) and the eye's height on both sides over the frames of the first recording.
// Everything is deterministic.
//
// ge:summary Lab: demos converted to another format, read back frame for frame against the source
// ge:keywords lab se-demo-convert demo-convert dem mvd dm2 protocol-26 frame-for-frame eye stats
// ge:skill shooter labs
// ge:test tests/shooters_edge_demo_convert_test.cpp
// voice: agent (a lab's harness)
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_convert.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/client/sequel_demo.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
using ge::core::num::to_fixed;

struct Frame {
  ge::shooter::Vec3f eye, angles;
  int status[5] = {};
  int entities = 0;  // entities in view
  bool view = true;  // the view's player is there (a client recording's signon has none yet)
  double time = 0;   // the demo's clock, s
};

// `track`: a multiview recording's player to ride (-1 the reader's own choice); `player`: the
// view's slot at the end.
bool read_frames(std::vector<uint8_t> bytes, std::vector<Frame>& out, cl::DemoFormat& format,
                 std::string& error, int track = -1, int* player = nullptr) {
  cl::Demo d;
  if (!cl::open_demo(std::move(bytes), d, error)) return false;
  format = d.state.format;
  d.track = track;
  cl::DemoRead r;
  while ((r = cl::next_frame(d, error)) == cl::DemoRead::frame) {
    const cl::DemoState& s = d.state;
    if (s.map.empty()) continue;
    Frame f;
    f.eye = s.eye;
    f.angles = s.angles;
    const int k[5] = {cl::kStatHealth, cl::kStatArmour, cl::kStatAmmo, cl::kStatWeapon,
                      cl::kStatItems};
    for (int i = 0; i < 5; i++) f.status[i] = s.stats[k[i]];
    for (const cl::DemoEntity& e : s.entities) f.entities += e.present ? 1 : 0;
    f.time = s.time;
    if (s.format == cl::DemoFormat::client || s.format == cl::DemoFormat::multiview)
      f.view = s.player >= 0 && static_cast<size_t>(s.player) < s.players.size() &&
               s.players[static_cast<size_t>(s.player)].present;
    // Frames at one clock reading are one server frame (a single-player demo's reliable blocks
    // read as frames of their own): the last one stands.
    if (!out.empty() && out.back().time == f.time)
      out.back() = f;
    else
      out.push_back(f);
  }
  if (player != nullptr) *player = d.state.player;
  return r == cl::DemoRead::end;
}

double angle_gap(float a, float b) {
  const double d = std::fmod(std::fabs(static_cast<double>(a) - b), 360.0);
  return std::min(d, 360 - d);
}

struct Source {
  std::string name;
  std::vector<uint8_t> bytes;
};

void run(gl::Lab& lab) {
  std::vector<Source> sources = {
      {"scripted .dem (halls)",
       cl::write_single_demo(cl::loop_script("halls", {128, 192, 24}, 64, 240))},
      {"sequel fixture .dm2", cl::sequel_fixture(6, 7)},
      {"scripted client recording (halls)",
       cl::write_client_demo(cl::loop_script("halls", {128, 192, 24}, 64, 240))},
      {"scripted match .mvd (halls)", cl::write_multiview_match(cl::highlight_match())}};
  const std::string files = lab.arg("file");
  for (size_t at = 0; at < files.size();) {
    size_t comma = files.find(',', at);
    if (comma == std::string::npos) comma = files.size();
    const std::string path = files.substr(at, comma - at);
    at = comma + 1;
    if (path.empty()) continue;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
      lab.fail("se-demo-convert: " + path + " does not open", "next: pass --file=<paths>");
      return;
    }
    sources.push_back({path.substr(path.find_last_of("/\\") + 1),
                       {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()}});
  }
  gl::LabTable t{"each recording converted and read back against its source",
                 {"recording", "pair", "frames", "eye gap u", "angle gap deg", "status differs",
                  "entities differ", "bytes", "dropped", "rewritten"},
                 {}};
  gl::Trace src_z, dst_z;
  int64_t frames_total = 0, differ_total = 0;
  for (size_t i = 0; i < sources.size(); i++) {
    const Source& s = sources[i];
    std::vector<Frame> a, b;
    cl::DemoFormat fa{}, fb{};
    std::string error;
    int recorder = -1;  // a client recording's own slot
    int rode = -1;      // the view a multiview recording's client recording rode
    if (!read_frames(s.bytes, a, fa, error, -1, &recorder)) {
      lab.fail("se-demo-convert: " + s.name + " does not read: " + error,
               "next: check the file; skill: shooter");
      return;
    }
    for (const cl::DemoFormat to :
         fa == cl::DemoFormat::single
             ? std::vector<cl::DemoFormat>{cl::DemoFormat::multiview, cl::DemoFormat::client}
         : fa == cl::DemoFormat::multiview
             ? std::vector<cl::DemoFormat>{cl::DemoFormat::client, cl::DemoFormat::single}
         : fa == cl::DemoFormat::client
             ? std::vector<cl::DemoFormat>{cl::DemoFormat::multiview, cl::DemoFormat::single}
             : std::vector<cl::DemoFormat>{fa}) {
      b.clear();
      const std::string key = std::to_string(i) + (to == cl::DemoFormat::client   ? "c"
                                                   : to == cl::DemoFormat::single ? "s"
                                                                                  : "");
      std::vector<uint8_t> out;
      cl::ConvertNotes notes;
      cl::ConvertOptions opt;
      // A multiview recording made a single-player demo: the view its client recording rode.
      if (fa == cl::DemoFormat::multiview && to == cl::DemoFormat::single) opt.pov = rode;
      if (!cl::convert_demo(s.bytes, to, opt, out, notes, error)) {
        lab.fail("se-demo-convert: " + s.name + " does not convert: " + error,
                 "next: read client/demo_convert.hpp's pairs; skill: shooter");
        return;
      }
      const size_t out_bytes = out.size();
      int view = -1;
      // A client recording made multiview: read riding the recorder.
      const int ride = fa == cl::DemoFormat::client ? recorder : -1;
      if (!read_frames(std::move(out), b, fb, error, ride, &view)) {
        lab.fail("se-demo-convert: " + s.name + " converted does not read back: " + error,
                 "next: the converter wrote a message the reader refuses; skill: shooter");
        return;
      }
      if (to == cl::DemoFormat::client) rode = view;
      // One view of a multiview recording: the source read riding the same player.
      if (fa == cl::DemoFormat::multiview && to != fa) {
        if (to == cl::DemoFormat::single) view = rode;
        a.clear();
        if (!read_frames(s.bytes, a, fa, error, view)) {
          lab.fail("se-demo-convert: " + s.name + " does not read riding slot " +
                       std::to_string(view) + ": " + error,
                   "next: check the file; skill: shooter");
          return;
        }
      }
      // The two sides start apart by a few frames (a single-player demo reads its signon as a
      // frame of its own; a client recording its signon packets): the offset within 8 frames
      // where the most eyes agree is where they line up.
      size_t lead = 0, lead_b = 0;
      int64_t best = -1;
      for (int off = -8; off <= 8; off++) {
        const size_t la = off > 0 ? static_cast<size_t>(off) : 0;
        const size_t lb = off < 0 ? static_cast<size_t>(-off) : 0;
        int64_t agree = 0;
        for (size_t k = 0; la + k < a.size() && lb + k < b.size(); k++) {
          const Frame& x = a[la + k];
          const Frame& y = b[lb + k];
          agree += std::fabs(x.eye.x - y.eye.x) <= 0.25f && std::fabs(x.eye.y - y.eye.y) <= 0.25f &&
                           std::fabs(x.eye.z - y.eye.z) <= 0.25f
                       ? 1
                       : 0;
        }
        if (agree > best) {
          best = agree;
          lead = la;
          lead_b = lb;
        }
      }
      double eye = 0, ang = 0;
      int64_t status = 0, ents = 0;
      // The frames paired: one for one from the leads; a single-player demo fills a gap longer
      // than a tenth of a second with frames of its own, so there each source frame is paired
      // with the converted frame at its time (the clocks apart by the leads' difference).
      std::vector<std::pair<size_t, size_t>> pairs;
      if (to == cl::DemoFormat::single && lead < a.size() && lead_b < b.size()) {
        // The clocks' difference: at the first source frame whose eye moved, the converted frame
        // near it with the same eye (a still eye matches many frames).
        double shift = b[lead_b].time - a[lead].time;
        auto same = [](const Frame& x, const Frame& y) {
          return x.eye.x == y.eye.x && x.eye.y == y.eye.y && x.eye.z == y.eye.z;
        };
        bool found = false;
        for (size_t ia = lead + 1; ia < a.size() && !found; ia++) {
          if (!a[ia].view || same(a[ia], a[ia - 1])) continue;
          const size_t near = lead_b + (ia - lead);
          for (size_t ib = near > 32 ? near - 32 : 0; ib < std::min(b.size(), near + 32); ib++)
            if (b[ib].view && same(a[ia], b[ib])) {
              shift = b[ib].time - a[ia].time;
              found = true;
              break;
            }
        }
        for (size_t ia = lead, ib = lead_b; ia < a.size(); ia++) {
          while (ib < b.size() && b[ib].time - shift < a[ia].time - 0.0007) ib++;
          if (ib == b.size()) break;
          if (b[ib].time - shift <= a[ia].time + 0.0007) pairs.emplace_back(ia, ib);
        }
      } else {
        for (size_t k = 0; lead_b + k < b.size() && lead + k < a.size(); k++)
          pairs.emplace_back(lead + k, lead_b + k);
      }
      for (size_t k = 0; k < pairs.size(); k++) {
        const Frame& x = a[pairs[k].first];
        const Frame& y = b[pairs[k].second];
        if (!x.view || !y.view) continue;  // a signon's frame: no eye yet
        eye = std::max({eye, std::fabs(static_cast<double>(x.eye.x) - y.eye.x),
                        std::fabs(static_cast<double>(x.eye.y) - y.eye.y),
                        std::fabs(static_cast<double>(x.eye.z) - y.eye.z)});
        ang = std::max({ang, angle_gap(x.angles.x, y.angles.x), angle_gap(x.angles.y, y.angles.y)});
        // A single-player demo's armour and ammunition are bytes: past 255 they show as 255.
        int want[5];
        for (int j = 0; j < 5; j++)
          want[j] = to == cl::DemoFormat::single && (j == 1 || j == 2) ? std::min(x.status[j], 255)
                                                                       : x.status[j];
        status += std::equal(std::begin(want), std::end(want), std::begin(y.status)) ? 0 : 1;
        // A single-player demo's reader keeps an entity once seen, so only same-format pairs count.
        if (fa == to) ents += x.entities == y.entities ? 0 : 1;
        if (i == 0 && to == cl::DemoFormat::client) {
          src_z.x.push_back(static_cast<double>(k));
          src_z.y.push_back(x.eye.z);
          dst_z.x.push_back(static_cast<double>(k));
          dst_z.y.push_back(y.eye.z);
        }
      }
      int64_t dropped = 0, rewritten = 0;
      for (const auto& [w, n] : notes.dropped) dropped += n;
      for (const auto& [w, n] : notes.rewritten) rewritten += n;
      t.rows.push_back({s.name, cl::format_words(fa) + " to " + cl::format_words(to),
                        std::to_string(a.size()) + " / " + std::to_string(b.size()),
                        to_fixed(eye, 3), to_fixed(ang, 4), std::to_string(status),
                        std::to_string(ents),
                        std::to_string(s.bytes.size()) + " / " + std::to_string(out_bytes),
                        std::to_string(dropped), std::to_string(rewritten)});
      frames_total += static_cast<int64_t>(b.size());
      differ_total += status + ents + (eye > 0.25 ? 1 : 0);
      lab.metric("convert." + key + ".frames", static_cast<double>(b.size()), "count");
      lab.metric("convert." + key + ".eye-gap", eye, "u");
      lab.metric("convert." + key + ".status-differs", static_cast<double>(status), "count");
      lab.metric("convert." + key + ".bytes", static_cast<double>(out_bytes), "bytes");
    }
  }
  lab.table(t);
  lab.metric("convert.frames", static_cast<double>(frames_total), "count");
  lab.metric("convert.differing", static_cast<double>(differ_total), "count");
  gl::LineChart c;
  c.spikes = false;
  c.frame = {"THE EYE'S HEIGHT, SOURCE AND CONVERTED",
             sources[0].name + ": the eye's height a frame, read from each file",
             "frame",
             "z (units)",
             960,
             360};
  src_z.label = "source";
  dst_z.label = "converted";
  c.traces = {src_z, dst_z};
  const gl::Chart ch = gl::line_chart(c);
  lab.shot({"eye",
            ch.title,
            "The first recording's eye height frame by frame, as the window reads the source and "
            "as it reads the converted file.",
            "the two lines lie on each other",
            {}},
           gl::to_picture(ch.plot));
  if (differ_total > 0)
    lab.fail("se-demo-convert: " + std::to_string(differ_total) +
                 " frames differ between a source and its conversion",
             "next: read the table's row; skill: shooter");
}

const gl::LabSpec kSpec{
    "se-demo-convert",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/client: demo formats converted (demo_convert.hpp) and read back",
    "shooter",
    "Does a demo converted to another format show the same game, frame for frame: the eye, the "
    "view, the status and the entities?",
    {
        {"file", "", "more recordings to convert (comma-separated paths; any wrapper)"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
