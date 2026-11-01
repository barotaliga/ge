// A demo watched like a video (demo_playback.hpp): the commands, the keys and the bar.
//
// ge:summary The demo's playback controls: console commands, keys, the bar's pointer, the seek
// ge:keywords demo playback keys pointer seek skip jump speed pause markers parse_clock
// ge:skill shooter replay ui
// ge:test tests/shooters_edge_demo_highlights_test.cpp
#include "shooters_edge/client/demo_playback.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "shooters_edge/client/demo_highlights.hpp"

namespace shooters_edge::client {

namespace {

namespace gr = ge::replay;
namespace gu = ge::ui;
// The keys by their USB HID scancode (the platform's key numbers).
constexpr int kKeyJ = 13, kKeyK = 14, kKeyL = 15, kKeyN = 17, kKeyP = 19, kKeySpace = 44;
constexpr int kKeyHome = 74, kKeyPageUp = 75, kKeyEnd = 77, kKeyPageDown = 78, kKeyRight = 79;
constexpr int kKeyLeft = 80, kKeyDown = 81, kKeyUp = 82;

gu::Color color_of(uint32_t rgba) {
  return {static_cast<float>(rgba >> 24 & 0xff) / 255.0f,
          static_cast<float>(rgba >> 16 & 0xff) / 255.0f,
          static_cast<float>(rgba >> 8 & 0xff) / 255.0f, static_cast<float>(rgba & 0xff) / 255.0f};
}

double length_of(const DemoPlayback& p, const Timedemo& t) {
  return std::max(p.scan.length_s, t.timeline.t);
}

// The highlights and markers for the player the view rides, when that changed.
void mark(DemoPlayback& p, const Timedemo& t) {
  const int subject = t.demo.state.format == DemoFormat::single ? -1 : t.demo.state.player;
  if (subject == p.marked_for) return;
  p.marked_for = subject;
  p.highlights = demo_highlights(p.scan, subject);
  p.markers.clear();
  for (const gr::Highlight& h : p.highlights.list) {
    const gr::HighlightKind* k = gr::kind_of(demo_kinds(), h.kind);
    p.markers.push_back({h.at_s, h.from_s, h.to_s, color_of(k ? k->rgba : ~0u), h.text});
  }
}

// The next speed along kDemoSpeeds from `now` (dir > 0 faster), held at the ends.
double step_speed(double now, int dir) {
  if (dir > 0) {
    for (double s : kDemoSpeeds)
      if (s > now + 1e-9) return s;
    return kDemoSpeeds.back();
  }
  for (size_t i = kDemoSpeeds.size(); i-- > 0;)
    if (kDemoSpeeds[i] < now - 1e-9) return kDemoSpeeds[i];
  return kDemoSpeeds.front();
}

}  // namespace

float hud_inset(std::span<const ge::hud::Placed> placed, int height) {
  const float h = static_cast<float>(height);
  float top = h;
  for (const ge::hud::Placed& b : placed)
    if (b.rect.y > h * 2 / 3 && b.rect.h > 0) top = std::min(top, b.rect.y);
  return h - top;
}

double parse_clock(const std::string& text) {
  if (text.empty()) return -1;
  double total = 0;
  size_t at = 0;
  int parts = 0;
  while (at <= text.size()) {
    const size_t colon = text.find(':', at);
    const std::string part = text.substr(at, colon == std::string::npos ? colon : colon - at);
    char* end = nullptr;
    const double v = std::strtod(part.c_str(), &end);
    if (part.empty() || end == part.c_str() || *end != '\0' || v < 0) return -1;
    total = total * 60 + v;
    parts++;
    if (colon == std::string::npos) break;
    at = colon + 1;
  }
  return parts > 3 ? -1 : total;
}

void add_playback_commands(ge::console::Console& console, DemoPlayback& p, Timedemo& bench) {
  using Con = ge::console::Console;
  using CArgs = ge::console::Args;
  console.add_command("cl_demospeed", "cl_demospeed <x>: a played demo's speed, 0..20 (0 pauses)",
                      [&p](Con& c, const CArgs& a) {
                        if (a.size() > 1) p.speed = std::clamp(std::atof(a[1].c_str()), 0.0, 20.0);
                        c.print("cl_demospeed " + std::to_string(p.speed));
                      });
  console.add_command("demo_track", "demo_track <slot>: the player a multiview demo's view rides",
                      [&p, &bench](Con& c, const CArgs& a) {
                        if (a.size() > 1) p.track = std::atoi(a[1].c_str());
                        bench.demo.track = p.track;
                        c.print("demo_track " + std::to_string(p.track) +
                                (p.track < 0 ? " (the lowest slot playing)" : ""));
                      });
  console.add_command("demo_pause", "demo_pause: pause or play the demo playing",
                      [&p](Con& c, const CArgs&) {
                        p.paused = !p.paused;
                        c.print(p.paused ? "demo paused" : "demo playing");
                      });
  console.add_command("demo_seek", "demo_seek <time>: the demo playing to a time (s, m:ss)",
                      [&p](Con& c, const CArgs& a) {
                        const double at = parse_clock(a[1]);
                        if (at < 0)
                          c.print("demo_seek: name a time (demo_seek 83, demo_seek 1:23)");
                        else
                          p.seek_ask = at;
                      });
  console.add_command("demo_skip", "demo_skip <s>: the demo playing on (or back, negative) by s",
                      [&p](Con& c, const CArgs& a) {
                        if (a.size() < 2)
                          c.print("demo_skip: name the seconds (demo_skip 15, demo_skip -5)");
                        else
                          p.skip_ask += std::atof(a[1].c_str());
                      });
  console.add_command("demo_next", "demo_next: the next highlight of the demo playing",
                      [&p](Con&, const CArgs&) { p.jump_ask++; });
  console.add_command("demo_prev", "demo_prev: the last highlight before now",
                      [&p](Con&, const CArgs&) { p.jump_ask--; });
  console.add_command("demo_highlights",
                      "demo_highlights [all]: list the demo's highlights (the tracked player's)",
                      [&p](Con& c, const CArgs& a) {
                        for (const std::string& line : highlight_lines(p, a[1] == "all"))
                          c.print(line);
                      });
  console.cvars().bind({.name = "demo_skip_forward",
                        .fallback = "15",
                        .help = "how far forward Right (or L) moves a demo, s",
                        .kind = ge::console::CvarKind::number,
                        .min = 0,
                        .max = 3600},
                       &p.skip_forward_s);
  console.cvars().bind({.name = "demo_skip_back",
                        .fallback = "5",
                        .help = "how far back Left (or J) moves a demo, s",
                        .kind = ge::console::CvarKind::number,
                        .min = 0,
                        .max = 3600},
                       &p.skip_back_s);
  console.cvars().bind({.name = "demo_bar",
                        .fallback = "1",
                        .help = "a demo's play bar: 0 hidden, 1 shown after the pointer or a key, "
                                "2 always",
                        .kind = ge::console::CvarKind::integer,
                        .min = 0,
                        .max = 2},
                       &p.bar);
}

bool playback_begin(DemoPlayback& p, const Timedemo& t, std::string& error) {
  p.paused = false;
  p.seek_ask = -1;
  p.skip_ask = 0;
  p.jump_ask = 0;
  p.pointer = {};
  p.marked_for = -2;
  p.highlights = {};
  p.markers.clear();
  const bool ok = scan_demo(t.start, p.scan, error);
  if (!ok) p.scan = DemoScan{};
  return ok;
}

TimedemoStep playback_step(DemoPlayback& p, Timedemo& t, const PlaybackInput& in, int w, int h,
                           double wall_dt_s, double now_s, std::vector<std::string>& say,
                           std::string& error) {
  const float x = in.x, y = in.y;
  if (x != p.pointer_x || y != p.pointer_y) p.shown_until = now_s + kBarShowS;
  p.pointer_x = x;
  p.pointer_y = y;
  const double length = length_of(p, t);
  const bool bar_on = p.bar == 2 || (p.bar == 1 && (now_s <= p.shown_until || p.paused));
  if (bar_on || p.pointer.dragging) {
    const gu::PlayBarLayout l = gu::play_bar_layout(w, h, p.bar_bottom_px);
    const gu::PlayBarAct act = gu::play_bar_pointer(p.pointer, l, length, x, y, in.button);
    if (act == gu::PlayBarAct::toggle) p.paused = !p.paused;
    if (act == gu::PlayBarAct::seek) p.seek_ask = p.pointer.at_s;
  } else {
    p.pointer.was_down = in.button;
  }
  // The keys.
  bool key = false;
  const auto tap = [&](int k) {
    const bool on = in.tapped(k);
    key = key || on;
    return on;
  };
  if (tap(kKeySpace) || tap(kKeyK)) p.paused = !p.paused;
  if (tap(kKeyRight) || tap(kKeyL)) p.skip_ask += p.skip_forward_s;
  if (tap(kKeyLeft) || tap(kKeyJ)) p.skip_ask -= p.skip_back_s;
  if (tap(kKeyUp)) p.speed = step_speed(p.speed, 1);
  if (tap(kKeyDown)) p.speed = step_speed(p.speed, -1);
  if (tap(kKeyPageDown) || tap(kKeyN)) p.jump_ask++;
  if (tap(kKeyPageUp) || tap(kKeyP)) p.jump_ask--;
  if (tap(kKeyHome)) p.seek_ask = 0;
  if (tap(kKeyEnd)) p.seek_ask = std::max(0.0, length - 1);
  if (key) p.shown_until = now_s + kBarShowS;

  mark(p, t);
  // A skip and a jump become a seek from where the playhead stands.
  const double here = t.timeline.t;
  if (p.skip_ask != 0) {
    p.seek_ask = std::max(0.0, (p.seek_ask >= 0 ? p.seek_ask : here) + p.skip_ask);
    p.skip_ask = 0;
  }
  while (p.jump_ask != 0) {
    const int dir = p.jump_ask > 0 ? 1 : -1;
    const int i = gr::next_highlight(p.highlights.list, p.seek_ask >= 0 ? p.seek_ask : here, dir);
    p.jump_ask -= dir;
    if (i < 0) {
      say.push_back(dir > 0 ? "demo: no highlight after this" : "demo: no highlight before this");
      p.jump_ask = 0;
      break;
    }
    const gr::Highlight& hl = p.highlights.list[static_cast<size_t>(i)];
    p.seek_ask = hl.from_s;
    say.push_back("demo: " + gu::play_clock(hl.at_s) + " " + hl.text);
  }
  if (p.seek_ask >= 0) {
    const double to = std::clamp(p.seek_ask, 0.0, std::max(0.0, length - 0.5));
    p.seek_ask = -1;
    p.shown_until = now_s + kBarShowS;
    return demo_seek(t, to, error);
  }
  return playdemo_step(t, wall_dt_s, p.paused ? 0.0 : p.speed, error);
}

void playback_draw(gu::DrawList& overlay, const DemoPlayback& p, const Timedemo& t, int w, int h,
                   double now_s) {
  if (p.bar == 0) return;
  if (p.bar == 1 && now_s > p.shown_until && !p.paused && !p.pointer.dragging) return;
  gu::PlayBarView v;
  v.length_s = length_of(p, t);
  v.position_s = t.timeline.t;
  v.playing = !p.paused;
  v.speed = p.speed;
  v.markers = p.markers;
  v.pointer_x = p.pointer_x;
  v.pointer_y = p.pointer_y;
  v.drag_s = p.pointer.dragging ? p.pointer.at_s : -1;
  const DemoState& s = t.demo.state;
  const size_t n = p.highlights.list.size();
  v.title =
      (s.format == DemoFormat::single ? s.map : player_name(s.names, s.player) + "  ·  " + s.map) +
      "  ·  " + std::to_string(n) + (n == 1 ? " highlight" : " highlights");
  gu::draw_play_bar(overlay, gu::play_bar_layout(w, h, p.bar_bottom_px), v);
}

std::vector<std::string> highlight_lines(const DemoPlayback& p, bool all) {
  std::vector<std::string> out;
  const gr::Highlights everyone = all ? demo_highlights(p.scan, -1) : gr::Highlights{};
  const std::vector<gr::Highlight>& list = all ? everyone.list : p.highlights.list;
  out.reserve(list.size() + 1);
  for (size_t i = 0; i < list.size(); i++) {  // numbered as -demovideo's --highlight=N takes them
    const gr::Highlight& h = list[i];
    out.push_back((all ? std::string() : "#" + std::to_string(i + 1) + "  ") +
                  gu::play_clock(h.at_s) + "  " + player_name(p.scan.names, h.subject) + "  " +
                  h.text);
  }
  if (out.empty()) out.push_back("demo_highlights: none found");
  return out;
}

}  // namespace shooters_edge::client
