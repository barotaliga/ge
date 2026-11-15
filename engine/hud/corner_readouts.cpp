// The speed and the clock in a corner (corner_readouts.hpp).
//
// ge:summary The corner readouts' cvars, words and plates
// ge:keywords show_speed scr_clock corner-readouts
// ge:skill ui
// ge:test tests/hud_corner_readouts_test.cpp
#include "hud/corner_readouts.hpp"

#include <algorithm>
#include <string>

#include "core/num_text.hpp"

namespace ge::hud {

namespace gc = ge::console;

void add_readout_cvars(gc::Cvars& cvars, ReadoutSettings& s) {
  const std::string speed = std::to_string(s.speed), clock = std::to_string(s.clock);
  cvars.bind({.name = "show_speed",
              .fallback = speed,
              .flags = gc::kArchive,
              .help = "Whether your speed is shown in a corner.",
              .kind = gc::CvarKind::integer,
              .min = 0,
              .max = 2,
              .values = "0: off; 1: your speed over the ground; 2: your whole speed, rising and "
                        "falling too"},
             &s.speed);
  cvars.bind({.name = "scr_clock",
              .fallback = clock,
              .flags = gc::kArchive,
              .help = "Whether a clock is shown in a corner.",
              .kind = gc::CvarKind::integer,
              .min = 0,
              .max = 2,
              .values = "0: off; 1: the time of day; 2: the game's time"},
             &s.clock);
  cvars.bind({.name = "r_netgraph",
              .fallback = std::to_string(s.net),
              .flags = gc::kArchive,
              .help = "Whether a graph of the packets from the server is shown in a corner.",
              .kind = gc::CvarKind::integer,
              .min = 0,
              .max = 1,
              .values = "0: off; 1: each packet's round trip, the lost ones in red, with the "
                        "ping and the loss"},
             &s.net);
}

std::string speed_words(double units_a_second) {
  return core::num::to_fixed(std::max(units_a_second, 0.0), 0);
}

namespace {

std::string two(int32_t v) {
  const int32_t c = std::clamp(v, 0, 99);
  return std::string(1, static_cast<char>('0' + c / 10)) + static_cast<char>('0' + c % 10);
}

}  // namespace

std::string time_of_day_words(int32_t hour, int32_t minute) {
  return two(std::clamp(hour, 0, 23)) + ":" + two(std::clamp(minute, 0, 59));
}

std::string game_time_words(double seconds) {
  const auto whole = static_cast<int64_t>(std::max(seconds, 0.0));
  const int64_t h = whole / 3600, m = whole / 60 % 60, sec = whole % 60;
  const std::string tail = two(static_cast<int32_t>(sec));
  if (h > 0) return std::to_string(h) + ":" + two(static_cast<int32_t>(m)) + ":" + tail;
  return std::to_string(m) + ":" + tail;
}

float draw_corner_readouts(ui::DrawList& dl, const Frame& frame, const Style& style,
                           const ReadoutSettings& s, const Readings& r, float top) {
  const float size = frame.px(style.caption_size);
  const float pad = frame.px(4);
  const float gap = frame.px(4);
  float y = top > 0 ? top + gap : 0;
  const auto plate = [&](const std::string& words) {
    const float w = text_width(style.caption_face, size, words, 1) + 2 * pad;
    const float h = size * 1.3f + 2 * pad;
    const ui::Rect box = place(frame, Anchor::top_right, w, h, 0, y);
    dl.rect(box, style.plate, frame.px(3));
    draw_shadowed(dl, style.caption_face, size, box.right() - pad, box.y + pad + size, words,
                  style.ink, style.shadow, frame.px(style.shadow_drop) * 0.5f, ui::Align::right, 1);
    y += h + gap;
  };
  const float start = y;
  if (s.speed == 1 || s.speed == 2) plate(speed_words(s.speed == 2 ? r.speed : r.ground_speed));
  if (s.clock == 1) plate(time_of_day_words(r.hour, r.minute));
  if (s.clock == 2) plate(game_time_words(r.game_s));
  return y - start;
}

}  // namespace ge::hud
