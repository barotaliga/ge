// The map editors' .map text (map_text.hpp).
//
// ge:summary Reads and writes the map editors' .map text: entities, brushes, sides, projections
// ge:keywords map-text dot-map valve-220 three-points tokenizer
// ge:skill brush
// ge:test tests/brush_map_text_test.cpp
#include "brush/compile/map_text.hpp"

#include <cmath>
#include <cstdlib>
#include <vector>

#include "core/num_text.hpp"

namespace ge::brush::mapc {

namespace {

struct Token {
  std::string text;
  int line = 0;
  bool quoted = false;
};

// The text cut into tokens: quoted strings, the one-character marks ( ) [ ] { }, and runs of
// anything else up to a space or a mark.
std::vector<Token> tokens_of(std::string_view s) {
  std::vector<Token> out;
  int line = 1;
  size_t i = 0;
  while (i < s.size()) {
    const char c = s[i];
    if (c == '\n') {
      ++line;
      ++i;
    } else if (c == ' ' || c == '\t' || c == '\r') {
      ++i;
    } else if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') {
      while (i < s.size() && s[i] != '\n') ++i;
    } else if (c == '"') {
      const size_t end = s.find('"', i + 1);
      const size_t stop = end == std::string_view::npos ? s.size() : end;
      out.push_back({std::string(s.substr(i + 1, stop - i - 1)), line, true});
      for (size_t k = i; k < stop; ++k) line += s[k] == '\n' ? 1 : 0;
      i = stop + 1;
    } else if (c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}') {
      out.push_back({std::string(1, c), line, false});
      ++i;
    } else {
      const size_t start = i;
      while (i < s.size()) {
        const char d = s[i];
        if (d == ' ' || d == '\t' || d == '\r' || d == '\n' || d == '(' || d == ')' || d == '[' ||
            d == ']' || d == '{' || d == '}' || d == '"')
          break;
        ++i;
      }
      out.push_back({std::string(s.substr(start, i - start)), line, false});
    }
  }
  return out;
}

class Reader {
 public:
  explicit Reader(std::vector<Token> t) : t_(std::move(t)) {}
  bool done() const { return at_ >= t_.size(); }
  const Token& peek() const { return t_[at_]; }
  int line() const { return done() ? (t_.empty() ? 1 : t_.back().line) : t_[at_].line; }
  bool is(const char* s) const { return !done() && !t_[at_].quoted && t_[at_].text == s; }
  Token take() { return t_[at_++]; }
  bool expect(const char* s, std::string* fault) {
    if (is(s)) {
      ++at_;
      return true;
    }
    *fault = std::string("expected `") + s + "`, found " +
             (done() ? std::string("the end") : "`" + t_[at_].text + "`");
    return false;
  }
  bool number(double* out, std::string* fault) {
    if (done()) {
      *fault = "expected a number, found the end";
      return false;
    }
    const std::string& s = t_[at_].text;
    char* end = nullptr;
    *out = std::strtod(s.c_str(), &end);
    if (s.empty() || end != s.c_str() + s.size() || !std::isfinite(*out)) {
      *fault = "expected a number, found `" + s + "`";
      return false;
    }
    ++at_;
    return true;
  }
  // Skips to just after the next `}` (one brush or one entity's end), to read on after a fault.
  void skip_past_close() {
    while (!done() && !is("}")) ++at_;
    if (!done()) ++at_;
  }

 private:
  std::vector<Token> t_;
  size_t at_ = 0;
};

bool read_point(Reader& r, V3* p, std::string* fault) {
  if (!r.expect("(", fault)) return false;
  for (double& v : *p)
    if (!r.number(&v, fault)) return false;
  return r.expect(")", fault);
}

bool read_side(Reader& r, Side* side, std::string* fault) {
  V3 p[3];
  for (V3& q : p)
    if (!read_point(r, &q, fault)) return false;
  if (!plane_of(p[1], p[0], p[2], &side->plane)) {
    *fault = "the side's three points are in a line";
    return false;
  }
  if (r.done() || r.is("(") || r.is("}")) {
    *fault = "a side names no texture";
    return false;
  }
  side->texture = r.take().text;
  if (r.is("[")) {
    double ax[2][4];
    for (auto& a : ax) {
      if (!r.expect("[", fault)) return false;
      for (double& v : a)
        if (!r.number(&v, fault)) return false;
      if (!r.expect("]", fault)) return false;
    }
    double turn = 0, sx = 1, sy = 1;
    if (!r.number(&turn, fault) || !r.number(&sx, fault) || !r.number(&sy, fault)) return false;
    if (sx == 0) sx = 1;
    if (sy == 0) sy = 1;
    for (int k = 0; k < 3; ++k) {
      side->vecs[0][k] = ax[0][k] / sx;
      side->vecs[1][k] = ax[1][k] / sy;
    }
    side->vecs[0][3] = ax[0][3];
    side->vecs[1][3] = ax[1][3];
  } else {
    double v[5];
    for (double& x : v)
      if (!r.number(&x, fault)) return false;
    axial_vecs(side->plane.n, v[0], v[1], v[2], v[3], v[4], side->vecs);
  }
  // The 38 family's contents, flags and value, when the line goes on with numbers.
  if (!r.done() && !r.is("(") && !r.is("}")) {
    double c = 0, f = 0, value = 0;
    if (!r.number(&c, fault) || !r.number(&f, fault) || !r.number(&value, fault)) return false;
    side->flags = int32_t(f);
    side->value = int32_t(value);
  }
  return true;
}

