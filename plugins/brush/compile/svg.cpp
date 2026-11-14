// An SVG floor plan read as a map plan (svg.hpp): the elements, their translations, their shapes.
//
// ge:summary Reads an SVG floor plan's tagged shapes into plan statements
// ge:keywords map-compiler svg xml element attribute path points translate data-ge plan
// ge:skill brush
// ge:test tests/brush_compile_test.cpp
#include "brush/compile/svg.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <utility>
#include <vector>

#include "core/num_text.hpp"

namespace ge::brush::mapc {

namespace {

std::string fmt(double v) {
  const double r = std::nearbyint(v);
  if (std::fabs(v - r) < 1e-6) return std::to_string(int64_t(r));
  return core::num::to_fixed(v, 3);
}

std::string unescape(const std::string& s) {
  static const std::pair<const char*, char> kEnt[] = {
      {"&quot;", '"'}, {"&apos;", '\''}, {"&lt;", '<'}, {"&gt;", '>'}, {"&amp;", '&'}};
  std::string out;
  for (size_t i = 0; i < s.size();) {
    bool hit = false;
    for (const auto& [e, c] : kEnt) {
      const size_t n = std::char_traits<char>::length(e);
      if (s.compare(i, n, e) == 0) {
        out += c;
        i += n;
        hit = true;
        break;
      }
    }
    if (!hit) out += s[i++];
  }
  return out;
}

// Numbers in a points list or path data, with any commands kept as one-letter strings.
std::vector<std::string> tokens(const std::string& s) {
  std::vector<std::string> out;
  size_t i = 0;
  while (i < s.size()) {
    const char c = s[i];
    if (std::isspace(static_cast<unsigned char>(c)) || c == ',') {
      ++i;
      continue;
    }
    if (std::isalpha(static_cast<unsigned char>(c)) && c != 'e' && c != 'E') {
      out.emplace_back(1, c);
      ++i;
      continue;
    }
    char* end = nullptr;
    std::strtod(s.c_str() + i, &end);
    if (end == s.c_str() + i) {
      out.emplace_back(1, c);  // a stray character: refused by the caller
      ++i;
      continue;
    }
    out.emplace_back(s.substr(i, size_t(end - (s.c_str() + i))));
    i = size_t(end - s.c_str());
  }
  return out;
}

bool num(const std::string& s, double* v) {
  if (s.empty()) return false;
  char* end = nullptr;
  *v = std::strtod(s.c_str(), &end);
  return end == s.c_str() + s.size() && std::isfinite(*v);
}

struct Elem {
  std::string tag;
  std::map<std::string, std::string> attrs;
  bool closes = false, self_closing = false;
  int line = 0;
};

// translate(x[,y]) or empty; false on any other transform.
bool translation(const std::string& t, double* dx, double* dy) {
  *dx = *dy = 0;
  if (t.empty()) return true;
  const size_t open = t.find('(');
  const size_t close = t.find(')');
  if (open == std::string::npos || close == std::string::npos) return false;
  std::string name = t.substr(0, open);
  while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) name.pop_back();
  while (!name.empty() && std::isspace(static_cast<unsigned char>(name.front()))) name.erase(0, 1);
  if (name != "translate") return false;
  if (t.find_first_not_of(" \t\r\n", close + 1) != std::string::npos) return false;
  const std::vector<std::string> v = tokens(t.substr(open + 1, close - open - 1));
  if (v.empty() || v.size() > 2 || !num(v[0], dx)) return false;
  if (v.size() == 2 && !num(v[1], dy)) return false;
  return true;
}

}  // namespace

