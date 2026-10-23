// LAB `ladder`: FALL LINE'S QUALITY LADDER (games/fall_line/app/ladder.hpp, docs/rendering.md §
// The quality ladder). Without flags it reads the table and checks it: every row cheapest first
// from a stop that costs and is worth nothing, the presets rising, and the automatic preset over a
// sweep of budgets (shares of the table's top frame, on the table's own machine and on one twice
// as slow), each fit inside its budget and never worth more on a smaller one. All of that is
// deterministic.
//
// PRICING (`--price`) times the frame on this machine's adapter: seed --seed's map under a
// flurry (the dearest sky: heaps under a sheet, and the fall), the bot's run --from seconds in,
// seen from the named --view (`start`, its pitch raised --lift degrees so the broken sky fills
// the top of the frame and every row has something to draw; `chase` the camera behind him),
// --frames frames a picture after --warm unmeasured, each picture --rounds times in turn keeping
// its least median (a software adapter shares the machine). It draws the top
// picture, every row lowered alone to each of its stops, and every row at its cheapest (the floor),
// and prints each stop's cost over its row's cheapest (the median GPU frame with the row at the
// stop less the row at its bottom, the rest at the top; a stop measured cheaper than the one under
// it is set level, the noise), and the lines to paste into ladder.cpp's table. It shoots the view
// at every preset. Times are measured, and a software adapter's say nothing of a GPU's.
// Built only with GE_GPU.
//
// ge:summary Lab: Fall Line's quality ladder checked, its fit swept, and every stop priced
// ge:keywords lab ladder quality-ladder price-table preset auto-preset fit bench gpu-ms
// ge:keywords fall-line picture-rows stops
// ge:skill render perf labs
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "fall_line/app/frame.hpp"
#include "fall_line/app/ladder.hpp"
#include "fall_line/app/views.hpp"
#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/simulate.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "mountain/mapgen/generate.hpp"

namespace {

namespace fa = fall_line::app;
namespace gr = ge::render;
namespace mg = ge::mountain::mapgen;
using ge::lab::Det;

std::string fixed(double v, int places) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", places, v);
  return b;
}

double median(std::vector<double> v) {
  if (v.empty()) return 0;
  std::sort(v.begin(), v.end());
  return v[v.size() / 2];
}

std::string pick_text(const gr::LadderPick& pick) {
  const auto rows = fa::picture_rows();
  std::string s;
  for (size_t i = 0; i < rows.size(); ++i) {
    if (i) s += " ";
    s += std::string(rows[i].name) + "=" +
         std::string(rows[i].stops[static_cast<size_t>(pick[i])].name);
  }
  return s;
}

// The deterministic half: the table, the presets, the fit over a sweep of budgets.
void check_table(ge::lab::Lab& lab) {
  const auto rows = fa::picture_rows();
  const fa::PriceTable& table = fa::price_table();
  lab.metric("rows", static_cast<double>(rows.size()), "count");
  int stops = 0;
  for (const gr::LadderRow& row : rows) {
    stops += static_cast<int>(row.stops.size());
    bool ordered = row.stops[0].cost_ms == 0 && row.stops[0].worth == 0;
    for (size_t s = 1; s < row.stops.size(); ++s) {
      ordered = ordered && row.stops[s].cost_ms >= row.stops[s - 1].cost_ms &&
                row.stops[s].worth > row.stops[s - 1].worth;
    }
    if (!ordered) {
      lab.fail("the " + std::string(row.name) +
                   " row does not run cheapest first from a stop that costs and is worth nothing",
               "Order its stops in games/fall_line/app/ladder.cpp. Skill: render.");
    }
  }
  lab.metric("stops", stops, "count");
  ge::lab::LabTable presets{
      "The presets on " + std::string(table.machine), {"preset", "ms", "worth", "stops"}, {}};
  for (fa::Preset p : {fa::Preset::low, fa::Preset::medium, fa::Preset::high, fa::Preset::top}) {
    const gr::LadderPick pick = fa::preset_pick(p);
    const double ms = table.floor_ms + gr::ladder_cost(rows, pick);
    const double worth = gr::ladder_worth(rows, pick);
    presets.rows.push_back(
        {std::string(fa::preset_name(p)), fixed(ms, 2), fixed(worth, 0), pick_text(pick)});
    lab.metric("preset." + std::string(fa::preset_name(p)) + ".ms", ms, "ms");
    lab.metric("preset." + std::string(fa::preset_name(p)) + ".worth", worth, "worth");
  }
  lab.table(std::move(presets));

  // The fit: a machine as fast as the table's, and one twice as slow, over budgets from half as
  // much again as the table's top frame down to a fifth of it.
  const gr::LadderPick top = fa::preset_pick(fa::Preset::top);
  const double top_ms = table.floor_ms + gr::ladder_cost(rows, top);
  ge::lab::LabTable fits{"The automatic preset over budgets (the machine timed at the top)",
                         {"machine", "budget ms", "fits", "ms", "worth", "stops"},
                         {}};
  int broken = 0;
  for (double slow : {1.0, 2.0}) {
    double last = 1e18;
    for (double share : {1.5, 1.0, 0.85, 0.7, 0.55, 0.4, 0.2}) {
      const double budget = top_ms * share;
      const gr::LadderFit f = fa::auto_picture(budget, top, top_ms * slow);
      const double frame = table.floor_ms * slow + f.cost_ms;
      if (f.worth > last + 1e-9 || (f.fits && frame > budget + 1e-6)) ++broken;
      last = f.worth;
      fits.rows.push_back({slow == 1.0 ? "the table's" : "twice as slow", fixed(budget, 1),
                           f.fits ? "yes" : "no", fixed(frame, 2), fixed(f.worth, 0),
                           pick_text(f.pick)});
    }
  }
  lab.table(std::move(fits));
  lab.metric("fit-faults", broken, "count");
  if (broken > 0) {
    lab.fail(std::to_string(broken) + " fits over their budget or worth more on a smaller one",
             "Check fit_ladder in engine/render/ladder.cpp. Skill: render.");
  }
}

