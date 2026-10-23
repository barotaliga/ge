// Reference sheets: the reference file's rows parsed, the photographs cropped and set beside the
// model posed and turned to match.
//
// ge:summary Reference sheets: parsing the rows, cropping the photographs, laying out the sheet
// ge:keywords lab reference photograph compare crop sheet parse_refs
// ge:skill labs
// ge:test tests/lab_model_view_test.cpp
#include "lab/model_compare.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>

#include "lab/png.hpp"

namespace ge::lab {

namespace {

constexpr Rgba kSheet{18, 20, 26};
constexpr Rgba kText{226, 230, 238};
constexpr Rgba kDim{150, 156, 170};
constexpr int kCaption = 26;
constexpr int kGap = 6;

// A line's `key=value` pairs; a value in double quotes may hold spaces.
bool pairs_of(const std::string& line, std::vector<std::pair<std::string, std::string>>& out) {
  size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    if (i >= line.size() || line[i] == '#') break;
    const size_t eq = line.find('=', i);
    if (eq == std::string::npos) return false;
    const std::string key = line.substr(i, eq - i);
    i = eq + 1;
    std::string value;
    if (i < line.size() && line[i] == '"') {
      const size_t close = line.find('"', i + 1);
      if (close == std::string::npos) return false;
      value = line.substr(i + 1, close - i - 1);
      i = close + 1;
    } else {
      const size_t end = line.find_first_of(" \t", i);
      value = line.substr(i, end == std::string::npos ? std::string::npos : end - i);
      i = end == std::string::npos ? line.size() : end;
    }
    out.emplace_back(key, value);
  }
  return true;
}

bool number(const std::string& s, double* v) {
  char* end = nullptr;
  *v = std::strtod(s.c_str(), &end);
  return !s.empty() && end == s.c_str() + s.size();
}

std::string fixed0(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.0f", v);
  return b;
}

}  // namespace

bool parse_refs(const std::string& path, std::vector<RefRow>& rows, std::string* error) {
  std::ifstream in(path);
  if (!in) {
    *error = "cannot read the reference file " + path + "; next: pass --refs=<file> naming one";
    return false;
  }
  const std::filesystem::path dir = std::filesystem::path(path).parent_path();
  std::string line;
  int n = 0;
  while (std::getline(in, line)) {
    ++n;
    std::vector<std::pair<std::string, std::string>> kv;
    const std::string where = path + ":" + std::to_string(n);
    if (!pairs_of(line, kv)) {
      *error = where + ": not key=value pairs (an unclosed quote?); next: fix the line";
      return false;
    }
    if (kv.empty()) continue;
    RefRow r;
    for (const auto& [k, v] : kv) {
      double x = 0;
      if (k == "ref") {
        const std::filesystem::path p(v);
        r.ref = p.is_absolute() ? v : (dir / p).string();
      } else if (k == "crop") {
        const char* at = v.c_str();
        int read = 0;
        for (; read < 4; ++read) {
          char* end = nullptr;
          r.crop[read] = static_cast<int>(std::strtol(at, &end, 10));
          if (end == at || (read < 3 && *end != ',') || (read == 3 && *end != '\0')) break;
          at = end + 1;
        }
        if (read != 4 || r.crop[2] <= 0 || r.crop[3] <= 0) {
          *error = where + ": crop must be x,y,w,h in px with w and h above 0";
          return false;
        }
      } else if (k == "az") {
        double a0 = 0, a1 = 0, step = 0;
        const size_t dots = v.find(".."), colon = v.find(':');
        const bool range = dots != std::string::npos && colon != std::string::npos &&
                           colon > dots && number(v.substr(0, dots), &a0) &&
                           number(v.substr(dots + 2, colon - dots - 2), &a1) &&
                           number(v.substr(colon + 1), &step);
        if (range) {
          if (step <= 0 || a1 < a0 || (a1 - a0) / step > 11) {
            *error = where + ": az=a..b:step takes at most 12 panels with a step above 0";
            return false;
          }
          const int panels = static_cast<int>((a1 - a0) / step + 1e-9);
          for (int i = 0; i <= panels; ++i) r.azimuths.push_back(a0 + step * i);
        } else if (number(v, &x)) {
          r.azimuths.push_back(x);
        } else {
          *error = where + ": az must be degrees or a..b:step";
          return false;
        }
      } else if (k == "el" || k == "fov" || k == "fill" || k == "mesh") {
        if (!number(v, &x)) {
          *error = where;
          *error += ": " + k + " must be a number";
          return false;
        }
        if (k == "el") r.elevation = x;
        if (k == "fov") r.fov = x;
        if (k == "fill") r.fill = std::clamp(x, 0.2, 1.0);
        if (k == "mesh") r.lod = std::max(0, static_cast<int>(x));
      } else if (k == "pose") {
        r.pose = v;
      } else if (k == "label") {
        r.label = v;
      } else {
        *error = where;
        *error += ": unknown key '" + k;
        *error += "'; the keys are ref, crop, az, el, fov, fill, pose, mesh, label";
        return false;
      }
    }
    if (r.ref.empty()) {
      *error = where + ": a row needs ref=<photograph.png>";
      return false;
    }
    if (r.azimuths.empty()) r.azimuths.push_back(0);
    rows.push_back(r);
  }
  if (rows.empty()) {
    *error = path + " has no rows; next: add a line ref=<png> az=<deg> pose=<pose>";
    return false;
  }
  return true;
}