// The shortest exact text of `v`, "0" for a negative zero.
std::string num(double v) { return v == 0 ? "0" : core::num::to_string(v); }

std::string point(const V3& p) {
  return "( " + num(p[0]) + " " + num(p[1]) + " " + num(p[2]) + " )";
}

}  // namespace

bool read_map_text(std::string_view text, Source* out, std::string* error) {
  Reader r(tokens_of(text));
  out->entities.clear();
  std::string errors;
  const auto fail = [&](int line, const std::string& what, const char* next) {
    errors += "line " + std::to_string(line) + ": " + what + "; next: " + next + "\n";
  };
  while (!r.done()) {
    const int eline = r.line();
    std::string fault;
    if (!r.expect("{", &fault)) {
      fail(eline, fault, "start each entity with `{`");
      break;
    }
    Entity e;
    e.line = eline;
    while (!r.done() && !r.is("}")) {
      if (r.peek().quoted) {
        const Token k = r.take();
        if (r.done() || !r.peek().quoted) {
          fail(k.line, "the key \"" + k.text + "\" has no value", "write \"key\" \"value\"");
          continue;
        }
        e.keys.emplace_back(k.text, r.take().text);
      } else if (r.is("{")) {
        const int bline = r.line();
        r.take();
        Brush b;
        b.line = bline;
        bool ok = true;
        while (!r.done() && !r.is("}")) {
          Side s;
          const int sline = r.line();
          if (!read_side(r, &s, &fault)) {
            fail(sline, fault,
                 "write a side as ( x y z ) ( x y z ) ( x y z ) TEXTURE and its "
                 "projection");
            ok = false;
            r.skip_past_close();
            break;
          }
          b.sides.push_back(std::move(s));
        }
        if (ok) {
          if (!r.expect("}", &fault)) {
            fail(r.line(), fault, "close the brush with `}`");
            break;
          }
          if (b.sides.size() < 4) {
            fail(bline, "a brush of " + std::to_string(b.sides.size()) + " sides",
                 "give a brush four sides or more");
          } else {
            e.brushes.push_back(std::move(b));
          }
        }
      } else {
        fail(r.line(), "unexpected `" + r.peek().text + "`", "write a key and value or a brush");
        r.take();
      }
    }
    if (!r.expect("}", &fault)) {
      fail(r.line(), "the entity from line " + std::to_string(eline) + " is not closed",
           "close it with `}`");
      break;
    }
    out->entities.push_back(std::move(e));
  }
  if (errors.empty() && out->entities.empty())
    fail(1, "no entity", "write the world first: { \"classname\" \"worldspawn\" ... }");
  if (errors.empty() && out->entities[0].get("classname") != nullptr &&
      *out->entities[0].get("classname") != "worldspawn")
    fail(out->entities[0].line, "the first entity is not the world",
         "put the worldspawn entity first");
  set_contents(out);
  if (!errors.empty()) {
    *error = errors;
    return false;
  }
  return true;
}

std::string write_map_text(const Source& src) {
  std::string out;
  for (size_t ei = 0; ei < src.entities.size(); ++ei) {
    const Entity& e = src.entities[ei];
    out.append("// entity ").append(std::to_string(ei)).append("\n{\n");
    bool version = false;
    for (const auto& [k, v] : e.keys) {
      if (ei == 0 && k == "mapversion") {
        out += "\"mapversion\" \"220\"\n";
        version = true;
      } else {
        out.append("\"").append(k).append("\" \"").append(v).append("\"\n");
      }
    }
    if (ei == 0 && !version) out += "\"mapversion\" \"220\"\n";
    for (const Brush& b : e.brushes) {
      out += "{\n";
      for (const Side& s : b.sides) {
        // Three points on the plane: its point nearest the origin, then 64 along two axes in it
        // whose cross product is the normal, written in the order (p0 − p1) × (p2 − p1) reads.
        const V3& n = s.plane.n;
        const int least = std::fabs(n[0]) <= std::fabs(n[1]) && std::fabs(n[0]) <= std::fabs(n[2])
                              ? 0
                          : std::fabs(n[1]) <= std::fabs(n[2]) ? 1
                                                               : 2;
        V3 a = {0, 0, 0};
        a[size_t(least)] = 1;
        const V3 u = mul(normalized(cross(n, a)), 64);
        const V3 v = cross(n, u);
        const V3 p1 = mul(n, s.plane.d);
        out.append(point(add(p1, u))).append(" ").append(point(p1)).append(" ");
        out.append(point(add(p1, v))).append(" ").append(s.texture);
        double len[2];
        for (int k = 0; k < 2; ++k) {
          len[k] = length({s.vecs[k][0], s.vecs[k][1], s.vecs[k][2]});
          if (!(len[k] > 0)) len[k] = 1;
        }
        for (int k = 0; k < 2; ++k) {
          out.append(" [ ").append(num(s.vecs[k][0] / len[k])).append(" ");
          out.append(num(s.vecs[k][1] / len[k])).append(" ").append(num(s.vecs[k][2] / len[k]));
          out.append(" ").append(num(s.vecs[k][3])).append(" ]");
        }
        out.append(" 0 ").append(num(1 / len[0])).append(" ").append(num(1 / len[1]));
        if (s.flags != 0 || s.value != 0)
          out.append(" 0 ")
              .append(std::to_string(s.flags))
              .append(" ")
              .append(std::to_string(s.value));
        out += "\n";
      }
      out += "}\n";
    }
    out += "}\n";
  }
  return out;
}

}  // namespace ge::brush::mapc