void price(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const int width = static_cast<int>(lab.arg_num("width"));
  const int height = static_cast<int>(lab.arg_num("height"));
  const int frames = static_cast<int>(lab.arg_num("frames"));
  const int warm = static_cast<int>(lab.arg_num("warm"));
  const double from = lab.arg_num("from");
  const double fall = lab.arg_num("fall");
  const int rounds = std::max(1, static_cast<int>(lab.arg_num("rounds")));
  const std::string view_name = lab.arg("view");
  const double lift = lab.arg_num("lift");
  const bool chase = view_name == "chase";
  const std::string camera = chase ? "The chase camera" : "The `" + view_name + "` view";
  auto device = ge::gpu::create_device({.label = "ladder", .validation = 0});
  if (!device) {
    lab.fail("no GPU device to price the ladder on",
             "Run it where an adapter is (Mesa's lavapipe, or a GPU). Skill: gpu.");
    return;
  }
  const auto& rep = device->report;
  const std::string machine = rep.name + (rep.software ? " (software)" : "");
  lab.note("adapter: " + rep.name + " / " + rep.driver + " / " + rep.backend);

  mg::GenerateOptions opts;
  opts.version = fa::kMapVersion;
  mg::Level built = mg::generate_level(seed, opts);
  built.weather.kind = mg::WeatherKind::flurries;
  built.weather.snowfall = fall;
  const auto shared = std::make_shared<const mg::Level>(std::move(built));
  const mg::Level& level = *shared;
  if (gr::FrameView probe; !chase && !fa::make_view(level, view_name, probe)) {
    lab.fail("no view named '" + view_name + "'",
             "next: pass --view=chase, or a name from games/fall_line/app/views.hpp (start, piste, "
             "vista).");
    return;
  }
  const ge::gpu::Target target = ge::gpu::create_target(
      *device, "ladder.final", static_cast<uint32_t>(width), static_cast<uint32_t>(height));

  // The run, stepped once to --from and replayed from there for every picture.
  fall_line::sim::Driver racing(shared, 1);
  auto step = [&] { racing.step(fall_line::sim::bot_controls(racing.state(), 0, racing.plan())); };
  while (racing.state().t < from && !racing.over()) step();
  std::vector<std::vector<ge::skier::SkierState>> moments;
  std::vector<double> times;
  for (int k = 0; k < warm + frames; ++k) {
    std::vector<ge::skier::SkierState> s;
    for (const auto& racer : racing.state().racers) s.push_back(racer.skier);
    moments.push_back(std::move(s));
    times.push_back(racing.state().t);
    for (int i = 0; i < 2 && !racing.over(); ++i) step();
  }

  // The median GPU frame of a picture, and its last frame's picture when `shot` names one.
  auto time = [&](const gr::LadderPick& pick, const std::string& shot) {
    auto frame = fa::create_frame(*device, level, static_cast<uint32_t>(width),
                                  static_cast<uint32_t>(height), wgpu::TextureFormat::RGBA8Unorm,
                                  fa::apply_picture(pick));
    if (!frame) return -1.0;
    gr::FrameView view;
    fa::day_view(level, -1, view);
    fa::cut(*frame);
    std::vector<double> totals;
    if (!chase) {
      fa::make_view(level, view_name, view);
      view.pitch += lift * 3.14159265358979 / 180;
    }
    for (size_t k = 0; k < moments.size(); ++k) {
      if (chase) fa::chase_view(level, moments[k][0], view);
      view.frame = k;
      view.time_s = times[k];
      fa::draw(*device, *frame, view, {moments[k], 2.0 / 120.0, k == 0}, target.view);
      frame->recorder.wait(*device);
      if (static_cast<int>(k) < warm) continue;
      double total = 0;
      const gr::FrameStats& st = frame->recorder.stats;
      for (int i = 0; i < st.count; ++i)
        if (std::isfinite(st.passes[static_cast<size_t>(i)].gpu_ms))
          total += st.passes[static_cast<size_t>(i)].gpu_ms;
      totals.push_back(total);
    }
    if (!shot.empty()) {
      lab.shot({shot,
                "PRESET " + shot + ", SEED " + fixed(seed, 0) + " UNDER A FLURRY",
                camera + " on seed " + fixed(seed, 0) + "'s run under a flurry at the " + shot +
                    " preset: " + pick_text(pick) + ".",
                "the same moment at every preset: softer pixels, coarser shadows and fewer "
                "flakes lower down, never a missing skier, tree or track",
                {{"preset", shot}, {"width", std::to_string(width)}}},
               ge::lab::Picture{width, height, ge::gpu::read_now(*device, target).rgba},
               Det::measured);
    }
    return median(totals);
  };

  // Every picture timed: the top, the floor, and each row lowered alone to each of its stops. A
  // software adapter shares its threads with everything else on the machine, so every picture is
  // timed --rounds times in turn and keeps its least median.
  const auto rows = fa::picture_rows();
  const gr::LadderPick top = fa::preset_pick(fa::Preset::top);
  std::vector<gr::LadderPick> pictures = {top, gr::LadderPick(rows.size(), 0)};
  std::vector<std::vector<size_t>> at(rows.size());
  for (size_t r = 0; r < rows.size(); ++r) {
    for (size_t s = 0; s < rows[r].stops.size(); ++s) {
      if (static_cast<int>(s) == top[r]) {
        at[r].push_back(0);
        continue;
      }
      gr::LadderPick p = top;
      p[r] = static_cast<int>(s);
      at[r].push_back(pictures.size());
      pictures.push_back(p);
    }
  }
  std::vector<double> best(pictures.size(), 1e18);
  for (int round = 0; round < rounds; ++round)
    for (size_t i = 0; i < pictures.size(); ++i) best[i] = std::min(best[i], time(pictures[i], ""));
  const double top_ms = best[0], floor_ms = best[1];
  lab.metric("price.top-ms", top_ms, "ms", Det::measured);
  lab.metric("price.floor-ms", floor_ms, "ms", Det::measured);
  ge::lab::LabTable table{"Each stop's cost over its row's cheapest, the rest at the top, on " +
                              machine + " at " + std::to_string(width) + "x" +
                              std::to_string(height),
                          {"row", "stop", "frame ms", "cost ms"},
                          {}};
  std::string paste;
  for (size_t r = 0; r < rows.size(); ++r) {
    std::vector<double> ms(rows[r].stops.size());
    for (size_t s = 0; s < rows[r].stops.size(); ++s) ms[s] = best[at[r][s]];
    double last = 0;
    paste += std::string(rows[r].name) + ":";
    for (size_t s = 0; s < ms.size(); ++s) {
      const double cost = std::max(last, ms[s] - ms[0]);
      last = cost;
      table.rows.push_back({std::string(rows[r].name), std::string(rows[r].stops[s].name),
                            fixed(ms[s], 2), fixed(cost, 2)});
      lab.metric("price." + std::string(rows[r].name) + "." + std::string(rows[r].stops[s].name),
                 cost, "ms", Det::measured);
      paste += " " + fixed(cost, 2);
    }
    paste += "\n";
  }
  lab.table(std::move(table), Det::measured);
  lab.note("the frame at the top " + fixed(top_ms, 2) + " ms, at every row's cheapest " +
           fixed(floor_ms, 2) + " ms; the costs for ladder.cpp, a row a line:\n" + paste);
  for (fa::Preset p : {fa::Preset::low, fa::Preset::medium, fa::Preset::high, fa::Preset::top}) {
    const double ms = time(fa::preset_pick(p), std::string(fa::preset_name(p)));
    lab.metric("price.preset." + std::string(fa::preset_name(p)) + "-ms", ms, "ms", Det::measured);
  }
  lab.metric("gpu-errors", device->errors.load(), "count", Det::measured);
  if (device->errors.load() != 0) {
    lab.fail(std::to_string(device->errors.load()) + " GPU errors while pricing",
             "Read the gpu: lines above (each names its object's label). Skill: render.");
  }
}

