// LAB `se-series`: A CLAN FIGHT'S MAPS IN ONE DEMO SERIES (docs/tv.md § A demo series,
// engine/replay/series.hpp). Recordings, one a map (by default two multiview recordings written in
// code on the fixture halls; real ones with `--files=<a.mvd,b.mvd,...>`), are merged as `secli
// demos merge` merges them: each packed as a replay file (`.ged`) and kept whole, in order, in one
// `.geds`. The series is then split again (every entry must be the very replay file merged, and
// unpack to the very recording), and played by the demo player as the window plays it (the
// recordings one after another as new levels of one run: client/demo.hpp's `demo_series_next`),
// which must read as many frames of each as the recording alone.
//
// The pictures: each entry's players from above, on its map's own floors when `--paks=` names
// packs that hold the map (the live map's plate, livemap/route.hpp), else as lines on a chart.
// Sizes, frames and the round trips are deterministic; the map pictures are drawn on the CPU from
// the packs, deterministic too.
//
// ge:summary Lab: recordings merged into a demo series, split back exactly, played in order
// ge:keywords lab se-series demo-series geds merge split clan-fight play-in-order round-trip
// ge:skill replay labs
// ge:test tests/replay_series_test.cpp tests/shooters_edge_listen_record_test.cpp
#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "replay/series.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "shooters_edge/livemap/route.hpp"
#include "vfs/pak.hpp"
#include "vfs/unpack.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
namespace lv = shooters_edge::livemap;
namespace gr = ge::replay;
using ge::core::num::to_fixed;

constexpr int kSlots = 32;

std::vector<std::string> split_list(const std::string& s) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at <= s.size() && !s.empty()) {
    const size_t comma = s.find(',', at);
    const std::string w = s.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
    if (!w.empty()) out.push_back(w);
    if (comma == std::string::npos) break;
    at = comma + 1;
  }
  return out;
}

bool read_file(const std::string& path, std::vector<uint8_t>& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  out.assign(std::istreambuf_iterator<char>(in), {});
  return true;
}

std::string base_name(const std::string& path) {
  const size_t slash = path.find_last_of("/\\");
  std::string n = slash == std::string::npos ? path : path.substr(slash + 1);
  if (const size_t dot = n.find('.'); dot != std::string::npos && dot > 0) n.resize(dot);
  return n;
}

// The packs a map is looked for in: each read whole, in the order given.
struct Packs {
  std::vector<std::vector<uint8_t>> bytes;
  std::vector<ge::vfs::Pak> paks;
  std::span<const uint8_t> find(const std::string& path) const {
    for (size_t i = paks.size(); i-- > 0;)  // the last pack over the first, as a game reads them
      if (const ge::vfs::PakEntry* e = ge::vfs::pak_find(paks[i], path))
        return ge::vfs::pak_file(bytes[i], *e);
    return {};
  }
};

// One recording read to its end: frames with a map, and each player's path every tenth frame.
struct Read {
  double seconds = 0;  // the paths kept for this long from the recording's first frame (0: all)
  double first = -1;   // the first frame's time
  int64_t frames = 0;
  std::string map;
  std::array<std::vector<std::array<float, 3>>, kSlots> paths;
  int players = 0;
};

void take_frame(const cl::DemoState& s, Read& r) {
  if (s.map.empty()) return;
  r.map = s.map;
  if (r.first < 0) r.first = s.time;
  if (r.frames++ % 10 != 0 || (r.seconds > 0 && s.time - r.first > r.seconds)) return;
  for (int k = 0; k < kSlots; k++) {
    const cl::DemoEntity& p = s.players[static_cast<size_t>(k)];
    if (p.present) r.paths[static_cast<size_t>(k)].push_back({p.origin.x, p.origin.y, p.origin.z});
  }
}

void finish(Read& r) {
  r.players = 0;
  for (const auto& p : r.paths) r.players += p.empty() ? 0 : 1;
}

bool read_alone(std::vector<uint8_t> bytes, Read& r, std::string& error) {
  cl::Demo d;
  if (!cl::open_demo(std::move(bytes), d, error)) return false;
  cl::DemoRead k;
  while ((k = cl::next_frame(d, error)) == cl::DemoRead::frame) take_frame(d.state, r);
  finish(r);
  return k != cl::DemoRead::error;
}

gl::Picture paths_chart(const Read& r, const std::string& title) {
  gl::LineChart c;
  c.spikes = false;
  c.frame = {title,       "every player from above, a point every tenth frame (no map packs given)",
             "x (units)", "y (units)",
             960,         720};
  for (int k = 0; k < kSlots; k++) {
    const auto& p = r.paths[static_cast<size_t>(k)];
    if (p.empty()) continue;
    gl::Trace t{"slot " + std::to_string(k), {}, {}};
    for (const auto& v : p) {
      t.x.push_back(v[0]);
      t.y.push_back(v[1]);
    }
    c.traces.push_back(std::move(t));
  }
  return gl::to_picture(gl::line_chart(c).plot);
}

