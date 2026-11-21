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
#include <utility>

#include "shooters_edge/client/args.hpp"
#include "shooters_edge/client/cvar_layers.hpp"
#include "shooters_edge/client/demo_highlights.hpp"
#include "shooters_edge/client/mvd_info.hpp"
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

// Autotrack and the top fragger: the slot the view rides now, into `t`.
void follow(DemoPlayback& p, Timedemo& t, std::vector<std::string>& say) {
  const DemoState& s = t.demo.state;
  if ((p.autotrack == 0 && p.hightrack == 0) || s.format == DemoFormat::single) return;
  if (p.autotrack != 0) {
    if (p.plan_state == 0) {
      std::string why;
      p.plan_state = autotrack_plan(t.start, p.scan, {}, p.plan, why) ? 1 : -1;
      if (p.plan_state < 0) say.push_back("autotrack: " + why);
    }
    const int slot = p.plan_state > 0 ? autotrack_at(p.plan, t.timeline.t) : -1;
    if (slot >= 0) t.demo.track = slot;
    if (p.plan_state > 0) return;
  }
  if (p.hightrack == 0) return;
  const int slot = high_track_slot(s, s.player);
  if (slot >= 0) t.demo.track = slot;
}

}  // namespace

int high_track_slot(const DemoState& s, int riding) {
  const auto playing = [&s](int i) {
    const size_t k = static_cast<size_t>(i);
    return i >= 0 && k < s.players.size() && k < s.frags.size() && !s.names[k].empty() &&
           !s.spectator[k];
  };
  int best = -1;
  for (int i = 0; i < static_cast<int>(std::min(s.players.size(), s.frags.size())); i++)
    if (playing(i) &&
        (best < 0 || s.frags[static_cast<size_t>(i)] > s.frags[static_cast<size_t>(best)]))
      best = i;
  if (best < 0) return -1;
  if (playing(riding) && s.frags[static_cast<size_t>(riding)] >= s.frags[static_cast<size_t>(best)])
    return riding;
  return best;
}

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
    console.cvars().bind({.name = "cam_thirdperson",
                          .fallback = "0",
                          .flags = ge::console::kArchive,
                          .help = "Whether the player you watch is seen from behind.",
                          .kind = ge::console::CvarKind::boolean,
                          .values = "0: through his eyes (the default); 1: from behind him"},
                         &p.third_person);
    console.cvars().bind({.name = "cam_dist",
                          .fallback = "100",
                          .flags = ge::console::kArchive,
                          .help = "How far behind the player you watch the camera sits with "
                                  "cam_thirdperson 1, in units.",
                          .min = 0,
                          .max = 1000,
                          .values = "100: the default; 0..1000: that many units"},
                         &p.behind);
    p.dist_cvar = console.cvars().find("cam_dist");
    p.console = &console;
    console.cvars().bind({.name = "mvd_info",
                          .fallback = "0",
                          .flags = ge::console::kArchive,
                          .help = "Whether a table of every player shows while you watch a "
                                  "multiview demo.",
                          .kind = ge::console::CvarKind::boolean,
                          .values = "0: hidden (the default); 1: shown"},
                         &p.info);
    console.cvars().bind({.name = "mvd_info_setup",
                          .fallback = kMvdInfoSetup,
                          .flags = ge::console::kArchive,
                          .help = "What each row of the players' table says: %n name, %h health, "
                                  "%a armour, %f frags, %l place, %w weapon in hand, %W best "
                                  "weapon, %v rating; a number after % pads, %-12n to the left.",
                          .kind = ge::console::CvarKind::text,
                          .values = "a row's text with its codes"},
                         &p.info_setup);
    console.cvars().bind({.name = "mvd_info_show_header",
                          .fallback = "0",
                          .flags = ge::console::kArchive,
                          .help = "Whether the players' table has a header row.",
                          .kind = ge::console::CvarKind::boolean,
                          .values = "0: none (the default); 1: a header naming each column"},
                         &p.info_header);
    console.cvars().bind({.name = "mvd_info_x",
                          .fallback = "0",
                          .flags = ge::console::kArchive,
                          .help = "Where the players' table sits across, in cells from the left "
                                  "(from the right when negative).",
                          .min = -200,
                          .max = 200,
                          .values = "0: at the left (the default); -200..200: that many cells"},
                         &p.info_x);
    console.cvars().bind({.name = "mvd_info_y",
                          .fallback = "0",
                          .flags = ge::console::kArchive,
                          .help = "Where the players' table sits down, in cells from the top "
                                  "(up from the status bar when negative).",
                          .min = -200,
                          .max = 200,
                          .values = "0: at the top (the default); -200..200: that many cells"},
                         &p.info_y);
    console.cvars().bind({.name = "demo_playlist_track_name",
                          .fallback = "",
                          .flags = ge::console::kArchive,
                          .help = "A player to watch when a multiview demo starts: part of his "
                                  "name, in lower case.",
                          .kind = ge::console::CvarKind::text,
                          .values = "a name or part of one; empty: none (the default)"},
                         &p.track_name);
    console.cvars().bind({.name = "demo_benchmarkdumps",
                          .fallback = "1",
                          .flags = ge::console::kArchive,
                          .help = "Whether each timedemo's result is added to timedemo.log in "
                                  "the game folder.",
                          .kind = ge::console::CvarKind::boolean,
                          .values = "1: added (the default); 0: not kept"},
                         &p.bench_dumps);
    p.cvars = &console.cvars();
    console.cvars().bind({.name = "cam_lockpos",
                          .fallback = "0",
                          .flags = ge::console::kArchive,
                          .help = "Whether the third-person camera stays where it is and turns to "
                                  "follow the player you watch.",
                          .kind = ge::console::CvarKind::boolean,
                          .values = "0: it moves with him (the default); 1: it stays, turned to "
                                    "him"},
                         &p.lock_pos);
    console.cvars().bind({.name = "cam_zoomaccel",
                          .fallback = "2000",
                          .flags = ge::console::kArchive,
                          .help = "How fast the third-person camera's zoom gathers speed while "
                                  "forward or back is held, in units a second per second.",
                          .min = 0,
                          .max = 100000,
                          .values = "2000: the default; 0..100000: that many"},
                         &p.zoom_accel);
    console.cvars().bind({.name = "cam_zoomspeed",
                          .fallback = "300",
                          .flags = ge::console::kArchive,
                          .help = "The third-person camera's fastest zoom, in units a second.",
                          .min = 0,
                          .max = 100000,
                          .values = "300: the default; 0..100000: that many"},
                         &p.zoom_top);
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
  console.cvars().bind({.name = "mvd_autotrack",
                        .fallback = "0",
                        .flags = ge::console::kArchive,
                        .help = "Whether a multiview demo's view follows the hottest player on its "
                                "own.",
                        .kind = ge::console::CvarKind::boolean,
                        .values = "0: the player you pick (the default); 1: the hottest by armour, "
                                  "weapons, powerups, frags and kill streak"},
                       &p.autotrack);
  console.cvars().bind({.name = "cl_hightrack",
                        .fallback = "0",
                        .flags = ge::console::kArchive,
                        .help = "Whether the view follows the player with the most frags.",
                        .kind = ge::console::CvarKind::boolean,
                        .values = "0: the player you pick (the default); 1: the top fragger, "
                                  "changing only when someone has more"},
                       &p.hightrack);
  console.add_command("autotrack",
                      "autotrack: a multiview demo's view follows the hottest player, "
                      "or no longer",
                      [&p](ge::console::Console& c, const ge::console::Args&) {
                        c.cvars().set("mvd_autotrack", p.autotrack != 0 ? "0" : "1",
                                      ge::console::Source::code);
                        c.print(p.autotrack != 0 ? "Autotrack on." : "Autotrack off.");
                      });
  ge::shooter::add_demo_transport(
      console, p,
      {.tracked =
           [&bench, &console, &p](int track) {
             bench.demo.track = track;
             if (p.autotrack != 0)
               console.cvars().set("mvd_autotrack", "0", ge::console::Source::code);
             if (p.hightrack != 0)
               console.cvars().set("cl_hightrack", "0", ge::console::Source::code);
           },
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
  p.end_ask = false;
  p.share_ask = -1;
  p.share_skip_ask = 0;
  p.plan = {};
  p.plan_state = 0;
  p.track_name_done = false;
  const bool ok = scan_demo(t.start, p.scan, error);
  if (!ok) p.scan = DemoScan{};
  return ok;
}

TimedemoStep playback_step(DemoPlayback& p, Timedemo& t, const PlaybackInput& in, int w, int h,
                           double wall_dt_s, double now_s, std::vector<std::string>& say,
                           std::string& error) {
  t.smooth = p.lerp != 0;
  zoom_third_person(p, in.zoom, wall_dt_s);
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

  if (!p.track_name_done && !t.demo.state.map.empty()) {  // the player asked for by name
    p.track_name_done = true;
    const int slot = t.demo.state.format == DemoFormat::multiview
                         ? name_track_slot(t.demo.state, p.track_name)
                         : -1;
    if (slot >= 0) t.demo.track = slot;
  }
  follow(p, t, say);
  mark(p, t);
  // A skip and a jump become a seek from where the playhead stands.
  const double here = t.timeline.t;
  if (p.share_ask >= 0) p.seek_ask = std::clamp(p.share_ask, 0.0, 1.0) * length;
  if (p.share_skip_ask != 0) p.skip_ask += p.share_skip_ask * length;
  p.share_ask = -1;
  p.share_skip_ask = 0;
  if (std::exchange(p.end_ask, false)) {  // two seconds before the end, rounded up
    const bool in_intermission = t.demo.state.intermission;
    const double to = std::ceil(length) - 2;
    if (to - here < (in_intermission ? 10 : 2)) {
      say.push_back("Too close to the end of the demo.");
    } else {
      p.seek_ask = to;
      t.seek_stops_at_intermission = !in_intermission;
    }
  }
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
    p.jumped = true;
    t.seek_prints = p.skip_messages == 0;
    const TimedemoStep step = demo_seek(t, to, error);
    if (t.demo.state.intermission && to > t.timeline.t + 0.5) p.paused = true;  // stopped at one
    return step;
  }
  const TimedemoStep step = playdemo_step(t, wall_dt_s, p.paused ? 0.0 : p.speed * p.rate, error);
  if (step != TimedemoStep::end || p.hold_end == 0) return step;
  if (!p.paused) p.shown_until = now_s + kBarShowS;  // held: the bar shows the way back
  p.paused = true;
  return TimedemoStep::frame;
}

