// Any shooter's demo transport (demo_transport.hpp).
//
// ge:summary Any shooter's demo transport: speed, pause, seek, skips, highlights, the console words
// ge:keywords demo playback transport add_demo_transport parse_clock
// ge:skill shooter replay console
// ge:test tests/shooters_edge_demo_highlights_test.cpp
#include "shooter/demo_transport.hpp"

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace ge::shooter {

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

void add_demo_transport(console::Console& console, DemoTransport& p, const DemoCalls& calls) {
  using Con = console::Console;
  using CArgs = console::Args;
  console.add_command("cl_demospeed", "cl_demospeed <x>: a played demo's speed, 0..20 (0 pauses)",
                      [&p](Con& c, const CArgs& a) {
                        if (a.size() > 1) p.speed = std::clamp(std::atof(a[1].c_str()), 0.0, 20.0);
                        c.print("cl_demospeed " + std::to_string(p.speed));
                      });
  console.add_command("demo_track", "demo_track <slot>: the player a multiview demo's view rides",
                      [&p, calls](Con& c, const CArgs& a) {
                        if (a.size() > 1) p.track = std::atoi(a[1].c_str());
                        if (calls.tracked) calls.tracked(p.track);
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
  console.add_command("demo_jump",
                      "demo_jump [+|-][m:]<s>: the demo playing to a time, or on or back by one",
                      [&p](Con& c, const CArgs& a) {
                        const std::string w = a.size() > 1 ? a[1] : "";
                        const bool rel = !w.empty() && (w[0] == '+' || w[0] == '-');
                        const double t = parse_clock(rel ? w.substr(1) : w);
                        if (a.size() != 2 || t < 0 || std::count(w.begin(), w.end(), ':') > 1) {
                          c.print("Usage: demo_jump [+|-][m:]<s> (seconds)");
                          return;
                        }
                        if (!rel)
                          p.seek_ask = t;
                        else
                          p.skip_ask += w[0] == '-' ? -t : t;
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
                      [calls](Con& c, const CArgs& a) {
                        if (!calls.highlights) return;
                        for (const std::string& line : calls.highlights(a[1] == "all"))
                          c.print(line);
                      });
  console.cvars().bind({.name = "demo_skip_forward",
                        .fallback = "15",
                        .help = "how far forward Right (or L) moves a demo, s",
                        .kind = console::CvarKind::number,
                        .min = 0,
                        .max = 3600},
                       &p.skip_forward_s);
  console.cvars().bind({.name = "demo_skip_back",
                        .fallback = "5",
                        .help = "how far back Left (or J) moves a demo, s",
                        .kind = console::CvarKind::number,
                        .min = 0,
                        .max = 3600},
                       &p.skip_back_s);
  console.cvars().bind({.name = "demo_bar",
                        .fallback = "1",
                        .help = "a demo's play bar: 0 hidden, 1 shown after the pointer or a key, "
                                "2 always",
                        .kind = console::CvarKind::integer,
                        .min = 0,
                        .max = 2},
                       &p.bar);
  console.cvars().bind({.name = "demo_lerp",
                        .fallback = "1",
                        .help = "a played demo drawn between its frames, so it moves smoothly "
                                "(1), or each frame as read (0)",
                        .kind = console::CvarKind::integer,
                        .min = 0,
                        .max = 1},
                       &p.lerp);
}

}  // namespace ge::shooter
