// LAB `real-faces`: THE REAL FACES (R25), read and baked. Reading (the default) decodes every face
// named from the tree (real_faces/) and measures it as a skier would: its vertical (the ridge
// row over the valley floor across the playable face) and the share of the playable face that
// falls down the map; it draws the faces as a sheet of shaded relief, and each face named alone
// at full size with its mean profile down the map. A claim about "the drop on alpine-7" is a
// claim about a row here.
//
// BAKING (`--bake`) is the offline half that writes the tree's face files: each face's window is
// read off the 30 m elevation model (dem_tiles.hpp; `--fetch` downloads the tiles it reaches
// into `--dem`, once, from the model's public bucket, which needs no credentials), sampled every
// 32 m over the map's square, smoothed with a Gaussian of 1.2 cells (the forest canopy and the
// roofs a surface model carries out, the spurs and gullies kept), put on 0.5 m steps over its
// lowest point and encoded (real_face.hpp). The table says how far each bake lies from the
// face already in the tree; `--write` writes the faces baked and the index
// (plugins/mountain/mapgen/real_faces/), run from the repository's root. A face's file is
// GENERATED: never edited by hand.
//
// ge:summary Lab: the real faces' heights decoded and measured, drawn, and baked off the model
// ge:keywords lab real-face bake elevation-model relief drop falls profile r25 picture
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/num.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/labs/dem_tiles.hpp"
#include "mountain/mapgen/real_face.hpp"
#include "mountain/mapgen/regions.hpp"

