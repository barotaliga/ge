// FALL LINE'S HUD (games/fall_line/hud): the snapshot read off a free ride on the hand-built
// slope (at rest, skied by the bot over its kicker, hunted), the news the run's events write,
// the words, and the view laid out with every widget up at the three reference viewports
// without an overlap or a box off the screen.
//
// ge:summary Test: Fall Line's HUD snapshot, news, words and layout at three viewports
// ge:keywords test hud snapshot note_step news air-clock best-air wanted layout viewports
// ge:skill ui testing
#include <memory>
#include <span>
#include <string>

#include "check.hpp"
#include "fall_line/hud/snapshot.hpp"
#include "fall_line/hud/view.hpp"
#include "fall_line/hud/words.hpp"
#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/wanted.hpp"
#include "skier/synthetic.hpp"

namespace {

namespace fs = fall_line::sim;
namespace fh = fall_line::hud;
namespace mg = ge::mountain::mapgen;

fs::GameState slope_ride() {
  return fs::create_free_ride(std::make_shared<const mg::Level>(ge::skier::synthetic_slope()));
}

}  // namespace

TEST(at_rest_the_snapshot_reads_still_and_unhunted) {
  const fs::GameState st = slope_ride();
  const fh::HudRun run;
  const fh::HudSnapshot s = fh::snapshot(st, run);
  CHECK(s.speed_kmh < 0.5);
  CHECK(s.air_s == 0 && s.best_air_s == 0);
  CHECK(!s.wanted.has_value());
  CHECK(s.prompt == fh::Prompt::none);
  CHECK(s.machine == fh::Machine::skis);
}

TEST(the_bot_over_the_kicker_shows_the_air_clock_and_lands_into_the_news) {
  fs::GameState st = slope_ride();
  const fs::BotPlan plan = fs::plan_free(*st.level, 0, st.spec);
  fh::HudRun run;
  const fh::NewsInk ink;
  double most_air = 0, fastest = 0;
  bool shown_short = false;
  while (st.t < 40) {
    const fs::Controls c = fs::bot_controls(st, 0, plan);
    fs::step(st, std::span<const fs::Controls>(&c, 1));
    fh::note_step(run, st, ink);
    const fh::HudSnapshot s = fh::snapshot(st, run);
    if (s.air_s > 0 && s.air_s < fh::kAirShown) shown_short = true;
    most_air = std::max(most_air, s.air_s);
    fastest = std::max(fastest, s.speed_kmh);
    if (run.best_air > 0) break;
  }
  CHECK(!shown_short);
  CHECK(fastest > 20);
  CHECK_MSG(most_air >= fh::kAirShown, "the bot never flew past the air clock's least");
  CHECK(run.best_air >= fh::kAirShown);
  bool landed = false;
  for (const auto& l : run.news.lines()) landed |= l.text.rfind("AIR ", 0) == 0;
  CHECK(landed);
}

TEST(a_hunt_shows_its_stars_and_its_timer) {
  fs::GameState st = slope_ride();
  fs::set_wanted(st, 3);
  const fh::HudSnapshot s = fh::snapshot(st, fh::HudRun{});
  CHECK(s.wanted.has_value());
  if (!s.wanted) return;
  CHECK(s.wanted->stars == 3 && s.wanted->of == 5);
  CHECK(s.wanted->ended == fs::WantedEnd::running);
}

TEST(words_put_numbers_into_their_lines) {
  CHECK(fh::words::seed_at(38, 412.4, -1290.2) == "38 @ 412,-1290");
  CHECK(fh::words::distance(640) == "640 M");
  CHECK(fh::words::distance(2440) == "2.4 KM");
  CHECK(fh::words::clock(67.9) == "1:07");
  CHECK(fh::words::seconds(1.844) == "1.84 S");
}

TEST(every_widget_up_lays_out_clean_at_the_three_viewports) {
  const ge::hud::Style st = fh::hud_style();
  fh::HudSnapshot s;
  s.t = 12.3;
  s.seed = 38, s.x = -1412, s.z = -2290;
  s.speed_kmh = 104, s.edge = -0.6, s.air_s = 2.31, s.air_best = true, s.best_air_s = 1.9;
  s.skied_m = 12480, s.run = "B12 BLACK";
  s.prompt = fh::Prompt::stuck;
  fh::WantedTile w;
  w.stars = 4, w.flashing = true, w.search = 0.4, w.survived = 754;
  s.wanted = w;
  fh::HudRun run;
  run.news.push("WIPEOUT - CAUGHT AN EDGE", st.bad, 10);
  run.news.push("AIR 2.31 S  3.4 G", st.good, 11);
  run.news.push("ONTO B12 BLACK", st.ink, 11.5);
  run.news.push("TOP OF THE LIFT  4:12", st.ink, 12);
  for (const auto& [w_px, h_px] : {std::pair{1280, 720}, {390, 844}, {844, 390}}) {
    const ge::hud::Frame f = ge::hud::frame_for(w_px, h_px, st);
    ge::ui::DrawList dl;
    dl.begin(w_px, h_px);
    const auto placed = fh::draw_hud(dl, f, st, s, run);
    const auto problems = ge::hud::layout_problems(f, placed);
    std::string all;
    for (const auto& p : problems) all += p + "; ";
    CHECK_MSG(problems.empty(), std::to_string(w_px) + "x" + std::to_string(h_px) + ": " + all);
  }
}
