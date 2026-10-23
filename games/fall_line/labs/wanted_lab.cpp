// LAB `wanted`: the wanted level on a real map. The bot skis a seed's run while scripted pursuers
// hunt him, standing in for the army's machines until they exist, and the hunt runs through the
// real `step` (`step_wanted`): THE HUNT table (per scenario, the stars reached and when, the share
// of the run he was seen, the searches, how it ended, the time survived) and, per scenario, the
// map with his line coloured by the stars and the pursuer's path over it. Two SIGHT SHEDS show
// what a soldier at the start gate and a helicopter over the map's middle can see: every spot of
// the map a man could stand on, open, hidden by the ground, hidden by the woods, or out of range.
//
// Scenarios: `shadow` (a helicopter trailing him 120 m up and 4 s behind: seen throughout, the
// heat climbs), `posts` (a soldier at the start gate and a snowmobile crew at the finish: he skis
// out of the start post's sight, they search, and he is lost or found again by the finish post),
// `arrest` (the army not shooting to kill, a soldier on skis closing on him until he takes him in).
//
// ge:summary Lab: the wanted level on a real map: scripted pursuers, the hunt table, sight sheds
// ge:keywords lab wanted wanted-level stars hunt pursuer sight-shed line-of-sight search arrest
// ge:keywords scenario shadow posts
// ge:skill sim labs
// ge:test tests/fall_line_wanted_test.cpp
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "core/num_text.hpp"
#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/wanted.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/labs/level_map.hpp"
#include "mountain/mapgen/generate.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
using ge::lab::Rgba;

std::string fixed(double v, int d) { return ge::core::num::to_fixed(v, d); }

enum class Scene : uint8_t { shadow, posts, arrest };
constexpr Scene kScenes[] = {Scene::shadow, Scene::posts, Scene::arrest};

const char* scene_name(Scene s) {
  return s == Scene::shadow ? "shadow" : s == Scene::posts ? "posts" : "arrest";
}

// The heat is raised this long into the run, s, once he is under way.
constexpr double kRaiseAt = 5;
// The shadowing helicopter: this high over him, m, and this far behind on his own line, s.
constexpr double kShadowUp = 120;
constexpr double kShadowLag = 4;
// The soldier closing for the arrest starts this far behind on his line, s, and closes one second
// of it every `kCloseEvery` s of the hunt.
constexpr double kArrestLag = 8;
constexpr double kCloseEvery = 8;
// A soldier's eyes over the snow, m.
constexpr double kEyeM = 1.7;
// After the finish (or the hunt's end) the run goes on this long, s.
constexpr double kAfter = 5;

struct Sample {
  double x, z;
  int32_t stars;
  bool seen;
};

struct HuntRun {
  std::vector<Sample> line;                  // every tenth of a second
  std::vector<std::vector<fs::Spot>> paths;  // each pursuer's, every tenth of a second
  fs::WantedState wanted;
  double t = 0;
  double seen_s = 0;  // s of the hunt he was seen
  int32_t searches = 0;
  int32_t finds = 0;
};

