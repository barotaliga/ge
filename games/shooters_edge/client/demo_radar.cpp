// A multiview recording's radar, defined: the demo read frame by frame, sampled four times a
// second into the radar's chunks; the track added to a packed file.
//
// ge:summary A recording's radar defined: frames sampled into roster and positions chunks
// ge:keywords demo radar sample roster positions heading yaw add_radar
// ge:skill replay shooter
// ge:test tests/shooters_edge_demo_radar_test.cpp
#include "shooters_edge/client/demo_radar.hpp"

#include <algorithm>
#include <cmath>

#include "brush/read.hpp"
#include "brush/survey.hpp"
#include "replay/radar.hpp"
#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::client {

namespace gr = ge::replay;

namespace {

// The classic yaw (degrees, anticlockwise from east) as the radar's heading (256ths of a turn,
// clockwise from north).
uint8_t heading_of(float yaw) {
  const double turns = (90.0 - static_cast<double>(yaw)) / 360.0;
  const double frac = turns - std::floor(turns);
  return static_cast<uint8_t>(std::lround(frac * 256.0) & 255);
}

int32_t whole(float v) { return static_cast<int32_t>(std::lround(static_cast<double>(v))); }

}  // namespace

namespace {

// demo_radar's walk, with every sampled origin's height kept in `zs` when it is given.
bool radar_walk(std::span<const uint8_t> recording, std::vector<gr::Chunk>& chunks, gr::Meta& meta,
                std::string& error, std::vector<float>* zs) {
  chunks.clear();
  meta.clear();
  Demo d;
  if (!open_demo(std::vector<uint8_t>(recording.begin(), recording.end()), d, error)) return false;
  if (d.state.format != DemoFormat::multiview) {
    error = "only a multiview recording has every player's place; this one is not";
    return false;
  }
  std::vector<gr::RadarPlayer> roster;
  const int64_t step_ms = 1000 / kRadarHz;
  int64_t next_ms = 0;
  for (;;) {
    const DemoRead read = next_frame(d, error);
    if (read == DemoRead::error) return false;
    if (read == DemoRead::end) break;
    const auto t_ms = static_cast<int64_t>(std::llround(d.state.time * 1000.0));
    if (t_ms < next_ms) continue;
    next_ms = t_ms - t_ms % step_ms + step_ms;
    std::vector<gr::RadarPlayer> now;
    std::vector<gr::RadarDot> dots;
    const size_t slots = std::min<size_t>(d.state.players.size(), 32);
    for (size_t s = 0; s < slots; s++) {
      if (d.state.names[s].empty()) continue;
      now.push_back({static_cast<uint8_t>(s), d.state.names[s], d.state.teams[s]});
      const DemoEntity& p = d.state.players[s];
      if (p.model == 0 && !p.present) continue;  // a spectator: no body on the map
      gr::RadarDot dot;
      dot.slot = static_cast<uint8_t>(s);
      dot.flags = p.dead ? gr::kRadarDead : 0;
      dot.x = whole(p.origin.x);
      dot.y = whole(p.origin.y);
      dot.heading = heading_of(p.angles.y);
      dots.push_back(dot);
      if (zs != nullptr && !p.dead) zs->push_back(p.origin.z);
    }
    const bool changed =
        now.size() != roster.size() ||
        !std::equal(now.begin(), now.end(), roster.begin(),
                    [](const gr::RadarPlayer& a, const gr::RadarPlayer& b) {
                      return a.slot == b.slot && a.name == b.name && a.team == b.team;
                    });
    if (changed) {
      roster = now;
      chunks.push_back({kRadarTrack, t_ms, gr::radar_roster(roster)});
    }
    if (!dots.empty()) chunks.push_back({kRadarTrack, t_ms, gr::radar_positions(dots)});
    if (meta.empty() && !d.state.map.empty()) meta.emplace_back("map", d.state.map);
  }
  error.clear();
  std::vector<std::string> teams;
  for (const gr::RadarPlayer& p : roster) {
    meta.emplace_back("player", p.name + "\t" + p.team);
    if (!p.team.empty() && std::find(teams.begin(), teams.end(), p.team) == teams.end()) {
      teams.push_back(p.team);
    }
  }
  for (const std::string& t : teams) meta.emplace_back("team", t);
  return true;
}

}  // namespace

