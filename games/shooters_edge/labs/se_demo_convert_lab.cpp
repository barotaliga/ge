// LAB `se-demo-convert`: DOES A DEMO CONVERTED TO ANOTHER FORMAT SHOW THE SAME GAME, without a GPU.
// Each recording is converted (client/demo_convert.hpp: a single-player demo to a multiview
// recording, a sequel recording to protocol 34) and both are read to their ends by the window's
// own reader (client/demo.hpp), frame by frame: the eye, the view's angles, the status (health,
// armour, ammunition, the weapon) and, for a pair of one format, the entities in view, compared
// frame for frame (a frame a reading of the demo's clock). By default
// it converts recordings written in code (demo_write.hpp's scripted walk, sequel_demo.hpp's
// fixture); `--file=<a,b,...>` adds real ones (any wrapper).
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
  double time = 0;   // the demo's clock, s
};

bool read_frames(std::vector<uint8_t> bytes, std::vector<Frame>& out, cl::DemoFormat& format,
                 std::string& error) {
  cl::Demo d;
  if (!cl::open_demo(std::move(bytes), d, error)) return false;
  format = d.state.format;
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
    // Frames at one clock reading are one server frame (a single-player demo's reliable blocks
    // read as frames of their own): the last one stands.
    if (!out.empty() && out.back().time == f.time)
      out.back() = f;
    else
      out.push_back(f);
  }
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
  std::vector<Source> sources = {{"scripted .dem (halls)", cl::write_single_demo(cl::loop_script(
                                                               "halls", {128, 192, 24}, 64, 240))},
                                 {"sequel fixture .dm2", cl::sequel_fixture(6, 7)}};
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
    if (!read_frames(s.bytes, a, fa, error)) {
      lab.fail("se-demo-convert: " + s.name + " does not read: " + error,
               "next: check the file; skill: shooter");
      return;
    }
    const cl::DemoFormat to = fa == cl::DemoFormat::single ? cl::DemoFormat::multiview : fa;
    std::vector<uint8_t> out;
    cl::ConvertNotes notes;
    if (!cl::convert_demo(s.bytes, to, {}, out, notes, error)) {
      lab.fail("se-demo-convert: " + s.name + " does not convert: " + error,
               "next: read client/demo_convert.hpp's pairs; skill: shooter");
      return;
    }
    const size_t out_bytes = out.size();
    if (!read_frames(std::move(out), b, fb, error)) {
      lab.fail("se-demo-convert: " + s.name + " converted does not read back: " + error,
               "next: the converter wrote a message the reader refuses; skill: shooter");
      return;
    }
    // A single-player demo reads its signon as a frame of its own; the target's rides its first.
    const size_t lead = a.size() > b.size() ? a.size() - b.size() : 0;
    double eye = 0, ang = 0;
    int64_t status = 0, ents = 0;
    for (size_t k = 0; k < b.size() && lead + k < a.size(); k++) {
      const Frame& x = a[lead + k];
      const Frame& y = b[k];
      eye = std::max({eye, std::fabs(static_cast<double>(x.eye.x) - y.eye.x),
                      std::fabs(static_cast<double>(x.eye.y) - y.eye.y),
                      std::fabs(static_cast<double>(x.eye.z) - y.eye.z)});
      ang = std::max({ang, angle_gap(x.angles.x, y.angles.x), angle_gap(x.angles.y, y.angles.y)});
      status += std::equal(std::begin(x.status), std::end(x.status), std::begin(y.status)) ? 0 : 1;
      // A single-player demo's reader keeps an entity once seen, so only same-format pairs count.
      if (fa == to) ents += x.entities == y.entities ? 0 : 1;
      if (i == 0) {
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
                      std::to_string(a.size()) + " / " + std::to_string(b.size()), to_fixed(eye, 3),
                      to_fixed(ang, 4), std::to_string(status), std::to_string(ents),
                      std::to_string(s.bytes.size()) + " / " + std::to_string(out_bytes),
                      std::to_string(dropped), std::to_string(rewritten)});
    frames_total += static_cast<int64_t>(b.size());
    differ_total += status + ents + (eye > 0.25 ? 1 : 0);
    lab.metric("convert." + std::to_string(i) + ".frames", static_cast<double>(b.size()), "count");
    lab.metric("convert." + std::to_string(i) + ".eye-gap", eye, "u");
    lab.metric("convert." + std::to_string(i) + ".status-differs", static_cast<double>(status),
               "count");
    lab.metric("convert." + std::to_string(i) + ".bytes", static_cast<double>(out_bytes), "bytes");
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