HuntRun run_scene(const std::shared_ptr<const mg::Level>& level, Scene scene, fs::WantedCause cause,
                  double limit) {
  fs::GameState st = fs::create_game_on(level, 1);
  // Staged: the scripted pursuers alone: the army's helicopters are the army-flight lab's.
  st.army.send_helis = false;
  st.army.fire = false;
  const fs::BotPlan plan = fs::plan_bot(*level, st.spec);
  st.wanted.lethal = scene != Scene::arrest;
  HuntRun out;
  std::vector<fs::Spot> trail;  // his feet every step
  trail.reserve(static_cast<size_t>(limit * fs::kHz) + 1);
  const mg::Checkpoint& start = level->checkpoints.front();
  const mg::Checkpoint& finish = level->checkpoints.back();
  bool raised = false;
  double over_at = -1;
  const auto behind = [&trail](double lag_s) {
    const auto back = static_cast<size_t>(lag_s * fs::kHz);
    return trail[trail.size() > back ? trail.size() - 1 - back : 0];
  };
  while (st.t < limit) {
    trail.push_back(fs::quarry_of(st));
    if (!raised && st.t >= kRaiseAt) {
      fs::raise_heat(st, cause);
      raised = true;
    }
    auto& ps = st.wanted.pursuers;
    ps.clear();
    switch (scene) {
      case Scene::shadow: {
        const fs::Spot b = behind(kShadowLag);
        ps.push_back(fs::Pursuer{1, fs::PursuerKind::helicopter, b.x, b.y + kShadowUp, b.z});
        break;
      }
      case Scene::posts:
        ps.push_back(fs::Pursuer{1, fs::PursuerKind::soldier, start.x, start.y + kEyeM, start.z});
        ps.push_back(
            fs::Pursuer{2, fs::PursuerKind::snowmobile, finish.x, finish.y + kEyeM, finish.z});
        break;
      case Scene::arrest: {
        const double hunted = raised ? st.t - kRaiseAt : 0;
        const double lag = kArrestLag - hunted / kCloseEvery;
        const fs::Spot b = behind(lag > 0 ? lag : 0);
        ps.push_back(fs::Pursuer{1, fs::PursuerKind::soldier, b.x, b.y + kEyeM, b.z});
        break;
      }
    }
    const fs::Controls c = fs::bot_controls(st, 0, plan);
    fs::step(st, std::span<const fs::Controls>(&c, 1));
    for (const fs::GameEvent& e : st.events) {
      if (e.kind != fs::EventKind::wanted) continue;
      out.searches += fs::wanted_change(e) == fs::WantedChange::searching;
      out.finds += fs::wanted_change(e) == fs::WantedChange::seen;
    }
    const fs::WantedState& w = st.wanted;
    const bool hunted = w.active && w.ended == fs::WantedEnd::running && w.stars > 0;
    if (hunted && w.seen) out.seen_s += fs::kDt;
    if (scene == Scene::arrest && hunted) {
      const fs::Spot q = fs::quarry_of(st);
      const fs::Pursuer& p = ps.front();
      if (ge::core::hypot(p.x - q.x, p.z - q.z) <= fs::wanted::kArrestReachM) fs::arrest_quarry(st);
    }
    if (st.steps % 12 == 0) {
      const fs::Spot q = fs::quarry_of(st);
      out.line.push_back(Sample{q.x, q.z, hunted ? w.stars : 0, w.seen});
      if (out.paths.size() < ps.size()) out.paths.resize(ps.size());
      for (size_t i = 0; i < ps.size(); i++) out.paths[i].push_back({ps[i].x, ps[i].y, ps[i].z});
    }
    const bool done = st.racers.front().progress.finished || w.ended == fs::WantedEnd::arrested;
    if (done && over_at < 0) over_at = st.t;
    if (over_at >= 0 && st.t >= over_at + kAfter) break;
  }
  out.wanted = st.wanted;
  out.t = st.t;
  return out;
}

// The stars as a colour, clear of the map's own red piste and green woods: white with none,
// yellow, orange, magenta, violet and black from one to five; grey while they search.
Rgba star_colour(int32_t stars, bool seen) {
  if (stars <= 0) return Rgba{255, 255, 255};
  if (!seen) return Rgba{150, 150, 150};
  static const Rgba kRamp[] = {
      {255, 230, 0}, {255, 140, 0}, {255, 0, 200}, {120, 0, 255}, {0, 0, 0}};
  return kRamp[stars - 1];
}

// Mixes `c` over the picture's pixel at (x, y) by `a` of 255.
void tint(ge::lab::Picture& p, int x, int y, Rgba c, int a) {
  uint8_t* px =
      &p.rgba[(static_cast<size_t>(y) * static_cast<size_t>(p.width) + static_cast<size_t>(x)) * 4];
  px[0] = static_cast<uint8_t>((px[0] * (255 - a) + c.r * a) / 255);
  px[1] = static_cast<uint8_t>((px[1] * (255 - a) + c.g * a) / 255);
  px[2] = static_cast<uint8_t>((px[2] * (255 - a) + c.b * a) / 255);
}

struct Shed {
  int32_t open = 0, ground = 0, woods = 0, range = 0;
};

