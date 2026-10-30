// LAB `se-senses`: does a bot see what a player would, and aim like its skill? On the server's
// arena a seer (plugins/shooter/bot.hpp) and a runner (a plain pilot) fill two bot slots and visit
// the arena's goals; the run is played once per aim skill (perfect, expert, average, novice). Per
// skill: the two trails from above with a sight line drawn every quarter second the seer sees the
// runner, the aim's error against time, and a table of how often a clear line joined them, how
// often the seer saw the runner, how it acquired and lost it, its aim error, its reaction and the
// goals it reached while looking. The truth (a clear line, the error) is measured beside the run
// and fed nothing back.
//
// ge:summary Lab: the bot's sight and aim on the arena, per skill: trails, sight lines, error
// ge:keywords lab se-senses senses sight aim skill error reaction sight-lines arena duel
// ge:skill shooter bots labs
// ge:test tests/shooter_senses_test.cpp
#include <algorithm>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "nav_top.hpp"
#include "shooters_edge/bots/duel_run.hpp"
#include "shooters_edge/bots/pilot_run.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace pr = shooters_edge::bots;
namespace sl = shooters_edge::labs;
using ge::core::num::to_fixed;
using In = std::vector<std::pair<std::string, std::string>>;

const gl::Rgba kSeer{30, 90, 210, 255};
const gl::Rgba kRunner{210, 60, 40, 255};
const gl::Rgba kSight{240, 170, 20, 255};

float quantile(std::vector<float> v, float q) {
  if (v.empty()) return -1;
  std::sort(v.begin(), v.end());
  return v[static_cast<size_t>(q * static_cast<float>(v.size() - 1))];
}

void legend(gl::Picture& p) {
  int x = sl::kTopMargin;
  const std::pair<const char*, gl::Rgba> items[3] = {
      {"SEER", kSeer}, {"RUNNER", kRunner}, {"SEEN (A LINE EVERY QUARTER SECOND)", kSight}};
  for (const auto& [name, c] : items) {
    gl::fill_rect(p, x, 8, 14, 6, c);
    gl::draw_text(p, x + 18, 7, name, {30, 30, 30, 255});
    x += 18 + gl::text_width(name) + 16;
  }
}