void run(ge::lab::Lab& lab) {
  check_table(lab);
  if (lab.arg_bool("price")) price(lab);
}

const ge::lab::LabSpec kSpec{
    "ladder",
    ge::lab::LabKind::bench,
    "game:fall_line",
    "games/fall_line/app/ladder.hpp: the quality ladder, its presets and the automatic preset",
    "render",
    "Does every stop of the picture's rows cost what the table says, and does the automatic "
    "preset keep the most of the look inside a budget?",
    {
        {"price", "", "time every stop on this machine's adapter, and shoot the presets", true},
        {"seed", "38", "the map's seed (with --price)"},
        {"width", "1280", "the output's width, px (with --price)"},
        {"height", "720", "the output's height, px (with --price)"},
        {"frames", "12", "frames timed a picture (with --price)"},
        {"warm", "3", "frames drawn first and not timed (with --price)"},
        {"rounds", "3", "times every picture is timed in turn, keeping its least (with --price)"},
        {"from", "10", "seconds into the bot's run (with --price)"},
        {"fall", "0.6", "the flurry's fall, 0..1 (with --price)"},
        {"view", "start", "the camera: chase, or a named view of app/views.hpp (with --price)"},
        {"lift", "15", "degrees added to a named view's pitch, to show more sky (with --price)"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
