// The definitions behind osm_data.hpp: the boxes, the cache and the XML read by its fixed shape.
//
// ge:summary Lab tooling: the map data's boxes, their cache and the API's XML read
// ge:keywords openstreetmap osm xml bounding-box cache fnv
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#include "mountain/labs/osm_data.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <thread>

#include "mountain/labs/fetch.hpp"
#include "mountain/mapgen/real_face.hpp"

namespace ge::mountain::labs {

namespace fs = std::filesystem;
namespace mg = ge::mountain::mapgen;

constexpr const char* kApi = "https://api.openstreetmap.org/api/0.6/";
// The wait between two calls, ms: the API's usage policy asks for a light hand.
constexpr int kPolite = 1500;

namespace {

const std::string kNone;

const std::string& tag_in(const std::map<std::string, std::string>& tags, const std::string& k) {
  const auto it = tags.find(k);
  return it == tags.end() ? kNone : it->second;
}

// XML's five entities undone.
std::string unescape(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] != '&') {
      out += s[i];
      continue;
    }
    const size_t end = s.find(';', i);
    if (end == std::string_view::npos) {
      out += s[i];
      continue;
    }
    const std::string_view e = s.substr(i + 1, end - i - 1);
    if (e == "amp")
      out += '&';
    else if (e == "quot")
      out += '"';
    else if (e == "lt")
      out += '<';
    else if (e == "gt")
      out += '>';
    else if (e == "apos")
      out += '\'';
    else
      out.append(s.substr(i, end - i + 1));
    i = end;
  }
  return out;
}

// The attribute `name`'s value in the element `el` (its text between `<` and `>`), or "".
std::string_view attr(std::string_view el, std::string_view name) {
  size_t at = 0;
  while ((at = el.find(name, at)) != std::string_view::npos) {
    const bool start = at > 0 && el[at - 1] == ' ';
    const size_t eq = at + name.size();
    if (start && eq + 1 < el.size() && el[eq] == '=' && el[eq + 1] == '"') {
      const size_t end = el.find('"', eq + 2);
      if (end == std::string_view::npos) return {};
      return el.substr(eq + 2, end - eq - 2);
    }
    at = eq;
  }
  return {};
}

int64_t to_id(std::string_view s) {
  int64_t v = 0;
  bool neg = false;
  for (const char c : s) {
    if (c == '-')
      neg = true;
    else if (c >= '0' && c <= '9')
      v = v * 10 + (c - '0');
  }
  return neg ? -v : v;
}

// FNV-1a over the crop's numbers, as text: a face cropped afresh reads boxes fetched for the new
// crop, never the old.
std::string crop_key(const mg::FaceCrop& f) {
  char b[160];
  std::snprintf(b, sizeof b, "%.6f,%.6f,%.3f,%.3f,%.3f", f.lat, f.lon, f.east, f.north, f.bearing);
  uint32_t h = 0x811c9dc5u;
  for (const char* c = b; *c; c++) h = (h ^ uint8_t(*c)) * 0x01000193u;
  char out[12];
  std::snprintf(out, sizeof out, "%08x", h);
  return out;
}

// The face's padded window on the globe: south, west, north, east, °.
std::array<double, 4> box_of(const mg::FaceCrop& f) {
  double s = 90, w = 180, n = -90, e = -180;
  for (const double x : {-kOsmPad, mg::kFaceSize + kOsmPad}) {
    for (const double z : {-kOsmPad, mg::kFaceSize + kOsmPad}) {
      const auto [lat, lon] = mg::globe_at(f, x, z);
      s = std::min(s, lat);
      n = std::max(n, lat);
      w = std::min(w, lon);
      e = std::max(e, lon);
    }
  }
  return {s, w, n, e};
}

std::string slurp(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

}  // namespace

const std::string& OsmWay::tag(const std::string& k) const { return tag_in(tags, k); }
const std::string& OsmRelation::tag(const std::string& k) const { return tag_in(tags, k); }