namespace {

namespace mg = ge::mountain::mapgen;
namespace fs = std::filesystem;
using ge::lab::Rgba;

constexpr int kN = mg::kFaceN;
constexpr double kCell = mg::kFaceCell;
constexpr double kSmooth = 1.2;
constexpr const char* kTree = "plugins/mountain/mapgen/real_faces";

std::string f0(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.0f", v);
  return b;
}
std::string f2(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.2f", v);
  return b;
}

// The faces asked for: "all", or ids comma-separated.
std::vector<const mg::FaceCrop*> faces_of(ge::lab::Lab& lab) {
  std::vector<const mg::FaceCrop*> out;
  const std::string& asked = lab.arg("faces");
  if (asked == "all") {
    for (const mg::FaceCrop& c : mg::kFaceCrops) out.push_back(&c);
    return out;
  }
  std::stringstream in(asked);
  std::string id;
  while (std::getline(in, id, ',')) {
    if (const mg::FaceCrop* c = mg::face_crop(id)) {
      out.push_back(c);
    } else {
      lab.fail("--faces names " + id + ", which no face is",
               "next: name faces by their ids in plugins/mountain/mapgen/real_face_crops.hpp.");
    }
  }
  return out;
}

// The face's height at sample (r, c), m.
double h_at(const std::vector<float>& g, int r, int c) {
  return g[size_t(r) * size_t(kN) + size_t(c)];
}

// What a face holds as a skier sees it: the ridge band over the valley floor across the playable
// face, m, and the share of the playable face falling down the map over four cells.
struct Measure {
  double drop = 0;
  double falls = 0;
};

Measure measure(const std::vector<float>& g) {
  const auto cell = [](double m) { return int(ge::core::num::round(m / kCell)); };
  const auto band = [&](double z0, double z1) {
    double s = 0;
    int n = 0;
    for (int r = cell(z0); r <= cell(z1); r++) {
      for (int c = cell(700); c <= cell(3300); c++) {
        s += h_at(g, std::min(r, kN - 1), c);
        n++;
      }
    }
    return s / n;
  };
  Measure m;
  m.drop = band(250, 420) - band(3450, mg::kFaceSize);
  int falls = 0;
  int all = 0;
  for (int r = cell(400); r < cell(3500) - 4; r++) {
    for (int c = cell(700); c <= cell(3300); c++) {
      all++;
      if (h_at(g, r + 4, c) < h_at(g, r, c)) falls++;
    }
  }
  m.falls = double(falls) / all;
  return m;
}

// A separable Gaussian of `sigma` cells over the grid, the edges held.
std::vector<double> smooth(const std::vector<double>& g, double sigma) {
  const int k = int(std::ceil(sigma * 3));
  std::vector<double> w(size_t(2 * k + 1));
  double sum = 0;
  for (int i = -k; i <= k; i++) {
    const double t = i / sigma;
    const int j = i + k;
    sum += w[size_t(j)] = ge::core::libm::exp(-0.5 * t * t);
  }
  for (double& v : w) v /= sum;
  const auto pass = [&](const std::vector<double>& src, int dr, int dc) {
    std::vector<double> out(src.size());
    for (int r = 0; r < kN; r++) {
      for (int c = 0; c < kN; c++) {
        double v = 0;
        for (int i = -k; i <= k; i++) {
          const int rr = std::clamp(r + i * dr, 0, kN - 1);
          const int cc = std::clamp(c + i * dc, 0, kN - 1);
          const int j = i + k;
          v += src[size_t(rr) * size_t(kN) + size_t(cc)] * w[size_t(j)];
        }
        out[size_t(r) * size_t(kN) + size_t(c)] = v;
      }
    }
    return out;
  };
  return pass(pass(g, 0, 1), 1, 0);
}

// A face baked: its floor, its steps over it and the grid as kept.
struct Baked {
  double floor = 0;
  std::vector<int> steps;
  std::string data;
};

// The tiles a face's window reaches into.
std::set<std::string> tiles_of(const mg::FaceCrop& f) {
  std::set<std::string> out;
  for (const double x : {0.0, mg::kFaceSize / 2, mg::kFaceSize}) {
    for (const double z : {0.0, mg::kFaceSize / 2, mg::kFaceSize}) {
      const auto [lat, lon] = mg::globe_at(f, x, z);
      out.insert(ge::mountain::labs::Dem::tile_name(lat, lon));
    }
  }
  return out;
}

bool bake(ge::mountain::labs::Dem& dem, const mg::FaceCrop& f, Baked& out, std::string& error) {
  std::vector<double> g(size_t(kN) * size_t(kN));
  for (int r = 0; r < kN; r++) {
    for (int c = 0; c < kN; c++) {
      const auto [lat, lon] = mg::globe_at(f, c * kCell, r * kCell);
      g[size_t(r) * size_t(kN) + size_t(c)] = dem.height_at(lat, lon, error);
      if (!error.empty()) return false;
    }
  }
  g = smooth(g, kSmooth);
  out.floor = std::floor(*std::min_element(g.begin(), g.end()));
  out.steps.resize(g.size());
  for (size_t i = 0; i < g.size(); i++) {
    out.steps[i] = int(ge::core::num::round((g[i] - out.floor) / mg::kFaceStep));
  }
  out.data = mg::encode_face_grid(out.steps);
  return true;
}

// The C++ name of a face's data: "alpine-12" -> "kAlpine12".
std::string symbol_of(const std::string& id) {
  std::string s = "k";
  bool up = true;
  for (const char ch : id) {
    if (ch == '-') {
      up = true;
      continue;
    }
    s += up ? char(std::toupper(static_cast<unsigned char>(ch))) : ch;
    up = false;
  }
  return s;
}

std::string file_of(const std::string& id) {
  std::string s = id;
  std::replace(s.begin(), s.end(), '-', '_');
  return "face_" + s + ".cpp";
}

constexpr const char* kNotice =
    "// The heights are off the Copernicus DEM GLO-30, © DLR e.V. 2010–2014 and © Airbus Defence\n"
    "// and Space GmbH 2014–2018, provided under COPERNICUS by the European Union and ESA; free "
    "to\n"
    "// use and redistribute with this notice.\n";

// The face's file, as clang-format leaves it.
std::string face_file(const mg::FaceCrop& f, const Baked& b) {
  std::string s =
      "// GENERATED by the `real-faces` lab (`make lab LAB=real-faces ARGS=\"--bake --write\"`):\n"
      "// never edit by hand. The heights of the real face " +
      std::string(f.id) + " (real_face.hpp).\n";
  s += kNotice;
  s += "//\n// ge:summary R25 the real face " + std::string(f.id) + "'s heights (generated)\n";
  s += "// ge:keywords real-face generated heights\n// ge:skill mountain\n";
  s += "// ge:test tests/mountain_real_face_test.cpp\n";
  s += "#include \"mountain/mapgen/real_face.hpp\"\n\nnamespace ge::mountain::mapgen::real_faces "
       "{\n\n";
  const std::string sym = symbol_of(f.id);
  s += "extern const FaceData " + sym + ";\n";
  s += "const FaceData " + sym + "{\n    \"" + f.id + "\", " + f0(b.floor) + ",\n";
  constexpr size_t kLine = 90;
  for (size_t i = 0; i < b.data.size(); i += kLine) {
    s += "    \"" + b.data.substr(i, kLine) + "\"";
    s += i + kLine >= b.data.size() ? "};\n" : "\n";
  }
  s += "\n}  // namespace ge::mountain::mapgen::real_faces\n";
  return s;
}

std::string index_file() {
  std::string s =
      "// GENERATED by the `real-faces` lab (`make lab LAB=real-faces ARGS=\"--bake --write\"`):\n"
      "// never edit by hand. Every real face baked, in the crops' order (real_face.hpp).\n";
  s += kNotice;
  s += "//\n// ge:summary R25 the real faces baked, listed (generated)\n";
  s += "// ge:keywords real-face generated baked-faces index\n// ge:skill mountain\n";
  s += "// ge:test tests/mountain_real_face_test.cpp\n";
  s += "#include <array>\n\n#include \"mountain/mapgen/real_face.hpp\"\n\n";
  s += "namespace ge::mountain::mapgen {\n\nnamespace real_faces {\n";
  std::vector<std::string> syms;
  for (const mg::FaceCrop& f : mg::kFaceCrops) {
    if (!fs::exists(fs::path(kTree) / file_of(f.id))) continue;
    syms.push_back(symbol_of(f.id));
    s += "extern const FaceData " + syms.back() + ";\n";
  }
  s += "}  // namespace real_faces\n\nstd::span<const FaceData* const> baked_faces() {\n";
  s += "  static constexpr std::array<const FaceData*, " + std::to_string(syms.size()) +
       "> kAll{{\n      // clang-format off\n";
  for (const std::string& sym : syms) s += "      &real_faces::" + sym + ",\n";
  s += "      // clang-format on\n  }};\n  return kAll;\n}\n\n}  // namespace "
       "ge::mountain::mapgen\n";
  return s;
}

// The face shaded: lit from the north-west, coloured by height between its lowest and highest.
ge::lab::Picture draw_face(const mg::RealFace& face, int px) {
  auto pic = ge::lab::create_picture(px, px, {0, 0, 0});
  const auto [lo_it, hi_it] = std::minmax_element(face.heights.begin(), face.heights.end());
  const double lo = *lo_it;
  const double span = std::max(1.0, double(*hi_it) - lo);
  const double k = mg::kFaceSize / px;
  for (int py = 0; py < px; py++) {
    for (int pxx = 0; pxx < px; pxx++) {
      const double x = (pxx + 0.5) * k;
      const double z = (py + 0.5) * k;
      const double e = std::max(2.0, k);
      const double gx =
          (mg::face_height(face, x + e, z) - mg::face_height(face, x - e, z)) / (2 * e);
      const double gz =
          (mg::face_height(face, x, z + e) - mg::face_height(face, x, z - e)) / (2 * e);
      const double nl = std::sqrt(gx * gx + 1 + gz * gz);
      const double lit = std::clamp((gx + 1.41421356 + gz) / (2 * nl), 0.0, 1.0);
      const double t = std::clamp((mg::face_height(face, x, z) - lo) / span, 0.0, 1.0);
      const double shade = 0.35 + 0.65 * lit;
      const double r = (90 + 165 * t) * shade;
      const double g = (130 + 125 * t) * shade;
      const double b = (90 + 165 * t) * shade;
      ge::lab::set_pixel(
          pic, pxx, py,
          {static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b)});
    }
  }
  return pic;
}