void run(gl::Lab& lab) {
  const int frames = static_cast<int>(lab.arg_num("frames"));
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  if (frames < 1) {
    lab.fail("se-senses: --frames must be at least 1", "next: make lab LAB=se-senses ARGS=--help");
    return;
  }
  const In in = {{"frames", lab.arg("frames")}, {"seed", lab.arg("seed")}};
  const sh::AimSkill skills[4] = {sh::AimSkill::perfect, sh::AimSkill::expert,
                                  sh::AimSkill::average, sh::AimSkill::novice};
  gl::LabTable t{"the seer per aim skill",
                 {"skill", "line frames", "seen", "seen per line", "acquired", "lost",
                  "error median deg", "error p90 deg", "reaction median s", "goals"},
                 {}};
  gl::LineChart ch;
  ch.spikes = false;
  ch.frame = {"THE AIM'S ERROR WHILE THE RUNNER IS SEEN", "seed " + lab.arg("seed"), "t (s)",
              "error (degrees)"};
  for (const sh::AimSkill k : skills) {
    pr::Duel d;
    if (!pr::make_duel(d, k, seed)) {
      lab.fail("se-senses: the arena's host did not start",
               "next: run tests/shooters_edge_server_test.cpp");
      return;
    }
    const pr::DuelResult r = pr::run_duel(d, frames);
    const std::string name = sh::aim_skill_name(k);
    const float share = r.line_frames > 0
                            ? static_cast<float>(r.seen_frames) / static_cast<float>(r.line_frames)
                            : 0;
    const float med = quantile(r.error_deg, 0.5f), p90 = quantile(r.error_deg, 0.9f);
    const float react = quantile(r.reaction_s, 0.5f);
    t.rows.push_back({name, std::to_string(r.line_frames), std::to_string(r.seen_frames),
                      to_fixed(share, 2), std::to_string(r.senses.acquired),
                      std::to_string(r.senses.lost), to_fixed(med, 2), to_fixed(p90, 2),
                      to_fixed(react, 3), std::to_string(d.seer.pilot.arrivals)});
    const std::string key = "senses." + name;
    lab.metric(key + ".seen-share", share, "share");
    lab.metric(key + ".error-median", med, "deg");
    lab.metric(key + ".error-p90", p90, "deg");
    lab.metric(key + ".reaction-median", react, "s");
    lab.metric(key + ".goals", d.seer.pilot.arrivals, "count");

    gl::Trace tr{name, {}, {}};
    for (size_t i = 0, e = 0; i < r.seen.size(); i++) {
      if (!r.seen[i]) continue;
      tr.x.push_back(static_cast<double>(i) * pr::kPilotMsec * 0.001);
      tr.y.push_back(r.error_deg[e++]);
    }
    ch.traces.push_back(tr);

    sh::MoveWorld world;
    sh::map_world(d.host.models[0], world);
    sl::TopView v{d.graph.lo, d.graph.hi};
    v.hi.z = 248;  // traced from under the server's roof (z 256), which would cover the floor
    gl::Picture p = sl::floor_picture(world, v);
    const int every = static_cast<int>(250 / pr::kPilotMsec);
    for (size_t i = 0; i < r.seen.size(); i += static_cast<size_t>(every))
      if (r.seen[i])
        gl::draw_line(p, v.px(r.seer[i].x), v.py(r.seer[i].y), v.px(r.runner[i].x),
                      v.py(r.runner[i].y), kSight, 1);
    for (size_t i = 0; i + 1 < r.seer.size(); i++) {
      gl::draw_line(p, v.px(r.seer[i].x), v.py(r.seer[i].y), v.px(r.seer[i + 1].x),
                    v.py(r.seer[i + 1].y), kSeer, 2);
      gl::draw_line(p, v.px(r.runner[i].x), v.py(r.runner[i].y), v.px(r.runner[i + 1].x),
                    v.py(r.runner[i + 1].y), kRunner, 2);
    }
    legend(p);
    lab.shot({"arena-" + name,
              "THE SEER WITH " +
                  std::string(k == sh::AimSkill::perfect   ? "PERFECT"
                              : k == sh::AimSkill::expert  ? "EXPERT"
                              : k == sh::AimSkill::average ? "AVERAGE"
                                                           : "NOVICE") +
                  " AIM",
              "To scale from above, the arena on the server for " + std::to_string(frames) +
                  " frames: the seer's trail (blue) and the runner's (red), each visiting the "
                  "arena's goals, and a sight line every quarter second the seer's senses saw "
                  "the runner. It saw it on " +
                  std::to_string(r.seen_frames) + " frames of the " +
                  std::to_string(r.line_frames) + " a clear line joined them.",
              "sight lines only where nothing stands between: none through the dividing wall "
              "or a shut door, none to a runner behind the seer's back",
              in},
             p);
  }
  lab.table(t);
  lab.shot({"error", ch.frame.title,
            "The angle between the seer's view and the runner's middle at each frame the "
            "runner was seen, one trace a skill.",
            "each acquisition a spike that falls away; the perfect trace near zero throughout, "
            "the expert's lowest of the others and the novice's highest",
            in},
           gl::to_picture(gl::line_chart(ch).plot));
}

const gl::LabSpec kSpec{
    "se-senses",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "plugins/shooter/senses.hpp and aim.hpp (client_view, sense, aim_profile) through bot.hpp on "
    "the server's arena (games/shooters_edge/bots/duel_run.hpp)",
    "bots",
    "Does a bot see only what a player's client could (in its view's cone, with a clear line), "
    "and does its aim at what it sees follow its skill: how far off, how late?",
    {
        {"frames", "2400", "the arena's server frames each skill's run lasts"},
        {"seed", "1", "the bots' hands and the server's seed"},
    },
    "--frames=1200",
    run,
};
GE_LAB(kSpec);

}  // namespace