// THE SIGHT SHED of `p` over the map: every `cell_px` square tinted by what a man standing at its
// middle is to him.
Shed draw_shed(ge::lab::Picture& pic, const mg::Level& level, const ge::skier::TreeIndex& trees,
               const fs::Pursuer& p, int cell_px) {
  const double k = pic.width / level.size;
  const ge::skier::TreeIndex bare;  // no trees: the ground alone
  Shed shed;
  for (int cy = 0; cy < pic.height; cy += cell_px) {
    for (int cx = 0; cx < pic.width; cx += cell_px) {
      const double x = (cx + cell_px * 0.5) / k;
      const double z = (cy + cell_px * 0.5) / k;
      const fs::Spot q{x, level.ground_at(x, z), z};
      const double dx = q.x - p.x;
      const double dy = q.y + fs::wanted::kChestM - p.y;
      const double dz = q.z - p.z;
      const double d2 = dx * dx + dy * dy + dz * dz;
      const double sight = fs::wanted::kSightM[static_cast<size_t>(p.kind)];
      Rgba c{0, 0, 0};
      int a = 0;
      if (d2 > sight * sight) {
        c = Rgba{40, 40, 40};
        a = 150;
        shed.range++;
      } else if (!fs::sees_quarry(level, bare, p, q, false)) {
        c = Rgba{0, 20, 120};
        a = 150;
        shed.ground++;
      } else if (!fs::sees_quarry(level, trees, p, q, false)) {
        c = Rgba{150, 70, 0};
        a = 150;
        shed.woods++;
      } else {
        c = Rgba{120, 255, 120};
        a = 60;
        shed.open++;
      }
      for (int y = cy; y < cy + cell_px && y < pic.height; y++) {
        for (int x2 = cx; x2 < cx + cell_px && x2 < pic.width; x2++) tint(pic, x2, y, c, a);
      }
    }
  }
  ge::lab::fill_disc(pic, p.x * k, p.z * k, 5, Rgba{255, 0, 255});
  return shed;
}

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const auto version = static_cast<int>(lab.arg_num("version"));
  const double limit = lab.arg_num("limit");
  const int px = static_cast<int>(lab.arg_num("px"));
  const bool pictures = !lab.arg_bool("no-pictures");
  std::shared_ptr<const mg::Level> level;
  try {
    level = std::make_shared<const mg::Level>(fs::race_map(seed, version));
  } catch (const mg::GenerateError& e) {
    lab.fail("seed " + fixed(seed, 0) + " built no map: " + e.what(),
             "Run `make lab LAB=analyze ARGS=--seeds=" + fixed(seed, 0) +
                 " --refusals` (skill: mountain).");
    return;
  }
  const fs::WantedCause cause = fs::WantedCause::stole_police;
  ge::lab::LabTable table{"hunt",
                          {"scenario", "peak", "star 1 s", "star 2 s", "star 3 s", "star 4 s",
                           "star 5 s", "seen %", "searches", "finds", "end", "survived s"},
                          {}};
  const double k = px / level->size;
  for (const Scene scene : kScenes) {
    const std::string id = scene_name(scene);
    const HuntRun r = run_scene(level, scene, cause, limit);
    const fs::WantedState& w = r.wanted;
    const double survived = fs::survived_of(w, r.t);
    std::vector<std::string> row{id, std::to_string(w.peak)};
    for (size_t s = 1; s <= 5; s++) row.push_back(w.reached[s] >= 0 ? fixed(w.reached[s], 2) : "-");
    row.push_back(fixed(survived > 0 ? 100 * r.seen_s / survived : 0, 1));
    row.push_back(std::to_string(r.searches));
    row.push_back(std::to_string(r.finds));
    row.push_back(fs::wanted_end_name(w.ended));
    row.push_back(fixed(survived, 2));
    table.rows.push_back(row);
    lab.metric(id + ".peak", w.peak, "stars");
    lab.metric(id + ".survived", survived, "s");
    lab.metric(id + ".seen-share", survived > 0 ? r.seen_s / survived : 0, "share");
    lab.metric(id + ".searches", r.searches, "count");
    if (scene == Scene::shadow && w.peak < 3) {
      lab.fail("shadow: a helicopter in sight all run raised him only to " +
                   std::to_string(w.peak) + " stars",
               "Read the climb in games/fall_line/sim/wanted.cpp (step_wanted) and run "
               "build/debug/tests/fall_line_wanted_test (skill: sim).");
    }
    if (scene == Scene::arrest && w.ended != fs::WantedEnd::arrested) {
      lab.fail("arrest: the soldier closing on him never took him in",
               "Look at previews/wanted/arrest.png for the soldier's path; read arrest_quarry in "
               "games/fall_line/sim/wanted.cpp (skill: sim).");
    }
    if (!pictures) continue;
    ge::lab::Picture pic = ge::mountain::labs::draw_map(*level, px);
    for (const auto& path : r.paths) {
      for (size_t i = 1; i < path.size(); i++) {
        ge::lab::draw_line(pic, path[i - 1].x * k, path[i - 1].z * k, path[i].x * k, path[i].z * k,
                           Rgba{0, 200, 255}, 1);
      }
    }
    for (size_t i = 1; i < r.line.size(); i++) {
      const Sample& a = r.line[i - 1];
      const Sample& b = r.line[i];
      ge::lab::draw_line(pic, a.x * k, a.z * k, b.x * k, b.z * k, star_colour(b.stars, b.seen), 2);
    }
    lab.shot({id,
              "SEED " + fixed(seed, 0) + " HUNT: " + std::string(scene_name(scene)),
              "The map from above (+x right, +z down; the piste red, trees dark green) with the "
              "bot's line: white with no stars, then by the stars while seen (yellow 1, orange 2, "
              "magenta 3, violet 4, black 5), grey while they search; the "
              "pursuers' paths light blue. Peak " +
                  std::to_string(w.peak) + " stars, " + fs::wanted_end_name(w.ended) + " after " +
                  fixed(survived, 1) + " s.",
              scene == Scene::shadow ? "the line orange from the start, then magenta, violet "
                                       "and black as the stars climb; no grey; the blue path "
                                       "trailing the line"
              : scene == Scene::posts
                  ? "orange from the start until the brow below the start post hides him, then "
                    "grey while they search, white once the search runs out (or orange again "
                    "if the finish post finds him first)"
                  : "the line coloured to where the soldier takes him in, then white for the "
                    "last seconds; grey only where a crest hides him from the soldier behind",
              {{"seed", fixed(seed, 0)},
               {"version", lab.arg("version")},
               {"scenario", id},
               {"cause", fs::wanted_cause_name(cause)},
               {"raise at s", fixed(kRaiseAt, 0)}}},
             pic);
  }
  lab.table(std::move(table));
  if (!pictures) return;
  const ge::skier::TreeIndex trees = ge::skier::build_tree_index(*level);
  const mg::Checkpoint& start = level->checkpoints.front();
  const double mid = level->size / 2;
  const fs::Pursuer sheds[] = {
      {1, fs::PursuerKind::soldier, start.x, start.y + kEyeM, start.z},
      {2, fs::PursuerKind::helicopter, mid, level->ground_at(mid, mid) + 150, mid},
  };
  const int cell = px / 128 > 1 ? px / 128 : 1;
  for (const fs::Pursuer& p : sheds) {
    const bool soldier = p.kind == fs::PursuerKind::soldier;
    const std::string id = soldier ? "shed-soldier" : "shed-helicopter";
    ge::lab::Picture pic = ge::mountain::labs::draw_map(*level, px);
    const Shed s = draw_shed(pic, *level, trees, p, cell);
    const double all = s.open + s.ground + s.woods + s.range;
    lab.metric(id + ".open", s.open / all, "share");
    lab.metric(id + ".ground", s.ground / all, "share");
    lab.metric(id + ".woods", s.woods / all, "share");
    lab.shot({id,
              "SEED " + fixed(seed, 0) +
                  " SIGHT SHED: " + (soldier ? "SOLDIER AT THE START" : "HELICOPTER 150 M UP"),
              std::string("What a ") +
                  (soldier ? "soldier at the start gate (eyes 1.7 m up)"
                           : "helicopter crew 150 m over the map's middle") +
                  " can see of a man standing anywhere on the map (magenta dot: the pursuer). "
                  "Green tint open, blue hidden by the ground, brown hidden by the woods, grey "
                  "out of range (" +
                  fixed(fs::wanted::kSightM[static_cast<size_t>(p.kind)], 0) + " m). Open " +
                  fixed(100 * s.open / all, 0) + "%, ground " + fixed(100 * s.ground / all, 0) +
                  "%, woods " + fixed(100 * s.woods / all, 0) + "%.",
              soldier ? "blue behind every crest and below every brow from the start; brown "
                        "only in or behind the forest; open down the piste in view"
                      : "almost all green inside its range: the woods hide nothing from the air; "
                        "blue only behind high ridges",
              {{"seed", fixed(seed, 0)}, {"version", lab.arg("version")}, {"pursuer", id}}},
             pic);
  }
}

const ge::lab::LabSpec kSpec{
    "wanted",
    ge::lab::LabKind::scenario,
    "game:fall_line",
    "games/fall_line/sim/wanted (the wanted level: the climb, the look, the search, the arrest)",
    "sim",
    "On a real map, how does a hunt climb, search and end against scripted pursuers, and what can "
    "a soldier on the snow and a helicopter's crew see?",
    {
        {"seed", "38", "the map's seed"},
        {"version", "1", "the generator's version"},
        {"limit", "400", "each scenario's time limit, s"},
        {"px", "768", "the pictures' size, px"},
        {"no-pictures", "", "skip the pictures", true},
    },
    "--px=256",
    run,
};
GE_LAB(kSpec);

}  // namespace