// The players on the map's own floors, when the packs hold it (`ok` false: they do not).
gl::Picture paths_on_map(const Packs& packs, const Read& r, bool& ok) {
  ok = false;
  const std::span<const uint8_t> pal = packs.find("gfx/palette.lmp");
  const std::span<const uint8_t> bsp = packs.find("maps/" + r.map + ".bsp");
  if (pal.empty() || bsp.empty()) return {};
  lv::Plate plate;
  std::string error;
  if (!lv::build_plate(bsp, r.map, pal, 1400, &plate, &error)) return {};
  lv::RouteMarks marks;
  bool first = true;
  for (const auto& p : r.paths) {
    if (p.empty()) continue;
    if (first)
      marks.track = p;
    else
      marks.party.push_back(p);
    first = false;
  }
  const ge::ui::Image img = lv::draw_route(plate, marks);
  gl::Picture out;
  out.width = img.width;
  out.height = img.height;
  out.rgba = img.rgba;
  ok = true;
  return out;
}

void run(gl::Lab& lab) {
  const std::vector<std::string> files = split_list(lab.arg("files"));
  std::vector<std::string> names;
  std::vector<std::vector<uint8_t>> plain;  // each recording as it is (unpacked)
  if (files.empty()) {
    names = {"halls-round1", "halls-round2"};
    plain.push_back(cl::write_multiview_demo(cl::loop_script("halls", {256, 192, 24}, 120, 600)));
    plain.push_back(cl::write_multiview_demo(cl::loop_script("halls", {128, 320, 24}, 90, 480)));
  } else {
    for (const std::string& f : files) {
      std::vector<uint8_t> b;
      if (!read_file(f, b)) {
        lab.fail("se-series: " + f + " does not open", "next: pass --files=<recordings' paths>");
        return;
      }
      if (ge::vfs::wrapping_of(b) != ge::vfs::Wrapping::none) {
        ge::vfs::Unpacked u;
        std::string error;
        if (!ge::vfs::unpack_one(std::move(b), u, error)) {
          lab.fail(std::string("se-series: ").append(f).append(" does not unpack: ").append(error),
                   "next: pick another file");
          return;
        }
        b = std::move(u.bytes);
      }
      names.push_back(base_name(f));
      plain.push_back(std::move(b));
    }
  }
  Packs packs;
  for (const std::string& p : split_list(lab.arg("paks"))) {
    std::vector<uint8_t> b;
    ge::vfs::Pak pak;
    std::string why;
    if (!read_file(p, b) || !ge::vfs::read_pak(b, &pak, &why)) {
      lab.fail(std::string("se-series: the pack ").append(p).append(" does not read ").append(why),
               "next: pass --paks=<pak0.pak,pak1.pak> of the game");
      return;
    }
    packs.bytes.push_back(std::move(b));
    packs.paks.push_back(std::move(pak));
  }
  std::string shown;  // the files by name alone, so the caption stays a line
  for (const std::string& n : names) shown += (shown.empty() ? "" : ", ") + n;
  const std::vector<std::pair<std::string, std::string>> in = {
      {"files", files.empty() ? "(two written in code)" : shown},
      {"paks", lab.arg("paks").empty() ? "(none)" : "the game's packs"},
      {"seconds", lab.arg("seconds")}};

  // Merged as `secli demos merge` merges: each packed as a replay file, then kept in order.
  std::vector<std::vector<uint8_t>> geds(plain.size());
  std::vector<gr::SeriesPart> parts;
  for (size_t i = 0; i < plain.size(); i++) {
    std::string error;
    if (!cl::pack_demo(plain[i], geds[i], error)) {
      lab.fail("se-series: " + names[i] + " does not pack: " + error,
               "next: read client/demo_v2.cpp; skill: replay");
      return;
    }
    parts.push_back({names[i], geds[i]});
  }
  std::vector<uint8_t> series;
  std::string error;
  if (!gr::series_merge(parts,
                        {{std::string(gr::kSeriesKind), "matches"},
                         {std::string(gr::kSeriesTitle), lab.arg("title")}},
                        series, error)) {
    lab.fail("se-series: the merge failed: " + error, "next: read engine/replay/series.cpp");
    return;
  }
  // Split: every entry the replay file merged, unpacking to the recording.
  gr::SeriesView v;
  if (!gr::read_series(series, v, error) || v.entries.size() != plain.size() || !v.cut.empty()) {
    lab.fail("se-series: the series does not read back whole: " + error + v.cut,
             "next: tests/replay_series_test.cpp");
    return;
  }
  int exact = 0;
  std::vector<bool> back(plain.size());
  for (size_t i = 0; i < plain.size(); i++) {
    const std::vector<uint8_t> ged(v.entries[i].bytes.begin(), v.entries[i].bytes.end());
    std::vector<uint8_t> un;
    std::string cut;
    back[i] = ged == geds[i] && cl::unpack_demo(ged, un, cut) && un == plain[i];
    exact += back[i] ? 1 : 0;
  }
  // Each alone, and the series played as the window plays it.
  std::vector<Read> alone(plain.size()), played(plain.size());
  for (Read& r : played) r.seconds = lab.arg_num("seconds");
  for (size_t i = 0; i < plain.size(); i++)
    if (!read_alone(plain[i], alone[i], error)) {
      lab.fail("se-series: " + names[i] + " does not play: " + error, "next: pick another file");
      return;
    }
  cl::Timedemo t;
  if (!cl::open_demo(series, t.demo, error)) {
    lab.fail("se-series: the series does not open in the demo player: " + error,
             "next: read client/demo.cpp's open_demo");
    return;
  }
  t.start = t.demo;
  t.active = true;
  int levels = 0;
  for (;;) {
    const cl::TimedemoStep st = cl::timedemo_step(t, error);
    if (st == cl::TimedemoStep::error) {
      lab.fail("se-series: the series stopped in entry " + std::to_string(t.demo.series_at + 1) +
                   ": " + error,
               "next: read client/timedemo.cpp");
      return;
    }
    if (st == cl::TimedemoStep::end) break;
    if (st == cl::TimedemoStep::new_level) levels++;
    take_frame(t.demo.state, played[std::min(t.demo.series_at, played.size() - 1)]);
  }
  int same_frames = 0;
  gl::LabTable table{"the series, entry by entry",
                     {"entry", "name", "map", "players", "frames alone", "frames in the series",
                      "recording KiB", ".ged KiB", "split back exactly"},
                     {}};
  size_t plain_total = 0, ged_total = 0;
  for (size_t i = 0; i < plain.size(); i++) {
    finish(played[i]);
    // The series' first frame of an entry is read as it begins; the rest frame for frame.
    const bool same = played[i].frames == alone[i].frames;
    same_frames += same ? 1 : 0;
    plain_total += plain[i].size();
    ged_total += geds[i].size();
    table.rows.push_back({std::to_string(i + 1), v.entries[i].name, alone[i].map,
                          std::to_string(alone[i].players), std::to_string(alone[i].frames),
                          std::to_string(played[i].frames),
                          std::to_string((plain[i].size() + 512) / 1024),
                          std::to_string((geds[i].size() + 512) / 1024), back[i] ? "yes" : "NO"});
  }
  lab.table(table);
  lab.metric("series.entries", static_cast<double>(v.entries.size()), "count");
  lab.metric("series.bytes", static_cast<double>(series.size()), "bytes");
  lab.metric("series.overhead", static_cast<double>(series.size() - ged_total), "bytes");
  lab.metric("series.recordings-bytes", static_cast<double>(plain_total), "bytes");
  lab.metric("series.split-exact", exact, "count");
  lab.metric("series.levels-played", levels, "count");
  lab.metric("series.entries-same-frames", same_frames, "count");
  if (exact != static_cast<int>(plain.size()))
    lab.fail("se-series: " + std::to_string(plain.size() - exact) +
                 " entries did not split back to the bytes merged",
             "next: tests/replay_series_test.cpp; skill: replay");
  if (levels != static_cast<int>(plain.size()) || same_frames != static_cast<int>(plain.size()))
    lab.fail("se-series: the series played " + std::to_string(levels) + " levels of " +
                 std::to_string(plain.size()) + ", " + std::to_string(same_frames) +
                 " with every frame",
             "next: read client/timedemo.cpp's timedemo_step at a recording's end");
  if (files.empty()) lab.attach("series.geds", series);

  for (size_t i = 0; i < played.size(); i++) {
    const std::string id = "entry-" + std::to_string(i + 1);
    if (!lab.wants(id)) continue;
    const std::string title = "ENTRY " + std::to_string(i + 1) + " OF " +
                              std::to_string(played.size()) + ": " + v.entries[i].name + " ON " +
                              played[i].map;
    bool on_map = false;
    const gl::Picture pic = paths_on_map(packs, played[i], on_map);
    lab.shot({id, title,
              "The series' entry " + std::to_string(i + 1) + " as the demo player read it, " +
                  std::to_string(played[i].frames) + " frames: every player's path in its first " +
                  lab.arg("seconds") + " s" +
                  (on_map ? " on the map's own floors from above (the first player red, the "
                            "rest in their own colours)"
                          : " from above"),
              "each player's line walks the map's rooms; the next entry is the next map", in},
             on_map ? pic : paths_chart(played[i], title));
  }
}

const gl::LabSpec kSpec{
    "se-series",
    gl::LabKind::sim,
    "game:shooters_edge",
    "engine/replay/series.hpp (the demo series), client/demo.hpp's demo_series_next and "
    "client/timedemo.cpp: a series played in order",
    "replay",
    "Do a clan fight's recordings merge into one demo series, split back byte for byte and play "
    "one after another as the window plays them?",
    {
        {"files", "",
         "recordings' paths, comma-separated, in play order (plain, packed or archived); empty: "
         "two written in code"},
        {"paks", "", "the game's packs, comma-separated, to draw each map's floors under it"},
        {"title", "fs vs hf", "the series' title"},
        {"seconds", "60", "each entry's paths drawn for this long from its start (0: all of it)"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
