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

double parse_tenths_clock(const std::string& text) {
  const auto digits = [](const std::string& d) {
    if (d.empty() || d.size() > 9) return -1.0;
    double v = 0;
    for (char c : d) {
      if (c < '0' || c > '9') return -1.0;
      v = v * 10 + (c - '0');
    }
    return v;
  };
  const size_t colon = text.find(':');
  const size_t dot = text.find('.');
  if (colon != std::string::npos &&
      (text.find(':', colon + 1) != std::string::npos || (dot != std::string::npos && dot < colon)))
    return -1;
  if (dot != std::string::npos && text.find('.', dot + 1) != std::string::npos) return -1;
  const size_t s_from = colon == std::string::npos ? 0 : colon + 1;
  const double m = colon == std::string::npos ? 0 : digits(text.substr(0, colon));
  const double sec = digits(text.substr(s_from, dot == std::string::npos ? dot : dot - s_from));
  const double tenths = dot == std::string::npos ? 0 : digits(text.substr(dot + 1));
  if (m < 0 || sec < 0 || tenths < 0) return -1;
  return m * 60 + sec + tenths / 10;
}

namespace {

// Quake II's `seek [+-]<timespec|percent>[%]` into the asks (parse_tenths_clock).
void q2_seek(console::Console& c, DemoTransport& p, const console::Args& a) {
  const std::string w = a.size() > 1 ? a[1] : "";
  if (a.size() != 2 || w.empty()) {
    c.print("Usage: seek [+-]<timespec|percent>[%]");
    return;
  }
  const bool rel = w[0] == '+' || w[0] == '-';
  const double sign = w[0] == '-' ? -1 : 1;
  const std::string body = rel ? w.substr(1) : w;
  if (!body.empty() && body.back() == '%') {
    char* end = nullptr;
    const std::string n = body.substr(0, body.size() - 1);
    const double v = std::strtod(n.c_str(), &end);
    if (n.empty() || end == n.c_str() || *end != '\0' || !(v >= 0)) {
      c.print("Invalid percentage.");
      return;
    }
    if (rel)
      p.share_skip_ask += sign * v / 100;
    else
      p.share_ask = std::min(v, 100.0) / 100;
    return;
  }
  const double t = parse_tenths_clock(body);
  if (t < 0) {
    c.print(rel ? "Invalid relative timespec." : "Invalid absolute timespec.");
    return;
  }
  if (rel)
    p.skip_ask += sign * t;
  else
    p.seek_ask = t;
}

}  // namespace

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
                        if (a[0].size() == 4 && (a[0][0] | 0x20) == 's' &&
                            (a[0][1] | 0x20) == 'e' && (a[0][2] | 0x20) == 'e' &&
                            (a[0][3] | 0x20) == 'k') {  // Quake II's spelling takes its own forms
                          q2_seek(c, p, a);
                          return;
                        }
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
  console.add_command("demo_jump_end",
                      "demo_jump_end: the demo playing to two seconds before its end",
                      [&p](Con&, const CArgs&) { p.end_ask = true; });
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
  console.cvars().bind({.name = "demo_jump_skip_messages",
                        .fallback = "1",
                        .flags = console::kArchive,
                        .help = "Whether the messages a demo jump passes over are left unshown.",
                        .kind = console::CvarKind::boolean,
                        .values = "1: left unshown (the default); 0: printed as the jump reads "
                                  "them"},
                       &p.skip_messages);
}

}  // namespace ge::shooter