void read_osm(const std::string& xml, OsmData& into) {
  const std::string_view s(xml);
  enum class In : uint8_t { none, way, relation } in = In::none;
  int64_t id = 0;
  OsmWay way;
  OsmRelation rel;
  size_t at = 0;
  while ((at = s.find('<', at)) != std::string_view::npos) {
    const size_t end = s.find('>', at);
    if (end == std::string_view::npos) break;
    const std::string_view el = s.substr(at + 1, end - at - 1);
    at = end + 1;
    const auto is = [&](std::string_view name) {
      return el.substr(0, name.size()) == name &&
             (el.size() == name.size() || el[name.size()] == ' ' || el[name.size()] == '/');
    };
    if (is("node")) {
      const std::string_view lat = attr(el, "lat");
      const std::string_view lon = attr(el, "lon");
      if (!lat.empty() && !lon.empty()) {
        into.nodes[to_id(attr(el, "id"))] = {std::stod(std::string(lat)),
                                             std::stod(std::string(lon))};
      }
    } else if (is("way")) {
      in = In::way;
      id = to_id(attr(el, "id"));
      way = {};
      if (el.back() == '/') {
        into.ways.try_emplace(id, std::move(way));
        in = In::none;
      }
    } else if (is("relation")) {
      in = In::relation;
      id = to_id(attr(el, "id"));
      rel = {};
      if (el.back() == '/') {
        into.relations.try_emplace(id, std::move(rel));
        in = In::none;
      }
    } else if (is("nd") && in == In::way) {
      way.refs.push_back(to_id(attr(el, "ref")));
    } else if (is("member") && in == In::relation) {
      if (attr(el, "type") == "way") {
        rel.members.push_back(to_id(attr(el, "ref")));
        rel.roles.push_back(unescape(attr(el, "role")));
      }
    } else if (is("tag")) {
      std::string k = unescape(attr(el, "k"));
      std::string v = unescape(attr(el, "v"));
      if (in == In::way) {
        way.tags[std::move(k)] = std::move(v);
      } else if (in == In::relation) {
        rel.tags[std::move(k)] = std::move(v);
      }
    } else if (el == "/way") {
      into.ways.try_emplace(id, std::move(way));
      in = In::none;
    } else if (el == "/relation") {
      into.relations.try_emplace(id, std::move(rel));
      in = In::none;
    }
  }
}

OsmCache::OsmCache(std::string dir) : dir_(std::move(dir)) {}

std::string OsmCache::tile_file(const mg::FaceCrop& face, int i, int j) const {
  return (fs::path(dir_) / (std::string(face.id) + "-" + crop_key(face) + "-" + std::to_string(i) +
                            std::to_string(j) + ".osm"))
      .string();
}

bool OsmCache::kept(const mg::FaceCrop& face) const {
  for (int i = 0; i < kOsmTiles; i++) {
    for (int j = 0; j < kOsmTiles; j++) {
      if (!fs::exists(tile_file(face, i, j))) return false;
    }
  }
  return true;
}

bool OsmCache::fetch_face(const mg::FaceCrop& face, std::string& error) {
  const auto [s, w, n, e] = box_of(face);
  for (int i = 0; i < kOsmTiles; i++) {
    for (int j = 0; j < kOsmTiles; j++) {
      const std::string file = tile_file(face, i, j);
      if (fs::exists(file)) continue;
      char bbox[128];
      std::snprintf(bbox, sizeof bbox, "%.5f,%.5f,%.5f,%.5f", w + (e - w) * j / kOsmTiles,
                    s + (n - s) * i / kOsmTiles, w + (e - w) * (j + 1) / kOsmTiles,
                    s + (n - s) * (i + 1) / kOsmTiles);
      const std::string url = std::string(kApi) + "map?bbox=" + bbox;
      const std::string http = fetch_to(url, file);
      std::this_thread::sleep_for(std::chrono::milliseconds(kPolite));
      if (http != "200") {
        error = url + ": HTTP " + (http.empty() ? std::string("none (is curl installed?)") : http);
        return false;
      }
    }
  }
  return true;
}

OsmData OsmCache::read_face(const mg::FaceCrop& face) const {
  OsmData d;
  for (int i = 0; i < kOsmTiles; i++) {
    for (int j = 0; j < kOsmTiles; j++) read_osm(slurp(tile_file(face, i, j)), d);
  }
  return d;
}

bool OsmCache::relation_full(int64_t id, bool fetch, OsmData& out, std::string& error) {
  const fs::path file = fs::path(dir_) / ("relation-" + std::to_string(id) + ".osm");
  if (!fs::exists(file)) {
    if (!fetch) return false;
    const std::string url = std::string(kApi) + "relation/" + std::to_string(id) + "/full";
    const std::string http = fetch_to(url, file.string());
    std::this_thread::sleep_for(std::chrono::milliseconds(kPolite));
    if (http != "200") {
      error = url + ": HTTP " + (http.empty() ? std::string("none (is curl installed?)") : http);
      return false;
    }
  }
  read_osm(slurp(file), out);
  return true;
}

}  // namespace ge::mountain::labs