bool demo_radar(std::span<const uint8_t> recording, std::vector<gr::Chunk>& chunks, gr::Meta& meta,
                std::string& error) {
  return radar_walk(recording, chunks, meta, error, nullptr);
}

std::vector<gr::RadarLine> map_outline(std::span<const uint8_t> bsp, double z) {
  std::vector<gr::RadarLine> lines;
  ge::brush::Map map;
  std::string why;
  ge::brush::Slice s;
  if (!ge::brush::read_bsp(bsp, &map, &why) || !ge::brush::slice_map(map, 2, z, 0, &s, &why)) {
    return lines;
  }
  const auto solid = [&](size_t row, size_t col) {
    return row < s.rows.size() && col < s.rows[row].size() && s.rows[row][col] == '#';
  };
  const double h = s.step / 2;
  const auto x_of = [&](double col) { return s.u0 + col * s.step; };
  const auto y_of = [&](double row) { return s.v0 - row * s.step; };
  const auto add = [&](double x0, double y0, double x1, double y1) {
    lines.push_back({static_cast<int32_t>(std::lround(x0)), static_cast<int32_t>(std::lround(y0)),
                     static_cast<int32_t>(std::lround(x1)), static_cast<int32_t>(std::lround(y1))});
  };
  const size_t rows = s.rows.size(), cols = rows ? s.rows[0].size() : 0;
  // Edges between a row's neighbours (upright lines), joined down each column boundary.
  for (size_t c = 0; c + 1 < cols; c++) {
    size_t run = rows;
    for (size_t r = 0; r <= rows; r++) {
      const bool edge = r < rows && solid(r, c) != solid(r, c + 1);
      if (edge && run == rows) run = r;
      if (!edge && run != rows) {
        add(x_of(static_cast<double>(c)) + h, y_of(static_cast<double>(run)) + h,
            x_of(static_cast<double>(c)) + h, y_of(static_cast<double>(r) - 1) - h);
        run = rows;
      }
    }
  }
  // Edges between a column's neighbours (level lines), joined along each row boundary.
  for (size_t r = 0; r + 1 < rows; r++) {
    size_t run = cols;
    for (size_t c = 0; c <= cols; c++) {
      const bool edge = c < cols && solid(r, c) != solid(r + 1, c);
      if (edge && run == cols) run = c;
      if (!edge && run != cols) {
        add(x_of(static_cast<double>(run)) - h, y_of(static_cast<double>(r)) - h,
            x_of(static_cast<double>(c) - 1) + h, y_of(static_cast<double>(r)) - h);
        run = cols;
      }
    }
  }
  return lines;
}

bool add_radar(std::span<const uint8_t> recording, std::span<const uint8_t> file,
               std::vector<uint8_t>& out, std::string& error, std::span<const uint8_t> bsp) {
  std::vector<gr::Chunk> chunks;
  gr::Meta meta;
  std::vector<float> z;
  if (!radar_walk(recording, chunks, meta, error, bsp.empty() ? nullptr : &z)) return false;
  if (!z.empty()) {
    std::nth_element(z.begin(), z.begin() + static_cast<std::ptrdiff_t>(z.size() / 2), z.end());
    const std::vector<gr::RadarLine> lines = map_outline(bsp, z[z.size() / 2]);
    if (!lines.empty()) {
      const int64_t t = chunks.empty() ? 0 : chunks.front().time_ms;
      chunks.insert(chunks.begin(), {kRadarTrack, t, gr::radar_outline(lines)});
    }
  }
  return gr::stream_add_track(file,
                              {kRadarTrack, gr::TrackKind::radar, gr::TrackCodec::zstd, "radar"},
                              chunks, out, error, meta);
}

}  // namespace shooters_edge::client