void zoom_third_person(DemoPlayback& p, int dir, double dt_s) {
  if (p.third_person == 0 || dir == 0 || !(dt_s > 0)) {
    if (p.zoom_speed > 0 && p.dist_cvar != nullptr && p.cvars != nullptr)  // the zoom ended
      p.cvars->set(*p.dist_cvar, std::to_string(std::lround(p.behind)), ge::console::Source::code);
    p.zoom_speed = 0;
    return;
  }
  const float dt = static_cast<float>(dt_s);
  p.zoom_speed = std::min(p.zoom_speed + p.zoom_accel * dt, p.zoom_top);
  p.behind = std::max(0.0f, p.behind - static_cast<float>(dir) * p.zoom_speed * dt);
}

float third_person_units(const DemoPlayback& p, bool watching) {
  return p.third_person != 0 && watching ? p.behind : 0;
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
  if (p.info != 0 && p.console != nullptr) {  // the players' table (mvd_info.hpp)
    const ge::hud::Style style = hud::hud_style();
    const ge::hud::Frame frame = ge::hud::frame_for(w, h, style);
    const float size = frame.px(style.caption_size);
    const float cell = size * 1.3f;
    const float right = static_cast<float>(w) - frame.safe.x;
    const float hud_top = static_cast<float>(h) - p.bar_bottom_px;
    float top = (p.info_y >= 0 ? frame.safe.y + p.info_y * cell : hud_top + p.info_y * cell) + cell;
    for (const MvdInfoLine& line :
         mvd_info_lines(t.demo, p.info_setup, p.info_header != 0, *p.console, p.places)) {
      const bool from_right = p.info_x < 0;
      const float x = from_right ? right + p.info_x * cell : frame.safe.x + p.info_x * cell;
      ge::hud::draw_shadowed(overlay, style.caption_face, size, x, top + size, line.text,
                             line.header ? style.bad : style.ink, style.shadow,
                             frame.px(style.shadow_drop) * 0.5f,
                             from_right ? gu::Align::right : gu::Align::left, 1);
      top += cell;
    }
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
