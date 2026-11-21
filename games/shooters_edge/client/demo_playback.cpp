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

#include "shooters_edge/client/args.hpp"
#include "shooters_edge/client/cvar_layers.hpp"
#include "shooters_edge/client/demo_highlights.hpp"
#include "shooters_edge/hud/layout.hpp"

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

void add_playback_commands(ge::console::Console& console, DemoPlayback& p, Timedemo& bench) {
  {
    ge::console::CvarLayer shell(console.cvars(), kLayerShell);
    bind_record_cvars(console, p.record);
  }
  add_video_cvars(console, &p.video);  // the engine's demo video (video.cfg)
  {
    ge::console::CvarLayer classic(console.cvars(), kLayerClassic);
    console.cvars().bind({.name = "cl_democlock",
                          .fallback = "0",
                          .flags = ge::console::kArchive,
                          .help = "A clock over a demo you watch.",
                          .kind = ge::console::CvarKind::integer,
                          .min = 0,
                          .max = 2,
                          .values = "0: none (the default); 1: the time since the demo began; 2: "
                                    "the recording's own clock"},
                         &p.clock);
    console.cvars().bind({.name = "cl_democlock_x",
                          .fallback = "0",
                          .flags = ge::console::kArchive,
                          .help = "Where the demo clock sits across the screen, in characters.",
                          .kind = ge::console::CvarKind::integer,
                          .min = -100,
                          .max = 100,
                          .values = "0: at the left edge (the default); a number: that many "
                                    "characters in from the left, or from the right when below 0"},
                         &p.clock_x);
    console.cvars().bind({.name = "cl_democlock_y",
                          .fallback = "-2",
                          .flags = ge::console::kArchive,
                          .help = "Where the demo clock sits up and down, in lines.",
                          .kind = ge::console::CvarKind::integer,
                          .min = -100,
                          .max = 100,
                          .values = "-2: two lines above the status bar (the default); a number: "
                                    "that many lines down from the top, or up from the status bar "
                                    "when below 0"},
                         &p.clock_y);
    console.cvars().bind({.name = "cl_startupdemo",
                          .fallback = "",
                          .flags = ge::console::kArchive,
                          .help = "A demo played once when the game starts, when you asked for no "
                                  "map, demo or server.",
                          .kind = ge::console::CvarKind::text,
                          .values = "a demo: its name, as playdemo takes it; empty: none (the "
                                    "default)"},
                         &p.startup);
  }
  ge::console::CvarLayer shooter(console.cvars(), kLayerShooter);
  console.cvars().bind({.name = "cl_demowait",
                        .fallback = "0",
                        .flags = ge::console::kArchive,
                        .help = "Whether a demo you watch holds on its last frame when it ends.",
                        .kind = ge::console::CvarKind::boolean,
                        .values = "0: the demo ends (the default); 1: it holds, paused, on its "
                                  "last frame until you seek back or leave"},
                       &p.hold_end);
  ge::shooter::add_demo_transport(
      console, p,
      {.tracked = [&bench](int track) { bench.demo.track = track; },
       .highlights = [&p](bool all) { return highlight_lines(p, all); }});
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
  t.smooth = p.lerp != 0;
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
  const TimedemoStep step = playdemo_step(t, wall_dt_s, p.paused ? 0.0 : p.speed * p.rate, error);
  if (step != TimedemoStep::end || p.hold_end == 0) return step;
  if (!p.paused) p.shown_until = now_s + kBarShowS;  // held: the bar shows the way back
  p.paused = true;
  return TimedemoStep::frame;
}

std::string demo_clock_text(double s) {
  const long long whole = s > 0 ? static_cast<long long>(s) : 0;
  const auto two = [](long long n) {
    return std::string(1, static_cast<char>('0' + n / 10)) + static_cast<char>('0' + n % 10);
  };
  return two(whole / 3600 % 100) + ":" + two(whole / 60 % 60) + ":" + two(whole % 60);
}

void playback_draw(gu::DrawList& overlay, const DemoPlayback& p, const Timedemo& t, int w, int h,
                   double now_s) {
  if (p.clock != 0) {
    const ge::hud::Style style = hud::hud_style();
    const ge::hud::Frame frame = ge::hud::frame_for(w, h, style);
    const float size = frame.px(style.caption_size);
    const float cell = size * 1.3f;
    const std::string words = demo_clock_text(p.clock == 2 ? t.demo.state.time : t.timeline.t);
    const float right = static_cast<float>(w) - frame.safe.x;
    const float x = p.clock_x >= 0 ? frame.safe.x + static_cast<float>(p.clock_x) * cell
                                   : right + static_cast<float>(p.clock_x) * cell;
    const float hud_top = static_cast<float>(h) - p.bar_bottom_px;
    const float top = p.clock_y >= 0 ? frame.safe.y + static_cast<float>(p.clock_y) * cell
                                     : hud_top + static_cast<float>(p.clock_y) * cell;
    ge::hud::draw_shadowed(overlay, style.caption_face, size, x, top + size, words, style.ink,
                           style.shadow, frame.px(style.shadow_drop) * 0.5f,
                           p.clock_x >= 0 ? gu::Align::left : gu::Align::right, 1);
  }
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

void add_demo_commands(ge::console::Console& console, std::string& ask, bool& paced) {
  using Con = ge::console::Console;
  console.add_command("timedemo", "timedemo <demo>: play a demo flat out and report its fps",
                      [&ask](Con& c, const ge::console::Args& a) {
                        if (a.size() < 2)
                          c.print("timedemo: name a demo (timedemo demo1)");
                        else
                          ask = expand_home(a[1]);
                      });
  console.add_command("playdemo", "playdemo <demo>: play a demo at its own pace",
                      [&ask, &paced](Con& c, const ge::console::Args& a) {
                        if (a.size() < 2) {
                          c.print("playdemo: name a demo (playdemo demo1, playdemo match.mvd)");
                        } else {
                          ask = expand_home(a[1]);
                          paced = true;
                        }
                      });
}

}  // namespace shooters_edge::client
