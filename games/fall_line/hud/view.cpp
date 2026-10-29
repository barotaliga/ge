// Fall Line's HUD drawn: the corners filled from the snapshot, every box named for the check.
//
// ge:summary Fall Line's HUD view defined: each corner's widgets from the snapshot
// ge:keywords hud view draw_hud hud_layout fill_hud elements corners wanted air speed prompt news
// ge:skill ui
// ge:test tests/fall_line_hud_test.cpp
#include "fall_line/hud/view.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include "core/num_text.hpp"
#include "fall_line/hud/words.hpp"
#include "hud/feed.hpp"

namespace fall_line::hud {

namespace {

namespace gh = ge::hud;
namespace ui = ge::ui;
namespace num = ge::core::num;

// How fast the wanted stars blink while the army searches, Hz.
constexpr double kStarBlinkHz = 2.5;
// The news column's width at the reference size, px, at most this share of the safe area's, and
// its type's size.
constexpr float kNewsW = 330;
constexpr float kNewsShare = 0.48f;
constexpr float kNewsSize = 21;
// The prompt's place: above the centre by this share of the safe area's height.
constexpr double kPromptY = -0.2;

const char* machine_word(Machine m) {
  switch (m) {
    case Machine::sled:
      return words::kOnSled;
    case Machine::heli:
      return words::kOnHeli;
    case Machine::para:
      return words::kOnPara;
    default:
      return "";
  }
}

struct PromptWords {
  const char* title;
  const char* line;
};

PromptWords prompt_words(Prompt p) {
  switch (p) {
    case Prompt::down:
      return {words::kDown, words::kDownHow};
    case Prompt::stuck:
      return {words::kStuck, words::kStuckHow};
    case Prompt::waiting:
      return {words::kWaiting, words::kWaitingHow};
    case Prompt::died:
      return {words::kDied, ""};
    case Prompt::arrested:
      return {words::kArrested, ""};
    case Prompt::none:
      break;
  }
  return {"", ""};
}

}  // namespace

gh::Style hud_style() {
  gh::Style st;
  st.value_face = gh::Face::bitmap(ge::charset::FontId::rounded);
  st.caption_face = gh::Face::bitmap(ge::charset::FontId::rounded);
  st.big_face = gh::Face::bitmap(ge::charset::FontId::rounded);
  return st;
}

NewsInk news_ink(const gh::Style& style) { return {style.ink, style.good, style.bad}; }

HudLayout hud_layout(const gh::Style& st) {
  HudLayout h;
  gh::Layout& l = h.layout;
  l.margin = st.margin;
  using gh::Anchor;
  using gh::Kind;
  // TOP LEFT: which mountain, where on it, how far.
  h.seed = l.add({.id = "seed", .kind = Kind::readout, .help = "the seed and where he stands"});
  h.run = l.add({.id = "run", .kind = Kind::readout, .help = "the run and the metres skied"});
  h.best_air = l.add({.id = "best_air", .kind = Kind::readout, .help = "the run's best air"});
  // TOP RIGHT: the hunt.
  gh::Pips stars;
  stars.size = 60;
  stars.spacing = 2;
  h.stars = l.add({.id = "stars",
                   .kind = Kind::pips,
                   .anchor = Anchor::top_right,
                   .help = "the wanted stars",
                   .pips = stars});
  gh::Meter search;
  search.w = 120;
  search.h = 7;
  h.search = l.add({.id = "search",
                    .kind = Kind::bar,
                    .anchor = Anchor::top_right,
                    .help = "the army's search running out",
                    .bar = search});
  h.timer = l.add({.id = "timer",
                   .kind = Kind::readout,
                   .anchor = Anchor::top_right,
                   .help = "the time survived"});
  // TOP CENTRE: the air clock, under the corners' columns where it would meet them.
  h.air = l.add({.id = "air",
                 .kind = Kind::big,
                 .anchor = Anchor::top,
                 .help = "the air clock",
                 .under_corners = true});
  // BOTTOM LEFT: the speed, the edge bar over it.
  h.lift = l.add({.id = "lift",
                  .kind = Kind::readout,
                  .anchor = Anchor::bottom_left,
                  .help = "riding a lift"});
  h.speed =
      l.add({.id = "speed", .kind = Kind::big, .anchor = Anchor::bottom_left, .help = "the speed"});
  gh::Meter edge;
  edge.centred = true;
  edge.ticks = 4;
  edge.w = 210;
  edge.h = 10;
  h.edge = l.add({.id = "edge",
                  .kind = Kind::bar,
                  .anchor = Anchor::bottom_left,
                  .help = "the edge bar",
                  .bar = edge});
  h.height = l.add({.id = "height",
                    .kind = Kind::readout,
                    .anchor = Anchor::bottom_left,
                    .help = "the height over the snow"});
  // BOTTOM RIGHT: the news.
  h.news = l.add({.id = "news",
                  .kind = Kind::feed,
                  .anchor = Anchor::bottom_right,
                  .help = "the news",
                  .feed_size = kNewsSize,
                  .feed_w = kNewsW,
                  .feed_share = kNewsShare});
  // UPPER CENTRE: the prompt, under the air clock where a short screen brings the two together.
  h.prompt = l.add({.id = "prompt",
                    .kind = Kind::banner,
                    .anchor = Anchor::center,
                    .y = kPromptY,
                    .help = "the prompt",
                    .under = "air"});
  return h;
}

void fill_hud(HudLayout& h, const gh::Style& st, const HudSnapshot& s, const HudRun& run) {
  gh::Layout& l = h.layout;
  for (size_t i = 0; i < l.size(); ++i) l[i].content.on = false;
  auto text = [&](int i, std::string value, std::string caption, ui::Color color) {
    gh::Content& c = l.content(i);
    c.on = true;
    c.value = std::move(value);
    c.caption = std::move(caption);
    c.unit.clear();
    c.color = color;
    return &c;
  };
  text(h.seed, words::seed_at(s.seed, s.x, s.z), words::kSeed, st.ink);
  text(h.run, s.run.empty() ? words::kOffRun : s.run,
       std::string(words::kSkied) + " " + words::distance(s.skied_m), st.ink);
  if (s.best_air_s > 0) text(h.best_air, words::seconds(s.best_air_s), words::kBestAir, st.ink);

  if (s.wanted) {
    const WantedTile& w = *s.wanted;
    gh::Content& c = l.content(h.stars);
    c.on = true;
    c.count = w.of;
    c.lit = w.stars;
    c.blink_off = w.flashing && !gh::blink(s.t, kStarBlinkHz);
    c.color = w.ended == sim::WantedEnd::escaped ? st.good : st.ink;
    if (w.search >= 0 || w.hidden) {
      gh::Content& b = *text(h.search, "", w.hidden ? words::kHidden : words::kSearching,
                             w.hidden ? st.good : st.ink);
      b.level = static_cast<float>(w.search >= 0 ? w.search : 1);
    }
    text(h.timer, words::clock(w.survived),
         w.ended == sim::WantedEnd::escaped ? words::kEscaped : words::kSurvived, st.ink);
  }

  if (s.air_s > 0) {
    gh::Content& c =
        *text(h.air, num::to_fixed(s.air_s, 2), s.air_best ? words::kBestAir : words::kAir,
              s.air_best ? st.accent : st.ink);
    c.unit = "S";
  }

  if (s.machine == Machine::lift) {
    text(h.lift, words::kRiding, "", st.ink);
  } else {
    const bool skis = s.machine == Machine::skis;
    const double kmh = skis || s.machine == Machine::para ? s.speed_kmh : s.machine_kmh;
    gh::Content& c = *text(h.speed, num::to_fixed(std::round(kmh), 0),
                           skis ? "" : machine_word(s.machine), st.ink);
    c.unit = words::kSpeedUnit;
    if (skis) {
      gh::Content& e = *text(h.edge, "", "", st.accent);
      e.level = static_cast<float>(s.edge);
    } else if (s.machine == Machine::heli || s.machine == Machine::para) {
      text(h.height, words::altitude(s.machine_height), words::kHeight, st.ink);
    }
  }

  gh::Content& news = l.content(h.news);
  news.on = true;
  news.feed = &run.news;
  news.t = s.t;

  if (s.prompt != Prompt::none) {
    const PromptWords pw = prompt_words(s.prompt);
    const bool bad = s.prompt == Prompt::died || s.prompt == Prompt::arrested;
    text(h.prompt, pw.title, pw.line, bad ? st.bad : st.ink);
  }
}

std::vector<gh::Placed> draw_hud(ui::DrawList& dl, const gh::Frame& f, const gh::Style& st,
                                 const HudSnapshot& s, const HudRun& run) {
  HudLayout h = hud_layout(st);
  fill_hud(h, st, s, run);
  return h.layout.draw(dl, f, st);
}

}  // namespace fall_line::hud
