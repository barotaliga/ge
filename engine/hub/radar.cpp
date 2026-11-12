// The hub's map of a match, defined: the SVG (ground, trails, dots and their animation), the page,
// the route.
//
// ge:summary The hub's map defined: a radar track as SVG, animated or still, its page and route
// ge:keywords hub radar svg animate-transform trail outline dots page route
// ge:skill net
// ge:test tests/hub_radar_test.cpp
#include "hub/radar.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <vector>

#include "hub/page_parts.hpp"
#include "hub/pages.hpp"

namespace ge::hub {

namespace gr = ge::replay;

namespace {

constexpr double kView = 1000;  // the SVG's long side, in its own units
constexpr const char* kPalette[] = {"#e5534b", "#4a8fe7", "#e0b43a", "#4fb36b",
                                    "#b469d8", "#e07f3a", "#3ab7c4", "#d65d9a"};

struct Frame {
  double scale = 1, ox = 0, oy = 0, w = kView, h = kView;
  int32_t min_x = 0, max_y = 0;
  double x(int32_t v) const { return ox + (v - min_x) * scale; }
  double y(int32_t v) const { return oy + (max_y - v) * scale; }
};

Frame frame_of(const gr::Radar& r) {
  Frame f;
  const double w = std::max(1, r.max_x - r.min_x), h = std::max(1, r.max_y - r.min_y);
  const double margin = 0.05;
  f.scale = kView * (1 - 2 * margin) / std::max(w, h);
  f.w = w >= h ? kView : w * f.scale + 2 * margin * kView;
  f.h = h >= w ? kView : h * f.scale + 2 * margin * kView;
  f.ox = margin * kView;
  f.oy = margin * kView;
  f.min_x = r.min_x;
  f.max_y = r.max_y;
  return f;
}

std::string num(double v) {
  char out[32];
  std::snprintf(out, sizeof out, "%.1f", v);
  std::string s = out;
  if (s.size() > 2 && s.compare(s.size() - 2, 2, ".0") == 0) s.resize(s.size() - 2);
  return s;
}

const gr::RadarPlayer* player_of(const gr::Radar& r, uint8_t slot) {
  for (const gr::RadarPlayer& p : r.roster) {
    if (p.slot == slot) return &p;
  }
  return nullptr;
}

// The colour of a slot: its team's (named colours by name, others by their order), or the slot's
// own place in the roster when nobody has a team.
std::string colour_of(const gr::Radar& r, uint8_t slot) {
  std::vector<std::string> teams;
  for (const gr::RadarPlayer& p : r.roster) {
    if (!p.team.empty() && std::find(teams.begin(), teams.end(), p.team) == teams.end()) {
      teams.push_back(p.team);
    }
  }
  const gr::RadarPlayer* me = player_of(r, slot);
  if (!teams.empty() && me && !me->team.empty()) {
    std::string t = me->team;
    for (char& c : t) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    const std::pair<const char*, int> named[] = {
        {"red", 0}, {"blue", 1}, {"yellow", 2}, {"green", 3}};
    for (const auto& [name, i] : named) {
      if (t == name) return kPalette[i];
    }
    const auto at =
        static_cast<size_t>(std::find(teams.begin(), teams.end(), me->team) - teams.begin());
    return kPalette[(at + 4) % 8];
  }
  size_t at = slot;
  for (size_t i = 0; i < r.roster.size(); i++) {
    if (r.roster[i].slot == slot) at = i;
  }
  return kPalette[at % 8];
}

std::vector<uint8_t> slots_of(const gr::Radar& r) {
  std::vector<uint8_t> s;
  for (const gr::RadarFrame& f : r.frames) {
    for (const gr::RadarDot& d : f.dots) {
      if (std::find(s.begin(), s.end(), d.slot) == s.end()) s.push_back(d.slot);
    }
  }
  std::sort(s.begin(), s.end());
  return s;
}

const gr::RadarDot* dot_of(const gr::RadarFrame& f, uint8_t slot) {
  for (const gr::RadarDot& d : f.dots) {
    if (d.slot == slot) return &d;
  }
  return nullptr;
}

// The index of the last frame at or before `t_ms` (0 when none is).
size_t frame_at(const gr::Radar& r, int64_t t_ms) {
  size_t i = 0;
  while (i + 1 < r.frames.size() && r.frames[i + 1].time_ms <= t_ms) i++;
  return i;
}

std::string trails(const gr::Radar& r, const Frame& fr) {
  std::string s =
      "<g class=\"trail\" fill=\"none\" stroke-width=\"4\" stroke-linecap=\"round\" "
      "stroke-linejoin=\"round\" opacity=\"0.14\">";
  const size_t step = std::max<size_t>(1, r.frames.size() / 1500);
  const double jump = 0.12 * kView;  // a respawn or a teleporter: the line breaks
  for (const uint8_t slot : slots_of(r)) {
    std::string pts;
    double lx = 0, ly = 0;
    bool open = false;
    auto close = [&] {
      if (open && pts.find(' ') != std::string::npos) {
        s += "<polyline stroke=\"";
        s += colour_of(r, slot);
        s += "\" points=\"";
        s += pts;
        s += "\"/>";
      }
      pts.clear();
      open = false;
    };
    for (size_t i = 0; i < r.frames.size(); i += step) {
      const gr::RadarDot* d = dot_of(r.frames[i], slot);
      if (!d || (d->flags & gr::kRadarDead)) {
        close();
        continue;
      }
      const double x = fr.x(d->x), y = fr.y(d->y);
      if (open && (std::abs(x - lx) > jump || std::abs(y - ly) > jump)) close();
      if (!pts.empty()) pts += ' ';
      pts += num(x);
      pts += ',';
      pts += num(y);
      open = true;
      lx = x;
      ly = y;
    }
    close();
  }
  return s + "</g>";
}

// The dot and its name (the wedge for the way it faces is drawn under it, turned on its own).
std::string glyph(const std::string& colour, const std::string& name, uint8_t flags) {
  std::string g =
      "<circle r=\"11\" fill=\"" + colour + "\" stroke=\"#0b0f12\" stroke-width=\"2\"/>";
  if (flags & gr::kRadarCarrier) {
    g += "<circle r=\"17\" fill=\"none\" stroke=\"" + colour + "\" stroke-width=\"3\"/>";
  }
  g += "<text x=\"16\" y=\"-14\" class=\"who\">" + html_escape(name) + "</text>";
  return g;
}

std::string heading_deg(uint8_t h) { return num(h * 360.0 / 256.0); }

}  // namespace

std::string radar_svg(const gr::Radar& r, int64_t from_ms, int64_t to_ms, bool animate) {
  const Frame fr = frame_of(r);
  std::string s = "<svg class=\"radar\" xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " +
                  num(fr.w) + " " + num(fr.h) + "\" role=\"img\" aria-label=\"the map\">";
  s += "<rect width=\"100%\" height=\"100%\" class=\"ground\"/>";
  if (!r.outline.empty()) {
    s += "<g class=\"outline\" stroke-width=\"3\">";
    for (const gr::RadarLine& l : r.outline) {
      s += "<line x1=\"" + num(fr.x(l.x0)) + "\" y1=\"" + num(fr.y(l.y0)) + "\" x2=\"" +
           num(fr.x(l.x1)) + "\" y2=\"" + num(fr.y(l.y1)) + "\"/>";
    }
    s += "</g>";
  }
  s += trails(r, fr);
  if (r.frames.empty()) return s + "</svg>";
  const size_t first = frame_at(r, from_ms);
  size_t last = first;
  if (animate) {
    while (last + 1 < r.frames.size() && r.frames[last + 1].time_ms <= to_ms) last++;
  }
  const bool moving = animate && last > first;
  const double dur_s =
      moving ? static_cast<double>(r.frames[last].time_ms - r.frames[first].time_ms) / 1000.0 : 0;
  s += "<g class=\"dots\">";
  for (const uint8_t slot : slots_of(r)) {
    const gr::RadarPlayer* p = player_of(r, slot);
    const std::string colour = colour_of(r, slot);
    const std::string name = p ? p->name : "#" + std::to_string(slot);
    // Where it is at each frame of the span (held at its last place when a frame leaves it out).
    std::string at, turn, fade, times;
    const gr::RadarDot* held = nullptr;
    for (size_t i = 0; i <= first; i++) {
      if (const gr::RadarDot* d = dot_of(r.frames[i], slot)) held = d;
    }
    if (!held) {  // not yet in the match at the span's start: its first place in the span
      for (size_t i = first; i <= last && !held; i++) held = dot_of(r.frames[i], slot);
    }
    if (!held) continue;
    const gr::RadarDot start = *held;
    for (size_t i = first; moving && i <= last; i++) {
      if (const gr::RadarDot* d = dot_of(r.frames[i], slot)) held = d;
      const std::string sep = i == first ? "" : ";";
      at += sep + num(fr.x(held->x)) + " " + num(fr.y(held->y));
      turn += sep + heading_deg(held->heading);
      fade += sep + ((held->flags & gr::kRadarDead) ? "0.25" : "1");
      char kt[16];
      std::snprintf(
          kt, sizeof kt, "%.4f",
          static_cast<double>(r.frames[i].time_ms - r.frames[first].time_ms) / 1000.0 / dur_s);
      times += sep + kt;
    }
    s += "<g transform=\"translate(" + num(fr.x(start.x)) + " " + num(fr.y(start.y)) + ")\"";
    s += (start.flags & gr::kRadarDead) ? " opacity=\"0.25\">" : ">";
    if (moving) {
      // Each value at its frame's own time (keyTimes), so a gap in the track is not squeezed.
      std::string timing = "dur=\"";
      timing.append(num(dur_s)).append("s\" repeatCount=\"indefinite\" keyTimes=\"");
      timing.append(times).append("\" ");
      s.append("<animateTransform attributeName=\"transform\" type=\"translate\" ")
          .append(timing)
          .append("values=\"")
          .append(at)
          .append("\"/>");
      s.append("<animate attributeName=\"opacity\" calcMode=\"discrete\" ")
          .append(timing)
          .append("values=\"")
          .append(fade)
          .append("\"/>");
    }
    s += "<g transform=\"rotate(" + heading_deg(start.heading) + ")\">";
    if (moving) {
      s.append(
           "<animateTransform attributeName=\"transform\" type=\"rotate\" calcMode=\"discrete\" "
           "dur=\"")
          .append(num(dur_s))
          .append("s\" repeatCount=\"indefinite\" keyTimes=\"")
          .append(times)
          .append("\" values=\"")
          .append(turn)
          .append("\"/>");
    }
    s += "<path d=\"M0,-24 L8,-12 L-8,-12 Z\" fill=\"" + colour + "\"/></g>";
    s += glyph(colour, name, start.flags) + "</g>";
  }
  return s + "</g></svg>";
}

std::string radar_page(const HubSpec& spec, const DemoRow& row, const gr::Radar& r,
                       std::string_view target) {
  const std::string at_q = query_param(target, "at");
  const bool still = !at_q.empty();
  const int64_t end_ms = r.frames.empty() ? 0 : r.frames.back().time_ms;
  const int64_t start_ms = r.frames.empty() ? 0 : r.frames.front().time_ms;
  auto clamp_ms = [&](int64_t v) { return std::clamp(v, start_ms, end_ms); };
  const int64_t asked_ms =
      static_cast<int64_t>(std::atof((still ? at_q : query_param(target, "from")).c_str()) * 1000);
  const int64_t from_ms = clamp_ms(start_ms + asked_ms);
  const int64_t to_ms = still ? from_ms : std::min(end_ms, from_ms + kRadarWindowMs);
  auto clock = [](int64_t ms) {
    char out[24];
    const int64_t s = std::max<int64_t>(0, ms / 1000);
    std::snprintf(out, sizeof out, "%lld:%02lld", static_cast<long long>(s / 60),
                  static_cast<long long>(s % 60));
    return std::string(out);
  };
  const std::string base = "/radar/" + row.sha;
  auto link = [&](const char* key, int64_t ms, const std::string& word) {
    const int64_t rel = clamp_ms(ms) - start_ms;
    return "<a class=\"btn\" href=\"" + base + "?" + key + "=" +
           num(static_cast<double>(rel) / 1000) + "\">" + word + "</a>";
  };
  const std::string title = row.title.empty() ? row.map : row.title;
  std::string h = head(spec, "Map of " + title, false) + nav(spec, "Demos");
  h += "<main class=\"radar\"><h2>" + html_escape(title) + "</h2><p class=\"meta\">" +
       html_escape(row.mode) + (row.mode.empty() || row.map.empty() ? "" : " · ") +
       html_escape(row.map) + (row.mode.empty() && row.map.empty() ? "" : " · ") +
       (still ? "at " + clock(from_ms - start_ms)
              : clock(from_ms - start_ms) + " to " + clock(to_ms - start_ms)) +
       " of " + clock(end_ms - start_ms) + "</p>";
  h += "<figure>" + radar_svg(r, from_ms, to_ms, !still) + "</figure>";
  h += "<p class=\"act\">";
  if (still) {
    h += link("at", from_ms - 5000, "−5 s") + link("at", from_ms - 1000, "−1 s") +
         link("at", from_ms + 1000, "+1 s") + link("at", from_ms + 5000, "+5 s") +
         link("from", from_ms, "Play from here");
  } else {
    if (from_ms > start_ms) h += link("from", from_ms - kRadarWindowMs, "Earlier minute");
    if (to_ms < end_ms) h += link("from", to_ms, "Next minute");
    h += link("at", from_ms, "Step through");
  }
  h += "</p><ul class=\"legend\">";
  for (const gr::RadarPlayer& p : r.roster) {
    h += "<li><span class=\"swatch\" style=\"background:" + colour_of(r, p.slot) + "\"></span>" +
         html_escape(p.name) +
         (p.team.empty() ? "" : " <small>" + html_escape(p.team) + "</small>") + "</li>";
  }
  h +=
      "</ul><p class=\"none\">The faint lines are where each player went over the whole match.</p>";
  h += "</main>" + foot();
  return h;
}

bool radar_answer(const Archive* a, const HubSpec& spec, std::string_view target, Answer& out) {
  const std::string path = target_path(target);
  const std::string kPrefix = "/radar/";
  if (path.rfind(kPrefix, 0) != 0) return false;
  std::string sha = path.substr(kPrefix.size());
  while (!sha.empty() && sha.back() == '/') sha.pop_back();
  const DemoRow* row = a ? archive_find(*a, sha) : nullptr;
  auto missing = [&](const std::string& why) {
    out = html(404,
               head(spec, "No map", false) + nav(spec, "Demos") + "<main><h2>No map</h2><p>" +
                   html_escape(why) +
                   " The demos are on <a href=\"/demos\">the Demos page</a>.</p></main>" + foot(),
               0);
    return true;
  };
  if (!row) return missing("The hub keeps no demo with that hash.");
  if (!row->radar) return missing("That demo was recorded without the positions a map needs.");
  std::ifstream f(std::filesystem::path(a->dir) / archive_path(sha), std::ios::binary);
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),
                                   std::istreambuf_iterator<char>());
  gr::Radar r;
  std::string error;
  if (bytes.empty() || !gr::read_radar_file(bytes, r, error) || !r.any) {
    return missing("That demo's file cannot be read right now.");
  }
  out = html(200, radar_page(spec, *row, r, target), 3600);
  return true;
}

}  // namespace ge::hub
