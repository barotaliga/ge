// The sequel's layout language (sequel_layout.hpp).
//
// ge:summary The sequel's status bar language run into pictures and text at screen positions
// ge:keywords sequel layout status-bar layout-language run_layout
// ge:skill ui shooter
// ge:test tests/shooters_edge_sequel_layout_test.cpp
#include "shooters_edge/hud/sequel_layout.hpp"

#include <algorithm>
#include <cstdio>

namespace shooters_edge::hud {

namespace {

constexpr int kDigitWidth = 16;
constexpr int kLetter = 8;

// The program's words: whitespace (every byte up to the space), a quoted word to the next quote,
// a comment from two slashes to the line's end. A plain word over 127 bytes reads as empty, a
// quoted one is cut to 128.
struct Words {
  std::string_view s;
  size_t at = 0;
  std::string word;

  bool next() {
    word.clear();
    for (;;) {
      while (at < s.size() && static_cast<uint8_t>(s[at]) <= ' ') at++;
      if (at >= s.size()) return false;
      if (s[at] == '/' && at + 1 < s.size() && s[at + 1] == '/') {
        while (at < s.size() && s[at] != '\n') at++;
        continue;
      }
      break;
    }
    if (s[at] == '"') {
      at++;
      while (at < s.size() && s[at] != '"') {
        if (word.size() < 128) word.push_back(s[at]);
        at++;
      }
      if (at < s.size()) at++;  // the closing quote
      return true;
    }
    while (at < s.size() && static_cast<uint8_t>(s[at]) > ' ') word.push_back(s[at++]);
    if (word.size() > 127) word.clear();
    return true;
  }
  int number() {
    next();
    return to_int(word);
  }
  static int to_int(std::string_view w) {
    size_t i = 0;
    bool minus = false;
    if (i < w.size() && (w[i] == '-' || w[i] == '+')) minus = w[i++] == '-';
    long v = 0;
    for (; i < w.size() && w[i] >= '0' && w[i] <= '9'; i++) {
      v = v * 10 + (w[i] - '0');
      if (v > 2147483647L) v = 2147483647L;
    }
    return static_cast<int>(minus ? -v : v);
  }
};

struct Run {
  const LayoutInput& in;
  std::vector<LayoutOp>& out;
  int x = 0, y = 0;

