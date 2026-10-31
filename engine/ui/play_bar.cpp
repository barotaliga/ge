// The play bar (play_bar.hpp): its layout, drawing and the pointer's drag.
//
// ge:summary The play bar's layout, its drawing into a draw list, the pointer's drag and seek
// ge:keywords play-bar layout draw marker tooltip playhead drag seek clock
// ge:skill ui replay
// ge:test tests/ui_play_bar_test.cpp
#include "ui/play_bar.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "ui/font.hpp"

namespace ge::ui {

namespace {

constexpr Color kPlayed = hex(0xff4e45);
constexpr Color kTrack = hex(0xffffff, 0.28f);
constexpr Color kInk = hex(0xffffff);
constexpr Color kDim = hex(0xffffff, 0.75f);

float x_of(const PlayBarLayout& l, double t, double length_s) {
  if (!(length_s > 0)) return l.track.x;
  const double f = std::clamp(t / length_s, 0.0, 1.0);
  return l.track.x + static_cast<float>(f) * l.track.w;
}

std::string speed_text(double speed) {
  char b[32];
  std::snprintf(b, sizeof b, "%gx", speed);
  return b;
}

}  // namespace

PlayBarLayout play_bar_layout(int width, int height, float bottom_px) {
  PlayBarLayout l;
  const float w = static_cast<float>(width);
  const float h = std::max(static_cast<float>(height) - std::max(bottom_px, 0.0f),
                           static_cast<float>(height) * 0.25f);
  const float s = std::max(0.5f, static_cast<float>(height) / 720.0f);
  l.scale = s;
  l.panel = {0, h - 84 * s, w, 84 * s};
  l.track = {24 * s, h - 60 * s, std::max(1.0f, w - 48 * s), 5 * s};
  l.hit = {l.track.x - 6 * s, l.track.y - 12 * s, l.track.w + 12 * s, l.track.h + 24 * s};
  l.button = {22 * s, h - 46 * s, 30 * s, 30 * s};
  l.text_y = h - 23 * s;
  return l;
}

double play_bar_time(const PlayBarLayout& l, float x, double length_s) {
  if (!(length_s > 0) || !(l.track.w > 0)) return 0;
  const double f = std::clamp(static_cast<double>((x - l.track.x) / l.track.w), 0.0, 1.0);
  return f * length_s;
}

int play_bar_marker_at(const PlayBarLayout& l, std::span<const PlayBarMarker> markers,
                       double length_s, float x, float y) {
  if (!l.hit.contains(x, y)) return -1;
  const float reach = 7 * l.scale;
  int best = -1;
  float best_d = reach;
  for (size_t i = 0; i < markers.size(); i++) {
    const float d = std::fabs(x_of(l, markers[i].at_s, length_s) - x);
    if (d <= best_d) {
      best_d = d;
      best = static_cast<int>(i);
    }
  }
  return best;
}

std::string play_clock(double s) {
  const auto total = static_cast<long long>(std::floor(std::max(0.0, s) + 1e-3));
  char b[32];
  if (total >= 3600)
    std::snprintf(b, sizeof b, "%lld:%02lld:%02lld", total / 3600, total / 60 % 60, total % 60);
  else
    std::snprintf(b, sizeof b, "%lld:%02lld", total / 60, total % 60);
  return b;
}

void draw_play_bar(DrawList& list, const PlayBarLayout& l, const PlayBarView& v) {
  const float s = l.scale;
  const Font& font = ui_font();
  // The band, darker towards the bottom edge.
  list.rect({l.panel.x, l.panel.y, l.panel.w, l.panel.h * 0.4f}, hex(0x000000, 0.25f));
  list.rect({l.panel.x, l.panel.y + l.panel.h * 0.4f, l.panel.w, l.panel.h * 0.6f},
            hex(0x000000, 0.5f));
  const bool hover = v.pointer_x >= 0 && l.hit.contains(v.pointer_x, v.pointer_y);
  Rect track = l.track;
  if (hover || v.drag_s >= 0) {  // the track thickens under the pointer, as a player's does
    track.y -= 1.5f * s;
    track.h += 3 * s;
  }
  const float r = track.h / 2;
  list.rect(track, kTrack, r);
  const double head_s = v.drag_s >= 0 ? v.drag_s : v.position_s;
  const float head = x_of(l, head_s, v.length_s);
  list.rect({track.x, track.y, std::max(head - track.x, 0.0f), track.h}, kPlayed, r);
  // The window of the highlight under the pointer, shaded in its colour.
  const int over = v.pointer_x >= 0
                       ? play_bar_marker_at(l, v.markers, v.length_s, v.pointer_x, v.pointer_y)
                       : -1;
  if (over >= 0) {
    const PlayBarMarker& m = v.markers[static_cast<size_t>(over)];
    if (m.to_s > m.from_s) {
      const float a = x_of(l, m.from_s, v.length_s), b = x_of(l, m.to_s, v.length_s);
      Color c = m.color;
      c.a *= 0.6f;
      list.rect({a, track.y, std::max(b - a, 1.0f), track.h}, c, r);
    }
  }
  // The markers: a tick in the highlight's colour standing through the track.
  for (size_t i = 0; i < v.markers.size(); i++) {
    const PlayBarMarker& m = v.markers[i];
    const float x = x_of(l, m.at_s, v.length_s);
    const bool big = static_cast<int>(i) == over;
    const float mw = (big ? 7 : 5) * s, mh = (big ? 20 : 15) * s;
    const float cy = track.y + track.h / 2;
    list.rect({x - mw / 2 - s, cy - mh / 2 - s, mw + 2 * s, mh + 2 * s}, hex(0x000000, 0.6f),
              2 * s);
    list.rect({x - mw / 2, cy - mh / 2, mw, mh}, m.color, 1.5f * s);
  }
  // The playhead.
  const float hr = (hover || v.drag_s >= 0 ? 8 : 6.5f) * s;
  const float cy = track.y + track.h / 2;
  list.rect({head - hr, cy - hr, 2 * hr, 2 * hr}, kInk, hr);
  // The tooltip: the marker's label and time, or the time under the pointer.
  if (over >= 0 || hover || v.drag_s >= 0) {
    std::string tip;
    double at = 0;
    if (over >= 0) {
      const PlayBarMarker& m = v.markers[static_cast<size_t>(over)];
      at = m.at_s;
      tip = m.label.empty() ? play_clock(at) : m.label + "   " + play_clock(at);
    } else {
      at = v.drag_s >= 0 ? v.drag_s : play_bar_time(l, v.pointer_x, v.length_s);
      tip = play_clock(at);
    }
    const float size = 19 * s;
    const float tw = font.advance(tip, size) + 16 * s;
    const float tx = std::clamp(x_of(l, at, v.length_s) - tw / 2, 4 * s,
                                static_cast<float>(list.width()) - tw - 4 * s);
    const float ty = track.y - 40 * s;
    list.rect({tx, ty, tw, 28 * s}, hex(0x101010, 0.88f), 5 * s);
    list.text(font, size, tx + tw / 2, ty + 21 * s, tip, kInk, Align::center);
  }
  // The row under the track: play or pause, the clock, the speed, the title.
  const Rect& b = l.button;
  if (v.playing) {  // pause: two bars
    list.rect({b.x + 7 * s, b.y + 6 * s, 5.5f * s, 18 * s}, kInk, 1 * s);
    list.rect({b.x + 17.5f * s, b.y + 6 * s, 5.5f * s, 18 * s}, kInk, 1 * s);
  } else {  // play: a triangle
    const Vec2 tri[3] = {
        {b.x + 8 * s, b.y + 5 * s}, {b.x + 25 * s, b.y + 15 * s}, {b.x + 8 * s, b.y + 25 * s}};
    list.polygon(tri, solid(kInk));
  }
  const float size = 21 * s;
  float x = b.right() + 12 * s;
  x +=
      list.text(font, size, x, l.text_y, play_clock(head_s) + " / " + play_clock(v.length_s), kInk);
  if (v.speed != 1) {
    x += 14 * s;
    const std::string sp = speed_text(v.speed);
    const float pw = font.advance(sp, 17 * s) + 12 * s;
    list.rect({x, l.text_y - 17 * s, pw, 22 * s}, hex(0xffffff, 0.18f), 4 * s);
    list.text(font, 17 * s, x + pw / 2, l.text_y - 1 * s, sp, kInk, Align::center);
  }
  if (!v.title.empty())
    list.text(font, 18 * s, l.track.right(), l.text_y, v.title, kDim, Align::right);
}

PlayBarAct play_bar_pointer(PlayBarPointer& p, const PlayBarLayout& l, double length_s, float x,
                            float y, bool down) {
  const bool pressed = down && !p.was_down;
  const bool released = !down && p.was_down;
  p.was_down = down;
  if (p.dragging) {
    p.at_s = play_bar_time(l, x, length_s);
    if (released || !down) {
      p.dragging = false;
      return PlayBarAct::seek;
    }
    return PlayBarAct::drag;
  }
  if (pressed && l.hit.contains(x, y)) {
    p.dragging = true;
    p.at_s = play_bar_time(l, x, length_s);
    return PlayBarAct::drag;
  }
  if (pressed && l.button.contains(x, y)) return PlayBarAct::toggle;
  return PlayBarAct::none;
}

}  // namespace ge::ui
