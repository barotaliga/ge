// Quake II's layout language (q2_layout.hpp).
//
// ge:summary Quake II's status bar language run into pictures and text at screen positions
// ge:keywords q2 layout status-bar layout-language run_layout
// ge:skill ui shooter
// ge:test tests/shooters_edge_q2_layout_test.cpp
#include "shooters_edge/hud/q2_layout.hpp"

#include <algorithm>
#include <array>
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

// The word's text form ("string", "cstring2", ...) without its `stat_`, or empty when it is none.
std::string_view text_form(std::string_view k) {
  if (k.starts_with("stat_")) k.remove_prefix(5);
  for (std::string_view f : {"string", "string2", "cstring", "cstring2", "rstring", "rstring2"})
    if (k == f) return k;
  return {};
}

struct Run {
  const LayoutInput& in;
  std::vector<LayoutOp>& out;
  int x = 0, y = 0;
  uint8_t rgba[4] = {255, 255, 255, 255};

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
    std::copy(rgba, rgba + 4, op.rgba);
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
    std::copy(rgba, rgba + 4, op.rgba);
    out.push_back(std::move(op));
  }
  void fill(int px, int py, int w, int h, std::array<uint8_t, 4> c) {
    if (w <= 0 || h <= 0) return;
    LayoutOp op;
    op.kind = LayoutOp::Kind::fill;
    op.x = px;
    op.y = py;
    op.w = w;
    op.h = h;
    std::copy(c.begin(), c.end(), op.rgba);
    out.push_back(std::move(op));
  }
  // Each line of `t` centred in 320 px from the cursor's x, 8 px apart.
  void centred(std::string_view t, bool alt) {
    int line_y = y;
    for (;;) {
      const size_t nl = t.find('\n');
      const std::string_view line = t.substr(0, nl);
      const int len = static_cast<int>(line.size());
      text(x + (320 - kLetter * len) / 2, line_y, line, alt);
      if (nl == std::string_view::npos) break;
      t.remove_prefix(nl + 1);
      line_y += kLetter;
    }
  }
  // One line ending at the cursor; `flip`: each letter's top bit flipped (the other set).
  void right(std::string_view t, bool flip) {
    std::string line(t);
    if (flip)
      for (char& c : line) c = static_cast<char>(static_cast<uint8_t>(c) ^ 0x80);
    text(x - kLetter * static_cast<int>(line.size()), y, line, false);
  }
  // Text in one of the six forms by its word's tail: "", "2", "c", "c2", "r", "r2".
  void form(std::string_view kind, std::string_view t) {
    const bool alt = kind.ends_with('2');
    if (kind.starts_with('c'))
      centred(t, alt);
    else if (kind.starts_with('r'))
      right(t, alt);
    else
      text(x, y, t, alt);
  }
  // `value` in a field of `width` digits at the cursor, in the plain or coloured digits.
  void number(int width, int value, bool coloured) {
    if (width < 1) return;
    width = std::min(width, 5);
    const std::string t = std::to_string(value);
    const int shown = std::min(static_cast<int>(t.size()), width);
    if (in.text_numbers) {  // the text picture's: the digits in letters, right-aligned
      text(x + kLetter * (width - shown), y, t.substr(0, static_cast<size_t>(shown)), false);
      return;
    }
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

bool layout_colour(std::string_view c, uint8_t rgba[4]) {
  static constexpr std::string_view kNames[8] = {"black", "red",  "green",   "yellow",
                                                 "blue",  "cyan", "magenta", "white"};
  int index = -1;
  if (c.size() == 1 && c[0] >= '0' && c[0] <= '7') index = c[0] - '0';
  for (int i = 0; i < 8; i++)
    if (c == kNames[i]) index = i;
  if (index >= 0) {  // bit 0 red, bit 1 green, bit 2 blue
    rgba[0] = (index & 1) != 0 ? 255 : 0;
    rgba[1] = (index & 2) != 0 ? 255 : 0;
    rgba[2] = (index & 4) != 0 ? 255 : 0;
    rgba[3] = 255;
    return true;
  }
  if (c.empty() || c[0] != '#') return false;
  c.remove_prefix(1);
  if (c.size() != 3 && c.size() != 6 && c.size() != 8) return false;
  uint8_t d[8];
  for (size_t i = 0; i < c.size(); i++) {
    const char h = c[i];
    if (h >= '0' && h <= '9')
      d[i] = static_cast<uint8_t>(h - '0');
    else if (h >= 'a' && h <= 'f')
      d[i] = static_cast<uint8_t>(h - 'a' + 10);
    else if (h >= 'A' && h <= 'F')
      d[i] = static_cast<uint8_t>(h - 'A' + 10);
    else
      return false;
  }
  if (c.size() == 3) {
    for (int k = 0; k < 3; k++) rgba[k] = static_cast<uint8_t>(d[k] * 17);
    rgba[3] = 255;
  } else {
    for (size_t k = 0; k < 4; k++)
      rgba[k] = k < c.size() / 2 ? static_cast<uint8_t>(d[2 * k] * 16 + d[2 * k + 1]) : 255;
  }
  return true;
}

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
        if (name.empty()) continue;
        // A scope (`scope2x`, `scope4x`...) placed at the centred box fills it, scaled by ch_scale.
        const bool scope = r.x == half_w && r.y == half_h && name.size() == 7 &&
                           name.starts_with("scope") && name.back() == 'x';
        if (!scope) {
          r.picture(r.x, r.y, std::string(name));
          continue;
        }
        const float s = std::clamp(in.scope_scale, 0.1f, 9.0f);
        const int sw = static_cast<int>(320 * s), sh = static_cast<int>(240 * s);
        r.picture((in.width - sw) / 2 + in.scope_x, (in.height - sh) / 2 + in.scope_y,
                  std::string(name));
        r.out.back().w = sw;
        r.out.back().h = sh;
      }
    } else if (k == "health_bars") {  // a boss's name and its two health bars
      const int v = r.stat(w.number());
      const int index = w.number();
      r.centred(r.string(index), false);
      const int bw = in.width / 3, left = r.x + 160 - bw / 2;
      for (int bar = 0; bar < 2; bar++) {
        const int byte = (v >> (8 * bar)) & 0xff;
        if (byte == 0) continue;  // that bar hidden
        // bw * (byte - 1) / 254, rounded to the nearest pixel (halves up).
        const int full = (2 * bw * (byte - 1) + 254) / 508;
        r.fill(left, r.y + 12 + 8 * bar, full, 4, {239, 0, 0, 255});
        r.fill(left + full, r.y + 12 + 8 * bar, bw - full, 4, {63, 63, 63, 255});
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
    } else if (const std::string_view tail = text_form(k); !tail.empty()) {
      // string, string2, cstring, cstring2, rstring, rstring2 and the stat_ form of each.
      std::string form(tail.substr(0, tail.find("string")));
      if (tail.ends_with('2')) form += '2';
      if (k.starts_with("stat_")) {
        const int s = w.number();
        if (s < 0 || s >= kLayoutStrings) continue;
        r.form(form, r.string(r.stat(s)));
      } else {
        w.next();
        r.form(form, w.word);
      }
    } else if (k == "color") {
      w.next();
      layout_colour(w.word, r.rgba);
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

std::vector<std::string> layout_text(LayoutInput in, std::string_view over) {
  constexpr int kCols = 80, kRows = 40;
  std::vector<std::string> rows(kRows, std::string(kCols, ' '));
  in.width = kCols * kLetter;
  in.height = kRows * kLetter;
  in.text_numbers = true;
  std::vector<LayoutOp> ops;
  const auto put = [&rows](const std::vector<LayoutOp>& list) {
    for (const LayoutOp& op : list) {
      if (op.kind != LayoutOp::Kind::text) continue;  // pictures have no letters
      const int row = op.y / kLetter;
      if (row < 0 || row >= kRows) continue;
      int col = op.x / kLetter;
      for (const char ch : op.text) {
        if (col >= kCols) break;
        if (col >= 0) {
          const auto c = static_cast<unsigned char>(ch & 0x7f);
          char shown = static_cast<char>(c);
          switch (c) {
            case 13:
              shown = '>';
              break;
            case 16:
              shown = '[';
              break;
            case 17:
              shown = ']';
              break;
            case 29:
              shown = '<';
              break;
            case 30:
              shown = '=';
              break;
            case 31:
              shown = '>';
              break;
            default:
              if (c < 32 || c == 127) shown = ' ';
          }
          rows[static_cast<size_t>(row)][static_cast<size_t>(col)] = shown;
        }
        ++col;
      }
    }
  };
  run_layout(in, ops);
  put(ops);
  if (!over.empty()) {
    in.program = over;
    run_layout(in, ops);
    put(ops);
  }
  return rows;
}

}  // namespace shooters_edge::hud
