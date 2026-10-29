// Fall Line's HUD drawn: the corners filled from the snapshot, every box named for the check.
//
// ge:summary Fall Line's HUD view defined: each corner's widgets from the snapshot
// ge:keywords hud view draw_hud corners stacks wanted air speed prompt news
// ge:skill ui
// ge:test tests/fall_line_hud_test.cpp
#include "fall_line/hud/view.hpp"

#include <algorithm>
#include <cmath>
#include <string>

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
// The news column's width at the reference size, px, and its type's.
constexpr float kNewsW = 330;
constexpr float kNewsSize = 21;

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

std::vector<gh::Placed> draw_hud(ui::DrawList& dl, const gh::Frame& f, const gh::Style& st,
                                 const HudSnapshot& s, const HudRun& run) {
  std::vector<gh::Placed> placed;
  const float gap = f.px(st.gap);

  // TOP LEFT: which mountain, where on it, how far.
  gh::Stack left(f, gh::Anchor::top_left, gap);
  {
    const gh::Readout seed{words::seed_at(s.seed, s.x, s.z), words::kSeed, st.ink};
    const gh::Size z = gh::measure(f, st, seed);
    const ui::Rect r = left.next(z.w, z.h);
    gh::draw(dl, f, st, seed, r, ui::Align::left);
    placed.push_back({"seed", r});
  }
  {
    const gh::Readout where{s.run.empty() ? words::kOffRun : s.run,
                            std::string(words::kSkied) + " " + words::distance(s.skied_m), st.ink};
    const gh::Size z = gh::measure(f, st, where);
    const ui::Rect r = left.next(z.w, z.h);
    gh::draw(dl, f, st, where, r, ui::Align::left);
    placed.push_back({"run", r});
  }
  if (s.best_air_s > 0) {
    const gh::Readout best{words::seconds(s.best_air_s), words::kBestAir, st.ink};
    const gh::Size z = gh::measure(f, st, best);
    const ui::Rect r = left.next(z.w, z.h);
    gh::draw(dl, f, st, best, r, ui::Align::left);
    placed.push_back({"best-air", r});
  }

  // TOP RIGHT: the hunt.
  gh::Stack right(f, gh::Anchor::top_right, gap);
  if (s.wanted) {
    const WantedTile& w = *s.wanted;
    gh::Pips stars;
    stars.count = w.of;
    stars.lit = w.stars;
    stars.blink_off = w.flashing && !gh::blink(s.t, kStarBlinkHz);
    stars.color = w.ended == sim::WantedEnd::escaped ? st.good : st.ink;
    stars.size = 60;
    stars.spacing = 2;
    const gh::Size z = gh::measure(f, stars);
    const ui::Rect r = right.next(z.w, z.h);
    gh::draw(dl, f, st, stars, r);
    placed.push_back({"wanted-stars", r});
    if (w.search >= 0 || w.hidden) {
      gh::Meter bar;
      bar.value = static_cast<float>(w.search >= 0 ? w.search : 1);
      bar.fill = w.hidden ? st.good : st.ink;
      bar.w = 120;
      bar.h = 7;
      const gh::Size bz = gh::measure(f, bar);
      const gh::Readout said{"", w.hidden ? words::kHidden : words::kSearching, st.ink};
      const gh::Size sz = gh::measure(f, st, said);
      const ui::Rect rb = right.next(bz.w, bz.h);
      gh::draw(dl, f, st, bar, rb);
      const ui::Rect rs = right.next(std::max(sz.w, bz.w), sz.h);
      gh::draw(dl, f, st, said, rs, ui::Align::right);
      placed.push_back({"wanted-search", {rb.x, rb.y, rb.w, rs.bottom() - rb.y}});
    }
    const gh::Readout timer{words::clock(w.survived),
                            w.ended == sim::WantedEnd::escaped ? words::kEscaped : words::kSurvived,
                            st.ink};
    const gh::Size tz = gh::measure(f, st, timer);
    const ui::Rect rt = right.next(tz.w, tz.h);
    gh::draw(dl, f, st, timer, rt, ui::Align::right);
    placed.push_back({"wanted-timer", rt});
  }

  float air_bottom = 0;
  // TOP CENTRE: the air clock. On a narrow screen the corners' columns take the top, so it drops
  // under the taller of them.
  if (s.air_s > 0) {
    const gh::Big air{num::to_fixed(s.air_s, 2), "S", s.air_best ? words::kBestAir : words::kAir,
                      s.air_best ? st.accent : st.ink};
    const gh::Size z = gh::measure(f, st, air);
    const float dy = f.portrait ? std::max(left.used(), right.used()) + gap : 0;
    const ui::Rect r = gh::place(f, gh::Anchor::top, z.w, z.h, 0, dy);
    gh::draw(dl, f, st, air, r, ui::Align::center);
    placed.push_back({"air", r});
    air_bottom = r.bottom();
  }

  // BOTTOM LEFT: the speed, the edge bar over it.
  gh::Stack low(f, gh::Anchor::bottom_left, gap);
  if (s.machine == Machine::lift) {
    const gh::Readout riding{words::kRiding, "", st.ink};
    const gh::Size z = gh::measure(f, st, riding);
    const ui::Rect r = low.next(z.w, z.h);
    gh::draw(dl, f, st, riding, r, ui::Align::left);
    placed.push_back({"lift", r});
  } else {
    const bool skis = s.machine == Machine::skis;
    const double kmh = skis || s.machine == Machine::para ? s.speed_kmh : s.machine_kmh;
    const gh::Big speed{num::to_fixed(std::round(kmh), 0), words::kSpeedUnit,
                        skis ? "" : machine_word(s.machine), st.ink};
    const gh::Size z = gh::measure(f, st, speed);
    const ui::Rect r = low.next(z.w, z.h);
    gh::draw(dl, f, st, speed, r, ui::Align::left);
    placed.push_back({"speed", r});
    if (skis) {
      gh::Meter edge;
      edge.value = static_cast<float>(s.edge);
      edge.centred = true;
      edge.ticks = 4;
      edge.fill = st.accent;
      edge.w = 210;
      edge.h = 10;
      const gh::Size ez = gh::measure(f, edge);
      const ui::Rect re = low.next(ez.w, ez.h);
      gh::draw(dl, f, st, edge, re);
      placed.push_back({"edge", re});
    } else if (s.machine == Machine::heli || s.machine == Machine::para) {
      const gh::Readout height{words::altitude(s.machine_height), words::kHeight, st.ink};
      const gh::Size hz = gh::measure(f, st, height);
      const ui::Rect rh = low.next(hz.w, hz.h);
      gh::draw(dl, f, st, height, rh, ui::Align::left);
      placed.push_back({"height", rh});
    }
  }

  // BOTTOM RIGHT: the news.
  {
    const float news_w = std::min(kNewsW, (f.safe.w / f.scale) * 0.48f);
    const ui::Rect r =
        gh::draw_feed(dl, f, st, run.news, s.t, gh::Anchor::bottom_right, kNewsSize, news_w);
    placed.push_back({"news", r});
  }

  // UPPER CENTRE: the prompt.
  if (s.prompt != Prompt::none) {
    const PromptWords pw = prompt_words(s.prompt);
    const bool bad = s.prompt == Prompt::died || s.prompt == Prompt::arrested;
    const gh::Banner b{pw.title, pw.line, bad ? st.bad : st.ink};
    const gh::Size z = gh::measure(f, st, b);
    // In the upper centre, or under the air clock where a short screen brings the two together.
    ui::Rect r = gh::place(f, gh::Anchor::center, z.w, z.h, 0, -f.safe.h * 0.2f);
    if (air_bottom > 0) r.y = std::max(r.y, air_bottom + gap);
    gh::draw(dl, f, st, b, r);
    placed.push_back({"prompt", r});
  }
  return placed;
}

}  // namespace fall_line::hud
