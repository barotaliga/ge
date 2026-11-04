// A real face's streams laid on its map (brook.hpp).
//
// ge:summary The mapped streams read off a face's hints, cut into its ground as channels, iced
// ge:keywords mapped-stream lay_stream mapped_streams stream_ice channel bank gully buried
// ge:skill mountain
// ge:test tests/mountain_brook_test.cpp
#include "mountain/tarn/brook.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <unordered_map>

#include "core/heightfield.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "mountain/mapgen/real_hints.hpp"
#include "mountain/tarn/tarn.hpp"

namespace ge::mountain::tarn {

namespace {

namespace mg = mapgen;

double ground(const mg::Level& level, double x, double z) {
  return core::sample_field(level.ground, x, z);
}

bool in_lake(const mg::Level& level, double x, double z) {
  for (const mg::MapLake& l : level.lakes) {
    const bool in = !l.shore.empty() ? inside_shore(l.shore, x, z)
                                     : core::hypot(x - l.x, z - l.z) <
                                           reach_of(l, core::libm::atan2(z - l.z, x - l.x));
    if (in) return true;
  }
  return false;
}

// The mapped line read every `kStreamStep` m, its first point its first.
std::vector<mg::PlanPoint> resampled(const std::vector<mg::PlanPoint>& pts) {
  std::vector<mg::PlanPoint> out{pts.front()};
  double carry = 0;
  for (size_t i = 1; i < pts.size(); i++) {
    const double dx = pts[i].x - pts[i - 1].x, dz = pts[i].z - pts[i - 1].z;
    const double len = core::hypot(dx, dz);
    double s = kStreamStep - carry;
    while (s <= len) {
      out.push_back({pts[i - 1].x + dx * s / len, pts[i - 1].z + dz * s / len});
      s += kStreamStep;
    }
    carry = len - (s - kStreamStep);
  }
  if (carry > 0.5 * kStreamStep) out.push_back(pts.back());
  return out;
}

double smooth(double s) { return s * s * (3 - 2 * s); }

// What a cell nearest a stream's line takes from it: how far off the line, and the line's
// surface, width and depth there.
struct Near {
  double d, level, width, depth;
};

}  // namespace

std::vector<mg::MapStream> mapped_streams(const mg::Level& level) {
  std::vector<mg::MapStream> out;
  if (!level.face) return out;
  const std::optional<mg::RealHints> hints = mg::real_hints(*level.face);
  if (!hints) return out;
  for (const mg::HintStream& h : hints->streams) {
    if (h.points.size() < 2) continue;
    const bool river = h.kind == mg::HintStreamKind::river;
    const double sink = river ? kRiverSink : kBrookSink;
    const double width = river ? kRiverWidth : kBrookWidth;
    const double depth = river ? kRiverDepth : kBrookDepth;
    double surface = 1e30;
    mg::MapStream run{river, false, 0, {}};
    const auto close = [&] {
      if (static_cast<double>(run.points.size()) * kStreamStep >= kShortestRun) out.push_back(run);
      run.points.clear();
    };
    for (const mg::PlanPoint& p : resampled(h.points)) {
      const double y = ground(level, p.x, p.z);
      surface = std::min(surface, y - sink);
      if (y - sink - surface > kDeepestCut || in_lake(level, p.x, p.z)) {
        close();
        continue;
      }
      run.points.push_back({p.x, p.z, surface, width, depth});
    }
    close();
  }
  return out;
}

double stream_ice(const mg::Level& level, const mg::MapStream& stream) {
  mg::MapLake still;
  still.level = stream.points.empty() ? 0 : stream.points.front().level;
  return tarn_ice(level, still);
}

bool frozen(const mg::MapStream& stream) {
  return stream.river ? bears(stream.ice) : stream.ice > 0;
}

void lay_stream(mg::Level& level, mg::MapStream stream) {
  stream.ice = stream_ice(level, stream);
  stream.frozen = frozen(stream);
  core::Heightfield& g = level.ground;
  const double sink = stream.river ? kRiverSink : kBrookSink;
  std::unordered_map<size_t, Near> cells;
  const std::vector<mg::StreamPoint>& p = stream.points;
  for (size_t i = 0; i + 1 < p.size(); i++) {
    const mg::StreamPoint& a = p[i];
    const mg::StreamPoint& b = p[i + 1];
    const double half = 0.5 * std::max(a.width, b.width);
    const double reach = stream.frozen ? 2 * half : half + 0.5 * g.cell + kStreamReach;
    const int c0 = std::max(
        0, static_cast<int>(std::floor((std::min(a.x, b.x) - reach - g.origin_x) / g.cell)));
    const int c1 =
        std::min(g.cols - 1,
                 static_cast<int>(std::ceil((std::max(a.x, b.x) + reach - g.origin_x) / g.cell)));
    const int r0 = std::max(
        0, static_cast<int>(std::floor((std::min(a.z, b.z) - reach - g.origin_z) / g.cell)));
    const int r1 =
        std::min(g.rows - 1,
                 static_cast<int>(std::ceil((std::max(a.z, b.z) + reach - g.origin_z) / g.cell)));
    const double ex = b.x - a.x, ez = b.z - a.z;
    const double len2 = std::max(1e-9, ex * ex + ez * ez);
    for (int row = r0; row <= r1; ++row) {
      for (int col = c0; col <= c1; ++col) {
        const double x = g.origin_x + col * g.cell, z = g.origin_z + row * g.cell;
        const double t = std::clamp(((x - a.x) * ex + (z - a.z) * ez) / len2, 0.0, 1.0);
        const double d = core::hypot(x - (a.x + t * ex), z - (a.z + t * ez));
        if (d > reach) continue;
        const size_t at =
            static_cast<size_t>(row) * static_cast<size_t>(g.cols) + static_cast<size_t>(col);
        const auto it = cells.find(at);
        if (it != cells.end() && it->second.d <= d) continue;
        cells[at] = {d, a.level + t * (b.level - a.level), a.width + t * (b.width - a.width),
                     a.depth + t * (b.depth - a.depth)};
      }
    }
  }
  for (const auto& [at, n] : cells) {
    float& y = g.data[at];
    const double half = 0.5 * n.width;
    if (stream.frozen) {
      // A gully under the snow: the line's own ground dipped by a share of the channel's depth.
      const double top = n.level + sink;
      const double dip = kGully * n.depth * (1 - smooth(std::clamp(n.d / (2 * half), 0.0, 1.0)));
      y = static_cast<float>(std::min<double>(y, top - dip));
      continue;
    }
    // The bed reaches half a cell past the edge, so the ground read between the grid's nodes
    // lies under the water across the whole channel.
    const double bed = half + 0.5 * g.cell;
    if (n.d < bed) {
      // From the waterline past the edge down to its depth mid-stream.
      y = static_cast<float>(std::min<double>(y, n.level - n.depth * (1 - smooth(n.d / bed))));
      continue;
    }
    // THE BANKS: just over the water past the bed, then no steeper than `kStreamBank`.
    const double off = n.d - bed;
    const double rim = n.level + kRim;
    y = static_cast<float>(std::clamp<double>(y, rim - kStreamBank * off, rim + kStreamBank * off));
  }
  const auto near_of = [&](double x, double z) -> const Near* {
    const int col = static_cast<int>(std::lround((x - g.origin_x) / g.cell));
    const int row = static_cast<int>(std::lround((z - g.origin_z) / g.cell));
    if (col < 0 || row < 0 || col >= g.cols || row >= g.rows) return nullptr;
    const auto it = cells.find(static_cast<size_t>(row) * static_cast<size_t>(g.cols) +
                               static_cast<size_t>(col));
    return it == cells.end() ? nullptr : &it->second;
  };
  std::erase_if(level.trees, [&](const mg::TreeDef& tree) {
    const Near* n = near_of(tree.x, tree.z);
    return n && n->d < 0.5 * n->width + kFell;
  });
  for (mg::TreeDef& tree : level.trees) {
    if (near_of(tree.x, tree.z)) tree.y = ground(level, tree.x, tree.z);
  }
  level.streams.push_back(std::move(stream));
}

void lay_mapped_streams(mg::Level& level) {
  for (mg::MapStream& s : mapped_streams(level)) lay_stream(level, std::move(s));
}

}  // namespace ge::mountain::tarn