bool reference_sheet(const std::vector<RefRow>& rows, const assets::Model& model,
                     const PoseFn& pose_of, int row_h, const ViewStyle& style, Picture& sheet,
                     std::string* error) {
  struct Laid {
    Picture photo;
    int panel_w;
  };
  std::vector<Laid> laid;
  int width = 0;
  for (const RefRow& r : rows) {
    std::optional<Picture> full = read_png(r.ref);
    if (!full) {
      *error = "cannot read " + r.ref +
               " as an 8-bit PNG; next: convert it (convert photo.jpg PNG32:photo.png)";
      return false;
    }
    int cx = 0, cy = 0, cw = full->width, ch = full->height;
    if (r.crop[2] > 0) {
      cx = std::clamp(r.crop[0], 0, full->width - 1);
      cy = std::clamp(r.crop[1], 0, full->height - 1);
      cw = std::min(r.crop[2], full->width - cx);
      ch = std::min(r.crop[3], full->height - cy);
    }
    Picture crop = create_picture(cw, ch, kSheet);
    for (int y = 0; y < ch; ++y) {
      const auto* src = &full->rgba[(size_t(cy + y) * size_t(full->width) + size_t(cx)) * 4];
      std::copy(src, src + size_t(cw) * 4, &crop.rgba[size_t(y) * size_t(cw) * 4]);
    }
    const int pw = std::max(16, static_cast<int>(std::lround(row_h * double(cw) / ch)));
    laid.push_back({resized(crop, pw, row_h), pw});
    const int w = kGap + pw * (1 + static_cast<int>(r.azimuths.size())) +
                  kGap * (1 + static_cast<int>(r.azimuths.size()));
    width = std::max(width, w);
  }
  const int line_h = kCaption + row_h + kGap;
  sheet = create_picture(width, line_h * static_cast<int>(rows.size()) + kGap, kSheet);
  for (size_t k = 0; k < rows.size(); ++k) {
    const RefRow& r = rows[k];
    const int y0 = kGap + static_cast<int>(k) * line_h;
    Shape shape;
    double floor_y = 0;
    if (!pose_of(r.pose, r.lod, shape, &floor_y, error)) return false;
    const assets::Box box = shape_box(shape);
    draw_text(sheet, kGap, y0,
              (r.label.empty() ? std::string("REFERENCE") : r.label) + "   POSE " + r.pose +
                  "   EL " + fixed0(r.elevation) + " FOV " + fixed0(r.fov),
              kText);
    blit(sheet, laid[k].photo, kGap, y0 + kCaption);
    int x = kGap * 2 + laid[k].panel_w;
    for (double az : r.azimuths) {
      const ViewCam cam =
          orbit_cam(box, az, r.elevation, r.fov, r.fill, double(laid[k].panel_w) / row_h);
      draw_shape(sheet, x, y0 + kCaption, laid[k].panel_w, row_h, cam, model, shape, style,
                 floor_y);
      fill_rect(sheet, x, y0 + kCaption - 11, laid[k].panel_w, 10, kSheet);
      draw_text(sheet, x + 2, y0 + kCaption - 10, "AZ " + fixed0(az), kDim);
      x += laid[k].panel_w + kGap;
    }
  }
  return true;
}

}  // namespace ge::lab
