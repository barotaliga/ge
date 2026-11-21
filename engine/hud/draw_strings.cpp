// Draw strings (draw_strings.hpp).
//
// ge:summary draw and undraw: the list, the colour words, the values drawn each frame
// ge:keywords draw_color add_draw_commands draw_strings
// ge:skill ui console
// ge:test tests/hud_draw_strings_test.cpp
#include "hud/draw_strings.hpp"

#include <algorithm>
#include <cstdlib>

#include "console/cvar.hpp"
#include "hud/face.hpp"

namespace ge::hud {

namespace {

int hex_digit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

std::string padded(std::string s, size_t n, bool right) {
  if (s.size() >= n) return s;
  return right ? std::string(n - s.size(), ' ') + s : s + std::string(n - s.size(), ' ');
}

}  // namespace

bool draw_color(std::string_view word, DrawString& out) {
  static constexpr std::string_view kNames[] = {"black", "red",  "green",   "yellow",
                                                "blue",  "cyan", "magenta", "white"};
  static constexpr float kRgb[8][3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0},
                                       {0, 0, 1}, {0, 1, 1}, {1, 0, 1}, {1, 1, 1}};
  const std::string w = console::lower(word);
  if (w.empty() || w == "none") {
    out.ink = DrawString::Ink::own;
    return true;
  }
  if (w == "alt") {
    out.ink = DrawString::Ink::alt;
    return true;
  }
  for (size_t i = 0; i < 8; ++i) {
    if (w == kNames[i] || (w.size() == 1 && w[0] == static_cast<char>('0' + i))) {
      out.ink = DrawString::Ink::color;
      out.color = {kRgb[i][0], kRgb[i][1], kRgb[i][2], 1};
      return true;
    }
  }
  if (w[0] == '#' && (w.size() == 4 || w.size() == 7 || w.size() == 9)) {
    float c[4] = {1, 1, 1, 1};
    const bool shortform = w.size() == 4;
    const size_t parts = shortform ? 3 : (w.size() - 1) / 2;
    for (size_t i = 0; i < parts; ++i) {
      int v = 0;
      if (shortform) {
        const int d = hex_digit(w[1 + i]);
        if (d < 0) return false;
        v = d * 17;
      } else {
        const int hi = hex_digit(w[1 + 2 * i]), lo = hex_digit(w[2 + 2 * i]);
        if (hi < 0 || lo < 0) return false;
        v = hi * 16 + lo;
      }
      c[i] = static_cast<float>(v) / 255.0f;
    }
    out.ink = DrawString::Ink::color;
    out.color = {c[0], c[1], c[2], c[3]};
    return true;
  }
  return false;
}

void add_draw_commands(console::Console& con, DrawStrings& strings) {
  con.add_command(
      "draw", "draw <name> <x> <y> [colour]: a setting's or a macro's value kept on screen",
      [&strings](console::Console& c, const console::Args& a) {
        if (a.size() == 1) {
          if (strings.list.empty()) {
            c.print("No draw strings registered.");
            return;
          }
          c.print("Name               X    Y");
          c.print("--------------- ---- ----");
          for (const DrawString& d : strings.list)
            c.print(padded(d.name, 15, false) + " " + padded(std::to_string(d.x), 4, true) + " " +
                    padded(std::to_string(d.y), 4, true));
          return;
        }
        if (a.size() < 4) {
          c.print("Usage: " + a[0] + " <name> <x> <y> [color]");
          return;
        }
        DrawString d{.name = console::lower(a[1]),
                     .x = std::atoi(a[2].c_str()),
                     .y = std::atoi(a[3].c_str())};
        if (a.size() > 4 && !draw_color(a[4], d)) {
          c.print("Unknown color '" + a[4] + "'");
          return;
        }
        const std::vector<std::string> macros = c.macro_names();
        const bool macro = std::find(macros.begin(), macros.end(), d.name) != macros.end();
        if (!macro && c.cvars().find(d.name) == nullptr)
          c.cvars().set(d.name, "", console::Source::local, true);
        for (DrawString& old : strings.list) {
          if (old.name == d.name) {
            old = d;
            return;
          }
        }
        strings.list.push_back(d);
      });
  con.add_command("undraw", "undraw <name>|all: a draw string taken off the screen",
                  [&strings](console::Console& c, const console::Args& a) {
                    if (a.size() != 2) {
                      c.print("Usage: " + a[0] + " <name>");
                      return;
                    }
                    if (strings.list.empty()) {
                      c.print("No draw strings registered.");
                      return;
                    }
                    const std::string name = console::lower(a[1]);
                    if (name == "all") {
                      strings.list.clear();
                      c.print("Deleted all draw strings.");
                      return;
                    }
                    const auto it =
                        std::find_if(strings.list.begin(), strings.list.end(),
                                     [&name](const DrawString& d) { return d.name == name; });
                    if (it == strings.list.end()) {
                      c.print("Draw string '" + a[1] + "' not found.");
                      return;
                    }
                    strings.list.erase(it);
                  });
}

void draw_strings(ui::DrawList& dl, const Frame& frame, const Style& style,
                  const DrawStrings& strings, const console::Console& con) {
  const float unit = frame.px(1);
  const float size = frame.px(8);
  const float width = static_cast<float>(frame.width), height = static_cast<float>(frame.height);
  for (const DrawString& d : strings.list) {
    const std::string value = con.expand_text("$" + d.name);
    if (value.empty()) continue;
    float x = static_cast<float>(d.x) * unit;
    float y = static_cast<float>(d.y) * unit;
    ui::Align align = ui::Align::left;
    if (d.x < 0) {
      x += width + unit;
      align = ui::Align::right;
    }
    if (d.y < 0) y += height - size + unit;
    const ui::Color ink = d.ink == DrawString::Ink::color ? d.color
                          : d.ink == DrawString::Ink::alt ? style.caption
                                                          : style.ink;
    draw_shadowed(dl, style.caption_face, size, x, y + size, value, ink, style.shadow,
                  frame.px(style.shadow_drop) * 0.5f, align);
  }
}

}  // namespace ge::hud