// The face's mean profile down the map (the mean of each row across the playable face): row
// across the page, height up it.
ge::lab::Picture draw_profile(const mg::RealFace& face, int w, int h) {
  auto pic = ge::lab::create_picture(w, h, {246, 246, 242});
  std::vector<double> mean(static_cast<size_t>(kN));
  for (int r = 0; r < kN; r++)
    mean[size_t(r)] = mg::face_mean(face, 700, 3300, r * kCell, r * kCell);
  const auto [lo, hi] = std::minmax_element(mean.begin(), mean.end());
  const auto X = [&](int r) { return 12 + double(r) / (kN - 1) * (w - 24); };
  const auto Y = [&](double y) { return 12 + (*hi - y) / std::max(1.0, *hi - *lo) * (h - 34); };
  for (int r = 1; r < kN; r++) {
    ge::lab::draw_line(pic, X(r - 1), Y(mean[size_t(r - 1)]), X(r), Y(mean[size_t(r)]),
                       {20, 20, 20}, 2);
  }
  ge::lab::draw_text(pic, 12, h - 16,
                     f0(*hi) + " M AT THE TOP OF THE MEAN, " + f0(*lo) + " M AT ITS FOOT",
                     {80, 80, 80});
  return pic;
}

void run(ge::lab::Lab& lab) {
  const auto faces = faces_of(lab);
  const int px = static_cast<int>(lab.arg_num("size"));
  const bool baking = lab.arg_bool("bake");
  ge::mountain::labs::Dem dem(lab.arg("dem"));
  if (baking && lab.arg_bool("fetch")) {
    std::set<std::string> tiles;
    for (const mg::FaceCrop* f : faces) tiles.merge(tiles_of(*f));
    for (const std::string& t : tiles) {
      std::string error;
      if (!dem.fetch(t, error)) {
        std::string what = "fetching tile ";
        what.append(t).append(": ").append(error);
        lab.fail(what,
                 "Check the network reaches the elevation model's public bucket, then run again.");
        return;
      }
    }
  }
  std::vector<std::vector<std::string>> rows;
  std::vector<const mg::FaceCrop*> written;
  for (const mg::FaceCrop* f : faces) {
    const std::string id = f->id;
    const auto kept = mg::real_face(id);
    std::vector<std::string> row{id, mg::region_name(f->region), f->range, f->area, f->part};
    if (kept) {
      const Measure m = measure(kept->heights);
      const auto [lo, hi] = std::minmax_element(kept->heights.begin(), kept->heights.end());
      lab.metric(id + ".drop", m.drop, "m");
      lab.metric(id + ".falls", m.falls, "share");
      lab.metric(id + ".floor", *lo, "m");
      lab.metric(id + ".top", *hi, "m");
      row.insert(row.end(), {f0(*lo), f0(*hi), f0(m.drop), f2(m.falls)});
    } else {
      row.insert(row.end(), {"-", "-", "-", "-"});
      if (!baking) {
        lab.fail(id + " has no heights in the tree",
                 "Bake it: make lab LAB=real-faces ARGS=\"--bake --fetch --write --faces=" + id +
                     "\" from the repository's root (skill: mountain).");
      }
    }
    if (baking) {
      Baked b;
      std::string error;
      if (!bake(dem, *f, b, error)) {
        lab.fail(std::string(id).append(": ").append(error),
                 "Fetch the tiles it reaches: add --fetch (skill: mountain).");
        continue;
      }
      // How far the bake lies from the face in the tree, in steps.
      int moved = 0;
      if (kept) {
        for (size_t i = 0; i < b.steps.size(); i++) {
          const double was = (kept->heights[i] - b.floor) / mg::kFaceStep;
          moved += std::abs(was - b.steps[i]) > 0.25 ? 1 : 0;
        }
      }
      row.insert(row.end(), {f0(b.floor), std::to_string(b.data.size()),
                             kept ? std::to_string(moved) : "new"});
      if (lab.arg_bool("write")) {
        if (!fs::exists("plugins/mountain/mapgen/real_face.hpp")) {
          lab.fail("--write must run from the repository's root",
                   "cd to the checkout's root and run the lab again.");
          return;
        }
        fs::create_directories(kTree);
        std::ofstream(fs::path(kTree) / file_of(id)) << face_file(*f, b);
        written.push_back(f);
      }
    }
    rows.push_back(std::move(row));
  }
  if (!written.empty()) {
    std::ofstream(fs::path(kTree) / "index.cpp") << index_file();
    lab.note("wrote " + std::to_string(written.size()) + " faces and the index into " + kTree +
             "; run make fmt-check and commit them");
  }
  std::vector<std::string> head{"face",    "region", "range",  "area", "part",
                                "floor m", "top m",  "drop m", "falls"};
  if (baking) head.insert(head.end(), {"baked floor m", "base64 bytes", "samples moved"});
  lab.table({"the real faces (the tree's heights; with --bake, the bake beside them)", head, rows});

  // THE SHEET: every face read, a thumbnail each with its id.
  const int thumb = std::max(48, px / 6);
  const int across = 6;
  std::vector<mg::RealFace> read;
  for (const mg::FaceCrop* f : faces) {
    if (auto face = mg::real_face(f->id)) read.push_back(std::move(*face));
  }
  if (read.empty()) return;
  const int down = (int(read.size()) + across - 1) / across;
  auto sheet =
      ge::lab::create_picture(across * (thumb + 4) + 4, down * (thumb + 16) + 4, {30, 30, 30});
  for (size_t i = 0; i < read.size(); i++) {
    const int x = 4 + int(i % across) * (thumb + 4);
    const int y = 4 + int(i / across) * (thumb + 16);
    ge::lab::blit(sheet, draw_face(read[i], thumb), x, y);
    ge::lab::draw_text(sheet, x, y + thumb + 3, read[i].crop->id, {230, 230, 230});
  }
  lab.shot({"sheet",
            "THE REAL FACES",
            "Every face named, shaded by height (dark green low, white high) and lit from the "
            "north-west, its summit ridge along the top and its valley floor along the bottom.",
            "every face falling down the page from a ridge to a floor, with spurs and gullies "
            "and no seam, step or hole",
            {{"faces", lab.arg("faces")}}},
           sheet);
  if (read.size() > 4) return;
  for (const mg::RealFace& face : read) {
    const std::string id = face.crop->id;
    std::string place = std::string(face.crop->area);
    if (std::string(face.crop->part) != face.crop->area) place.append(", ").append(face.crop->part);
    lab.shot({id,
              "REAL FACE " + id,
              std::string("The real face ")
                  .append(id)
                  .append(" (")
                  .append(place)
                  .append(") over the map's square, shaded by height and lit from the north-west."),
              "the real mountain's ridge, spurs and valley, no seam or step",
              {{"face", id}}},
             draw_face(face, px));
    lab.shot({id + "-profile",
              "REAL FACE " + id + ", DOWN THE MAP",
              "The face's mean height across its playable middle (700 to 3300 m), row by row "
              "from the ridge at the left to the valley floor at the right.",
              "a fall from the ridge to the floor, steepest on the face",
              {{"face", id}}},
             draw_profile(face, px, std::max(200, px / 3)));
  }
}

const ge::lab::LabSpec kSpec{
    "real-faces",
    ge::lab::LabKind::picture,
    "plugin:mountain",
    "plugins/mountain/mapgen",
    "mountain",
    "What does each real face hold (its vertical, how much of it falls down the map), and does a "
    "bake off the elevation model reproduce the faces in the tree?",
    {
        {"faces", "all", "the faces, comma-separated ids, or all"},
        {"size", "768", "the pictures' width, px"},
        {"bake", "", "bake the faces off the elevation model and compare them with the tree's",
         true},
        {"fetch", "", "with --bake: download the tiles not yet kept in --dem", true},
        {"write", "", "with --bake: write the faces baked and the index into the tree", true},
        {"dem", ".cache/dem", "where the elevation model's tiles are kept"},
    },
    "--faces=alpine-1,fell-1,continental-1,maritime-1 --size=192",
    run,
};
GE_LAB(kSpec);

}  // namespace