  int stat(int s) const {
    if (s < 0 || s >= kLayoutStats || static_cast<size_t>(s) >= in.stats.size()) return 0;
    return in.stats[static_cast<size_t>(s)];
  }
  std::string_view string(int index) const {
    if (index < 0 || index >= kLayoutStrings || in.string == nullptr) return {};
    return in.string(in.ctx, index);
  }
  void picture(int px, int py, std::string name, std::string fallback = {}) {
    if (name.empty()) return;
    LayoutOp op;
    op.kind = LayoutOp::Kind::picture;
    op.x = px;
    op.y = py;
    op.name = std::move(name);
    op.fallback = std::move(fallback);
    out.push_back(std::move(op));
  }
  void text(int px, int py, std::string_view t, bool alt) {
    if (t.empty()) return;
    LayoutOp op;
    op.kind = LayoutOp::Kind::text;
    op.x = px;
    op.y = py;
    op.text = std::string(t);
    op.alt = alt;
    out.push_back(std::move(op));
  }
  // `value` in a field of `width` digits at the cursor, in the plain or coloured digits.
  void number(int width, int value, bool coloured) {
    if (width < 1) return;
    width = std::min(width, 5);
    const std::string t = std::to_string(value);
    const int shown = std::min(static_cast<int>(t.size()), width);
    int at = x + 2 + kDigitWidth * (width - shown);
    for (int i = 0; i < shown; i++, at += kDigitWidth) {
      const std::string digit = t[static_cast<size_t>(i)] == '-'
                                    ? std::string("minus")
                                    : std::string(1, t[static_cast<size_t>(i)]);
      picture(at, y, (coloured ? "anum_" : "num_") + digit);
    }
  }
  // A big number's set: plain above `low`, flashing (every 4 server frames) from `low` down to the
  // first value that stays coloured.
  bool flashing() const { return ((in.frame >> 2) & 1) != 0; }
  // A score block's or row's client: its name (before the backslash) and its `model/skin`.
  void client_strings(int c, std::string& name, std::string& skin) const {
    const std::string_view s = string(kLayoutSkins + c);
    const size_t slash = s.find('\\');
    name = std::string(s.substr(0, slash));
    skin = slash == std::string_view::npos ? std::string() : std::string(s.substr(slash + 1));
  }
};

}  // namespace

std::string layout_picture_path(std::string_view name) {
  if (!name.empty() && (name[0] == '/' || name[0] == '\\')) return std::string(name.substr(1));
  return "pics/" + std::string(name) + ".pcx";
}

void run_layout(const LayoutInput& in, std::vector<LayoutOp>& out) {
  out.clear();
  Run r{in, out, 0, 0};
  Words w;
  w.s = in.program;
  const int half_w = in.width / 2 - 160, half_h = in.height / 2 - 120;
  while (w.next()) {
    const std::string& k = w.word;
    if (k == "xl") {
      r.x = w.number();
    } else if (k == "xr") {
      r.x = in.width + w.number();
    } else if (k == "xv") {
      r.x = half_w + w.number();
    } else if (k == "yt") {
      r.y = w.number();
    } else if (k == "yb") {
      r.y = in.height + w.number();
    } else if (k == "yv") {
      r.y = half_h + w.number();
    } else if (k == "pic") {
      const int v = r.stat(w.number());
      if (v > 0 && v < 256) {
        const std::string_view name = r.string(kLayoutImages + v);
        if (!name.empty()) r.picture(r.x, r.y, std::string(name));
      }
    } else if (k == "picn") {
      w.next();
      r.picture(r.x, r.y, w.word);
    } else if (k == "num") {
      const int width = w.number();
      r.number(width, r.stat(w.number()), false);
    } else if (k == "hnum") {
      const int v = r.stat(1);
      if ((r.stat(15) & 1) != 0) r.picture(r.x, r.y, "field_3");
      r.number(3, v, v > 25 ? false : v > 0 ? r.flashing() : true);
    } else if (k == "anum") {
      const int v = r.stat(3);
      if (v < 0) continue;
      if ((r.stat(15) & 4) != 0) r.picture(r.x, r.y, "field_3");
      r.number(3, v, v > 5 ? false : r.flashing());
    } else if (k == "rnum") {
      const int v = r.stat(5);
      if (v < 1) continue;
      if ((r.stat(15) & 2) != 0) r.picture(r.x, r.y, "field_3");
      r.number(3, v, false);
    } else if (k == "stat_string") {
      const int s = w.number();
      if (s < 0 || s >= kLayoutStrings) continue;
      r.text(r.x, r.y, r.string(r.stat(s)), false);
    } else if (k == "string" || k == "string2") {
      const bool alt = k == "string2";
      w.next();
      r.text(r.x, r.y, w.word, alt);
    } else if (k == "cstring" || k == "cstring2") {
      const bool alt = k == "cstring2";
      w.next();
      std::string_view t = w.word;
      int line_y = r.y;
      for (;;) {
        const size_t nl = t.find('\n');
        const std::string_view line = t.substr(0, nl);
        const int len = static_cast<int>(line.size());
        r.text(r.x + (320 - kLetter * len) / 2, line_y, line, alt);
        if (nl == std::string_view::npos) break;
        t.remove_prefix(nl + 1);
        line_y += kLetter;
      }
    } else if (k == "if") {
      if (r.stat(w.number()) != 0) continue;
      while (w.next() && w.word != "endif") {
      }
    } else if (k == "client") {
      const int bx = half_w + w.number();
      const int by = half_h + w.number();
      const int c = w.number();
      const int score = w.number(), ping = w.number(), minutes = w.number();
      r.x = bx;
      r.y = by;
      if (c < 0 || c > 255) continue;
      std::string name, skin;
      r.client_strings(c, name, skin);
      r.text(bx + 32, by, name, true);
      r.text(bx + 32, by + 8, "Score: ", false);
      r.text(bx + 88, by + 8, std::to_string(score), true);
      r.text(bx + 32, by + 16, "Ping:  " + std::to_string(ping), false);
      r.text(bx + 32, by + 24, "Time:  " + std::to_string(minutes), false);
      r.picture(bx, by, "/players/" + (skin.empty() ? std::string("male/grunt") : skin) + "_i.pcx",
                "/players/male/grunt_i.pcx");
    } else if (k == "ctf") {
      const int bx = half_w + w.number();
      const int by = half_h + w.number();
      const int c = w.number();
      const int score = w.number();
      const int ping = std::min(w.number(), 999);
      r.x = bx;
      r.y = by;
      if (c < 0 || c > 255) continue;
      std::string name, skin;
      r.client_strings(c, name, skin);
      name.resize(12, ' ');
      char row[64];
      std::snprintf(row, sizeof row, "%3d %3d ", score, ping);
      r.text(bx, by, std::string(row) + name, c == in.self);
    }
    // Any other word (an `endif` met while drawing among them) is skipped.
  }
}

}  // namespace shooters_edge::hud
