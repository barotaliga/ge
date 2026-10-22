// THE OPEN MAP DATA, for the lab that bakes a real face's hints (`real-hints`): a face's window
// read off OpenStreetMap's editing API (its bounding-box call, `kOsmTiles` × `kOsmTiles` boxes a
// face because the API caps a call's nodes, each kept once fetched in a cache directory keyed by
// the face's crop so a face cropped afresh never reads the old crop's data), and a
// multipolygon's whole self (its `full` call) where the box left members out. The XML is the
// API's own and is read by its fixed shape: nodes as (lat, lon), ways as their node ids and tags,
// relations as their member ways, each one's role, and their tags. No credentials are needed;
// the API's usage policy asks for a light hand, so the fetch waits between calls.
//
// The map data: © OpenStreetMap contributors, available under the Open Database Licence (ODbL
// 1.0).
//
// ge:summary Lab tooling: OpenStreetMap's data for a real face fetched, kept and read
// ge:keywords openstreetmap osm map-data fetch xml node way relation multipolygon odbl
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "mountain/mapgen/real_face_crops.hpp"

namespace ge::mountain::labs {

// The boxes a side a face's window is fetched in, and how far past the window it reaches, m.
inline constexpr int kOsmTiles = 3;
inline constexpr double kOsmPad = 150;

struct OsmWay {
  std::vector<int64_t> refs;
  std::map<std::string, std::string> tags;
  bool closed() const { return refs.size() >= 4 && refs.front() == refs.back(); }
  // The tag `k`'s value, or "" where it has none.
  const std::string& tag(const std::string& k) const;
};

struct OsmRelation {
  std::vector<int64_t> members;  // its member ways
  std::vector<std::string> roles;
  std::map<std::string, std::string> tags;
  const std::string& tag(const std::string& k) const;
};

struct OsmData {
  std::unordered_map<int64_t, std::pair<double, double>> nodes;  // (lat, lon), °
  std::map<int64_t, OsmWay> ways;  // ordered by id, so a bake reads them in one order
  std::map<int64_t, OsmRelation> relations;
};

// Read the API's XML into `into` (a way or relation already there is kept as it was).
void read_osm(const std::string& xml, OsmData& into);

class OsmCache {
 public:
  explicit OsmCache(std::string dir);
  // Fetch every box of the face's window not yet kept; false (with `error` set) on a failure.
  bool fetch_face(const mapgen::FaceCrop& face, std::string& error);
  // Whether every box of the face's window is kept.
  bool kept(const mapgen::FaceCrop& face) const;
  // Every node, way and relation of the face's window (`kept` first).
  OsmData read_face(const mapgen::FaceCrop& face) const;
  // The relation `id`'s whole self, fetched when `fetch` and not yet kept; false where it is not
  // kept and not fetched (`error` set on a failed fetch).
  bool relation_full(int64_t id, bool fetch, OsmData& out, std::string& error);

 private:
  std::string dir_;
  std::string tile_file(const mapgen::FaceCrop& face, int i, int j) const;
};

}  // namespace ge::mountain::labs
