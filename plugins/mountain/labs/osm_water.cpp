// The definitions behind osm_water.hpp.
//
// ge:summary Lab tooling: a real face's lakes and streams read off the open map data
// ge:keywords osm-water lakes-of streams-of
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#include "mountain/labs/osm_water.hpp"

#include <algorithm>
#include <cmath>

#include "mountain/labs/osm_shapes.hpp"

namespace ge::mountain::labs {

bool is_lake(const std::map<std::string, std::string>& tags) {
  const auto at = [&](const char* k) {
    const auto it = tags.find(k);
    return it == tags.end() ? std::string() : it->second;
  };
  const std::string water = at("water");
  if (water == "river" || water == "canal" || water == "stream" || water == "ditch" ||
      water == "drain" || water == "wastewater" || water == "lock" || water == "fish_pass")
    return false;
  return at("natural") == "water" || at("landuse") == "reservoir";
}

std::vector<mapgen::HintLake> lakes_of(const mapgen::FaceCrop& f, const OsmData& d, OsmCache& osm,
                                       bool fetch, int& missing, std::string& error) {
  struct Found {
    Ring shore;
    double area;
  };
  std::vector<Found> found;
  const auto keep = [&](const OsmData& src, const std::vector<int64_t>& refs) {
    Ring ring;
    for (size_t i = 1; i < refs.size(); i++) {
      const auto it = src.nodes.find(refs[i]);
      if (it == src.nodes.end()) return;  // a way cut short: no shore to trust
      const auto [x, z] = mapgen::map_at(f, it->second.first, it->second.second);
      ring.push_back({x, z});
    }
    ring = clip_to_map(ring);
    if (ring.size() < 3) return;
    const double area = std::abs(signed_area(ring));
    if (area < kSmallestLake) return;
    ring = thin_ring(ring, kShoreStray);
    if (ring.size() >= 3) found.push_back({std::move(ring), area});
  };
  for (const auto& [id, w] : d.ways) {
    if (is_lake(w.tags) && w.closed()) keep(d, w.refs);
  }
  for (const auto& [id, r] : d.relations) {
    if (r.tag("type") != "multipolygon" || !is_lake(r.tags)) continue;
    const auto whole = [&](const OsmData& src) {
      return std::all_of(r.members.begin(), r.members.end(),
                         [&](int64_t m) { return src.ways.count(m) > 0; });
    };
    const OsmData* src = &d;
    OsmData full;
    if (!whole(d)) {
      if (!osm.relation_full(id, fetch, full, error) || !whole(full)) {
        if (!error.empty()) return {};
        missing++;
        continue;
      }
      src = &full;
    }
    std::vector<std::vector<int64_t>> outer;
    for (size_t k = 0; k < r.members.size(); k++) {
      if (r.roles[k] == "inner") continue;
      outer.push_back(src->ways.at(r.members[k]).refs);
    }
    for (const std::vector<int64_t>& ring : join_rings(std::move(outer))) keep(*src, ring);
  }
  std::sort(found.begin(), found.end(), [](const Found& a, const Found& b) {
    if (a.area != b.area) return a.area > b.area;
    return a.shore[0].x != b.shore[0].x ? a.shore[0].x < b.shore[0].x : a.shore[0].z < b.shore[0].z;
  });
  if (found.size() > kMostLakes) found.resize(kMostLakes);
  std::vector<mapgen::HintLake> out;
  out.reserve(found.size());
  for (Found& l : found) out.push_back({std::move(l.shore)});
  return out;
}

namespace {

// Whether a way draws a brook or a river in the open, and which.
std::optional<mapgen::HintStreamKind> stream_kind(const OsmWay& w) {
  const std::string& tunnel = w.tag("tunnel");
  if (!tunnel.empty() && tunnel != "no") return std::nullopt;
  const std::string& v = w.tag("waterway");
  if (v == "stream") return mapgen::HintStreamKind::stream;
  if (v == "river") return mapgen::HintStreamKind::river;
  return std::nullopt;
}

}  // namespace

std::vector<mapgen::HintStream> streams_of(const mapgen::FaceCrop& f, const OsmData& d,
                                           const mapgen::RealFace& face) {
  struct Found {
    mapgen::HintStream stream;
    double length;
  };
  std::vector<Found> found;
  for (const auto& [id, w] : d.ways) {
    const auto kind = stream_kind(w);
    if (!kind) continue;
    std::vector<Pt> pts = longest_on_map(points_of(f, d, w));
    if (pts.size() < 2) continue;
    const double length = length_of(pts);
    if (length < kShortestStream) continue;
    if (mapgen::face_height(face, pts.back().x, pts.back().z) >
        mapgen::face_height(face, pts.front().x, pts.front().z))
      std::reverse(pts.begin(), pts.end());
    found.push_back({{*kind, thin(pts, kStreamStray)}, length});
  }
  std::sort(found.begin(), found.end(), [](const Found& a, const Found& b) {
    if (a.length != b.length) return a.length > b.length;
    const Pt pa = a.stream.points[0];
    const Pt pb = b.stream.points[0];
    return pa.x != pb.x ? pa.x < pb.x : pa.z < pb.z;
  });
  if (found.size() > kMostStreams) found.resize(kMostStreams);
  std::vector<mapgen::HintStream> out;
  out.reserve(found.size());
  for (Found& s : found) out.push_back(std::move(s.stream));
  return out;
}

}  // namespace ge::mountain::labs