bool svg_to_plan(std::string_view svg_text, std::string* plan, std::string* error) {
  const std::string svg(svg_text);
  std::vector<Elem> elems;
  int line = 1;
  for (size_t i = 0; i < svg.size();) {
    if (svg[i] == '\n') ++line;
    if (svg[i] != '<') {
      ++i;
      continue;
    }
    if (svg.compare(i, 4, "<!--") == 0) {
      const size_t e = svg.find("-->", i);
      for (size_t k = i; k < (e == std::string::npos ? svg.size() : e); ++k) line += svg[k] == '\n';
      i = e == std::string::npos ? svg.size() : e + 3;
      continue;
    }
    if (svg.compare(i, 2, "<?") == 0 || svg.compare(i, 2, "<!") == 0) {
      const size_t e = svg.find('>', i);
      i = e == std::string::npos ? svg.size() : e + 1;
      continue;
    }
    Elem el;
    el.line = line;
    size_t k = i + 1;
    if (k < svg.size() && svg[k] == '/') {
      el.closes = true;
      ++k;
    }
    while (k < svg.size() && !std::isspace(static_cast<unsigned char>(svg[k])) && svg[k] != '>' &&
           svg[k] != '/') {
      el.tag += svg[k++];
    }
    // Attributes.
    while (k < svg.size() && svg[k] != '>') {
      if (svg[k] == '\n') ++line;
      if (svg[k] == '/') {
        el.self_closing = true;
        ++k;
        continue;
      }
      if (std::isspace(static_cast<unsigned char>(svg[k]))) {
        ++k;
        continue;
      }
      std::string name;
      while (k < svg.size() && svg[k] != '=' && svg[k] != '>' &&
             !std::isspace(static_cast<unsigned char>(svg[k]))) {
        name += svg[k++];
      }
      while (k < svg.size() && std::isspace(static_cast<unsigned char>(svg[k]))) ++k;
      if (k < svg.size() && svg[k] == '=') {
        ++k;
        while (k < svg.size() && std::isspace(static_cast<unsigned char>(svg[k]))) ++k;
        if (k < svg.size() && (svg[k] == '"' || svg[k] == '\'')) {
          const char q = svg[k++];
          std::string v;
          while (k < svg.size() && svg[k] != q) {
            if (svg[k] == '\n') ++line;
            v += svg[k++];
          }
          ++k;
          el.attrs[name] = unescape(v);
        }
      }
    }
    i = k + 1;
    // A tag's own name may carry a namespace prefix: keep the local name.
    const size_t colon = el.tag.find(':');
    if (colon != std::string::npos) el.tag = el.tag.substr(colon + 1);
    elems.push_back(std::move(el));
  }

  std::string errors;
  std::string out;
  const auto fault = [&](int at, const std::string& what, const std::string& next) {
    errors += "svg line " + std::to_string(at) + ": " + what + "; next: " + next + "\n";
  };
  double scale = 1;
  // The groups' translations, a stack.
  std::vector<std::pair<double, double>> stack = {{0, 0}};
  for (const Elem& e : elems) {
    if (e.closes) {
      if (e.tag == "g" || e.tag == "svg" || e.tag == "a") {
        if (stack.size() > 1) stack.pop_back();
      }
      continue;
    }
    double dx = 0, dy = 0;
    const auto tr = e.attrs.find("transform");
    if (tr != e.attrs.end() && !translation(tr->second, &dx, &dy)) {
      fault(e.line, "the transform '" + tr->second + "' is not a translation",
            "apply the transform to the shape in your editor (an object to path, or ungroup)");
      continue;
    }
    const double ox = stack.back().first + dx, oy = stack.back().second + dy;
    if (e.tag == "svg") {
      const auto s = e.attrs.find("data-ge-scale");
      if (s != e.attrs.end() && (!num(s->second, &scale) || !(scale > 0))) {
        fault(e.line, "data-ge-scale is not a number above 0", "write data-ge-scale=\"2\"");
        scale = 1;
      }
    }
    if ((e.tag == "g" || e.tag == "svg" || e.tag == "a") && !e.self_closing) {
      stack.emplace_back(ox, oy);
    }
    const auto ge = e.attrs.find("data-ge");
    if (ge == e.attrs.end()) continue;
    std::string stmt = ge->second;
    const size_t sp = stmt.find_first_of(" \t");
    const std::string verb = stmt.substr(0, sp);
    const auto at = [&](double x, double y) {
      return fmt((x + ox) * scale) + "," + fmt(-(y + oy) * scale);
    };
    const auto attr = [&](const char* k, double* v) {
      const auto it = e.attrs.find(k);
      return it != e.attrs.end() && num(it->second, v);
    };
    if (verb == "world" || verb == "textures" || verb == "pad" || verb == "symmetry") {
      out += stmt + "\n";
      continue;
    }
    if (verb == "light" || verb == "spawn" || verb == "entity") {
      double cx = 0, cy = 0;
      if (!((attr("cx", &cx) && attr("cy", &cy)) || (attr("x", &cx) && attr("y", &cy)))) {
        fault(e.line, "a " + verb + " needs a circle (cx, cy) or a point (x, y)",
              "draw it as a circle");
        continue;
      }
      // z= gives the height: the point is x,y,z.
      std::string rest;
      std::string z = "0";
      size_t p = 0;
      std::vector<std::string> words;
      while (p < stmt.size()) {
        const size_t q = stmt.find(' ', p);
        words.push_back(stmt.substr(p, q == std::string::npos ? std::string::npos : q - p));
        if (q == std::string::npos) break;
        p = q + 1;
      }
      std::string head = words[0];
      size_t w = 1;
      if (verb == "entity" && words.size() > 1) head += " " + words[w++];
      for (; w < words.size(); ++w) {
        if (words[w].rfind("z=", 0) == 0) {
          z = words[w].substr(2);
        } else if (!words[w].empty()) {
          rest += " " + words[w];
        }
      }
      double zz = 0;
      if (!num(z, &zz)) {
        fault(e.line, "z= of a point is one number", "write z=24");
        continue;
      }
      out += head + " " + at(cx, cy) + "," + fmt(zz) + rest + "\n";
      continue;
    }
    // A shape.
    std::vector<std::pair<double, double>> pts;
    bool rect = false;
    if (e.tag == "rect") {
      double x = 0, y = 0, w = 0, h = 0;
      attr("x", &x);
      attr("y", &y);
      if (!attr("width", &w) || !attr("height", &h)) {
        fault(e.line, "the rect has no width or height", "give it a size");
        continue;
      }
      pts = {{x, y}, {x + w, y + h}};
      rect = true;
    } else if (e.tag == "line") {
      double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
      attr("x1", &x1);
      attr("y1", &y1);
      attr("x2", &x2);
      attr("y2", &y2);
      pts = {{x1, y1}, {x2, y2}};
    } else if (e.tag == "polygon" || e.tag == "polyline") {
      const auto it = e.attrs.find("points");
      const std::vector<std::string> t = tokens(it == e.attrs.end() ? "" : it->second);
      bool ok = t.size() >= 4 && t.size() % 2 == 0;
      for (size_t k = 0; ok && k + 1 < t.size(); k += 2) {
        double x = 0, y = 0;
        ok = num(t[k], &x) && num(t[k + 1], &y);
        pts.emplace_back(x, y);
      }
      if (!ok) {
        fault(e.line, "the points are not pairs of numbers", "redraw the shape");
        continue;
      }
    } else if (e.tag == "path") {
      const auto it = e.attrs.find("d");
      const std::vector<std::string> t = tokens(it == e.attrs.end() ? "" : it->second);
      char cmd = 0;
      double cx = 0, cy = 0;
      bool ok = true;
      for (size_t k = 0; k < t.size() && ok;) {
        if (t[k].size() == 1 && std::isalpha(static_cast<unsigned char>(t[k][0]))) {
          cmd = t[k][0];
          ++k;
          if (cmd == 'Z' || cmd == 'z') continue;
          if (std::string("MmLlHhVv").find(cmd) == std::string::npos) {
            fault(e.line, std::string("the path has a curve or arc (") + cmd + ")",
                  "flatten it to straight segments in your editor");
            ok = false;
          }
          continue;
        }
        double a = 0, b = 0;
        const bool rel = std::islower(static_cast<unsigned char>(cmd)) != 0;
        const char up = char(std::toupper(static_cast<unsigned char>(cmd)));
        if (up == 'H' || up == 'V') {
          ok = num(t[k], &a);
          ++k;
          if (up == 'H') cx = rel ? cx + a : a;
          if (up == 'V') cy = rel ? cy + a : a;
        } else if (up == 'M' || up == 'L') {
          ok = k + 1 < t.size() && num(t[k], &a) && num(t[k + 1], &b);
          k += 2;
          cx = rel ? cx + a : a;
          cy = rel ? cy + b : b;
          if (cmd == 'M') cmd = 'L';
          if (cmd == 'm') cmd = 'l';
        } else {
          ok = false;
        }
        if (ok) pts.emplace_back(cx, cy);
      }
      if (!ok) {
        if (errors.empty() ||
            errors.find("svg line " + std::to_string(e.line)) == std::string::npos) {
          fault(e.line, "the path's data is not straight segments", "redraw it with lines only");
        }
        continue;
      }
      // A closing point back at the start is the start.
      if (pts.size() > 1 && pts.front() == pts.back()) pts.pop_back();
    } else {
      fault(e.line, "a <" + e.tag + "> cannot carry a shape",
            "use rect, polygon, polyline, path, line or circle");
      continue;
    }
    std::string shape;
    if (verb == "wall") {
      if (stmt.find("thick=") == std::string::npos) {
        double sw = 0;
        if (attr("stroke-width", &sw) && sw > 0) stmt += " thick=" + fmt(sw * scale);
      }
      shape = "line";
      for (const auto& [x, y] : pts) shape += " " + at(x, y);
    } else if (rect) {
      shape = "box " + at(pts[0].first, pts[0].second) + " " + at(pts[1].first, pts[1].second);
    } else {
      if (verb == "stairs" || verb == "ramp") {
        fault(e.line, verb + " needs a rect", "draw stairs and ramps as rectangles");
        continue;
      }
      shape = "poly";
      for (const auto& [x, y] : pts) shape += " " + at(x, y);
    }
    // The shape goes after the statement's verb and name: room NAME ..., solid NAME ...
    out += stmt + " " + shape + "\n";
  }
  if (!errors.empty()) {
    errors.pop_back();
    *error = errors;
    return false;
  }
  *plan = out;
  return true;
}

}  // namespace ge::brush::mapc
