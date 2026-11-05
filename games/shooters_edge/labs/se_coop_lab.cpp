// LAB `se-coop`: does a co-op companion play beside a person rather than for him? Our campaign in
// coop on the coop hall (games/shooters_edge/bots/coop_run.hpp): a scripted leader walks his tour
// (waits, walks to a shut door and waits by it, walks into the monsters' room and stands there,
// walks back and waits) and one companion a skill in `--skills` plays beside him. Pictured: the
// hall from above with both trails, the monsters, where each fell and the button; each companion's
// distance from the leader against time with the tour's waits shaded and the button, the door and
// the kills marked; and a ribbon of what each companion was doing (its mode, and while bored its
// idle act) against the leader's legs. The table: how near it kept, how fast it pressed the button,
// what it killed, the shots it held for him, the acts it fidgeted through.
//
// ge:summary Lab: co-op companions beside a scripted leader: follow chart, trails, mode ribbon
// ge:keywords lab se-coop coop co-op companion follow defend button bored idle leader tour
// ge:keywords bot-follow-chart ribbon
// ge:skill shooter bots labs
// ge:test tests/shooters_edge_companion_test.cpp
#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "nav_top.hpp"
#include "shooter/idle.hpp"
#include "shooters_edge/bots/coop_run.hpp"
#include "shooters_edge/server/mod_match.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace fb = shooters_edge::bots;
namespace sl = shooters_edge::labs;
using ge::core::num::to_fixed;
using In = std::vector<std::pair<std::string, std::string>>;

const gl::Rgba kInk{30, 30, 30, 255};
const gl::Rgba kLeader{210, 60, 40, 255};
const gl::Rgba kMates[3] = {{30, 90, 210, 255}, {20, 150, 90, 255}, {150, 70, 190, 255}};
const gl::Rgba kMonster{40, 40, 40, 255};
const gl::Rgba kButton{240, 170, 20, 255};
const gl::Rgba kModes[fb::kEscortModes] = {
    {120, 160, 220, 255},  // follow
    {220, 70, 50, 255},    // defend
    {240, 170, 20, 255},   // button
    {90, 90, 200, 255},    // catch-up
    {150, 200, 120, 255},  // bored
};
const gl::Rgba kActs[sh::kIdleActs] = {
    {235, 235, 230, 255}, {200, 120, 200, 255}, {110, 180, 210, 255}, {230, 200, 90, 255},
    {80, 170, 120, 255},  {220, 130, 70, 255},  {160, 160, 160, 255},
};

bool parse_skills(const std::string& text, std::vector<sh::AimSkill>& out) {
  std::stringstream in(text);
  std::string w;
  while (std::getline(in, w, ',')) {
    bool found = false;
    for (const sh::AimSkill k :
         {sh::AimSkill::novice, sh::AimSkill::average, sh::AimSkill::expert, sh::AimSkill::perfect})
      if (w == sh::aim_skill_name(k)) {
        out.push_back(k);
        found = true;
      }
    if (!found) return false;
  }
  return !out.empty();
}

void legend(gl::Picture& p, const std::vector<std::pair<std::string, gl::Rgba>>& items, int y) {
  int x = sl::kTopMargin;
  for (const auto& [name, c] : items) {
    gl::fill_rect(p, x, y + 1, 14, 6, c);
    gl::draw_text(p, x + 18, y, name, kInk);
    x += 18 + gl::text_width(name) + 16;
  }
}

gl::Picture top_view(const fb::CoopReport& r) {
  sh::MoveWorld world;
  // The hall's world is the arena's (sim/arena.hpp), drawn from the run's own graph bounds.
  static const std::vector<sh::BrushModel> models = shooters_edge::server::arena_models(true);
  sh::map_world(models[0], world);
  sl::TopView v{r.lo, r.hi};
  v.hi.z = 248;
  gl::Picture p = sl::floor_picture(world, v);
  const auto trail = [&](const std::vector<sh::Vec3f>& path, gl::Rgba c) {
    for (size_t i = 0; i + 1 < path.size(); i++) {
      const sh::Vec3f a = path[i], b = path[i + 1];
      const float dx = b.x - a.x, dy = b.y - a.y;
      if (dx * dx + dy * dy > 64 * 64) continue;  // a respawn
      gl::draw_line(p, v.px(a.x), v.py(a.y), v.px(b.x), v.py(b.y), c, 2);
    }
  };
  trail(r.leader, kLeader);
  for (size_t k = 0; k < r.mates.size(); k++) trail(r.mates[k].path, kMates[k % 3]);
  for (const sh::Vec3f& m : r.monsters) {
    gl::draw_line(p, v.px(m.x) - 6, v.py(m.y) - 6, v.px(m.x) + 6, v.py(m.y) + 6, kMonster, 2);
    gl::draw_line(p, v.px(m.x) - 6, v.py(m.y) + 6, v.px(m.x) + 6, v.py(m.y) - 6, kMonster, 2);
  }
  for (const fb::CoopKill& k : r.kills) {
    gl::draw_ring(p, v.px(k.at.x), v.py(k.at.y), 9, kModes[1]);
    gl::draw_ring(p, v.px(k.at.x), v.py(k.at.y), 10, kModes[1]);
  }
  gl::fill_rect(p, v.px(r.button_at.x) - 5, v.py(r.button_at.y) - 12, 10, 24, kButton);
  std::vector<std::pair<std::string, gl::Rgba>> items = {{"LEADER", kLeader}};
  for (size_t k = 0; k < r.mates.size(); k++) items.push_back({r.mates[k].name, kMates[k % 3]});
  items.push_back({"MONSTER (X), FELL (RING)", kMonster});
  items.push_back({"BUTTON", kButton});
  legend(p, items, 8);
  return p;
}

gl::Picture ribbon(const fb::CoopReport& r) {
  const int w = 1100, row = 22, left = 120, top = 40;
  const int rows = 1 + 2 * static_cast<int>(r.mates.size());
  gl::Picture p =
      gl::create_picture(w + left + 20, top + rows * (row + 6) + 60, {250, 250, 246, 255});
  const double span = r.t.empty() ? 1 : r.t.back();
  const auto x_of = [&](double t) { return left + t / span * w; };
  const auto band = [&](int y, const char* name, auto colour) {
    gl::draw_text(p, 8, y + 7, name, kInk);
    for (size_t i = 0; i < r.t.size(); i++) {
      const double x0 = x_of(i == 0 ? 0 : r.t[i - 1]), x1 = x_of(r.t[i]);
      gl::fill_rect(p, x0, y, x1 - x0 + 1, row, colour(i));
    }
  };
  constexpr size_t kLegs = sizeof(fb::kCoopTour) / sizeof(fb::kCoopTour[0]);
  band(top, "LEADER", [&](size_t i) {
    return fb::kCoopTour[std::min<size_t>(r.leg[i], kLegs - 1)].walk ? gl::Rgba{230, 120, 100, 255}
                                                                     : gl::Rgba{250, 210, 200, 255};
  });
  int y = top + row + 6;
  for (size_t k = 0; k < r.mates.size(); k++) {
    const fb::CoopMate& m = r.mates[k];
    band(y, (m.name + " MODE").c_str(), [&](size_t i) {
      return m.mode[i] < fb::kEscortModes ? kModes[m.mode[i]] : gl::Rgba{60, 60, 60, 255};
    });
    y += row + 6;
    band(y, (m.name + " IDLE").c_str(), [&](size_t i) {
      return m.mode[i] == static_cast<uint8_t>(fb::EscortMode::bored)
                 ? kActs[m.act[i]]
                 : gl::Rgba{250, 250, 246, 255};
    });
    y += row + 6;
  }
  for (int s = 0; s <= static_cast<int>(span); s += 5) {
    gl::draw_line(p, x_of(s), y, x_of(s), y + 5, kInk, 1);
    gl::draw_text(p, x_of(s) - 6, y + 8, std::to_string(s) + "S", kInk);
  }
  std::vector<std::pair<std::string, gl::Rgba>> items;
  items.reserve(static_cast<size_t>(fb::kEscortModes) + static_cast<size_t>(sh::kIdleActs));
  for (int k = 0; k < fb::kEscortModes; k++)
    items.push_back({fb::escort_mode_name(static_cast<fb::EscortMode>(k)), kModes[k]});
  for (int k = 1; k < sh::kIdleActs; k++)
    items.push_back({sh::idle_act_name(static_cast<sh::IdleAct>(k)), kActs[k]});
  legend(p, items, 12);
  gl::draw_text(p, sl::kTopMargin, y + 26,
                "LEADER: DARK WHILE HE WALKS, LIGHT WHILE HE WAITS. IDLE: THE ACT WHILE BORED.",
                kInk);
  return p;
}

void run(gl::Lab& lab) {
  fb::CoopOptions o;
  o.root = lab.arg("root");
  o.seed = static_cast<uint32_t>(lab.arg_num("seed"));
  o.seconds = lab.arg_num("seconds");
  o.skill = static_cast<int>(lab.arg_num("skill"));
  o.skills.clear();
  if (!parse_skills(lab.arg("skills"), o.skills)) {
    lab.fail("se-coop: --skills is a comma list of novice, average, expert, perfect",
             "next: make lab LAB=se-coop ARGS=--skills=expert,novice");
    return;
  }
  if (!lab.arg("traits").empty()) {
    std::stringstream in(lab.arg("traits"));
    std::string w;
    while (std::getline(in, w, ',')) o.traits.push_back(w);
  }
  const fb::CoopReport r = fb::run_coop(o);
  if (!r.ok) {
    lab.fail("se-coop: " + r.error, "next: run tests/shooters_edge_companion_test.cpp");
    return;
  }
  const In in = {{"skills", lab.arg("skills")},
                 {"seed", lab.arg("seed")},
                 {"seconds", lab.arg("seconds")},
                 {"skill", lab.arg("skill")}};
  double waits_by_door = -1;
  for (size_t i = 0; i < r.leg.size(); i++)
    if (r.leg[i] == 2) {
      waits_by_door = r.t[i];
      break;
    }

  gl::LabTable t{"each companion beside the leader",
                 {"companion", "skill", "near share", "p90 dist", "farthest", "kills", "deaths",
                  "threats", "shots", "held for him", "gave way s", "bored share", "idle acts"},
                 {}};
  for (const fb::CoopMate& m : r.mates) {
    std::vector<float> d = m.dist;
    int near = 0;
    for (float x : d) near += x < fb::kEscortFar ? 1 : 0;
    std::sort(d.begin(), d.end());
    const float p90 =
        d.empty() ? 0 : d[static_cast<size_t>(0.9 * static_cast<double>(d.size() - 1))];
    const float far = d.empty() ? 0 : d.back();
    int64_t all = 0;
    for (int64_t n : m.escort.commands) all += n;
    const double bored =
        all > 0 ? static_cast<double>(m.escort.commands[static_cast<int>(fb::EscortMode::bored)]) /
                      static_cast<double>(all)
                : 0;
    std::string acts;
    int64_t act_count = 0;
    for (int k = 1; k < sh::kIdleActs; k++) {
      if (m.idle.acts[k] == 0) continue;
      if (!acts.empty()) acts += " ";
      acts += std::string(sh::idle_act_name(static_cast<sh::IdleAct>(k))) + "x" +
              std::to_string(m.idle.acts[k]);
      act_count += m.idle.acts[k];
    }
    const double share = d.empty() ? 0 : static_cast<double>(near) / static_cast<double>(d.size());
    t.rows.push_back({m.name, sh::aim_skill_name(m.skill), to_fixed(share, 3), to_fixed(p90, 0),
                      to_fixed(far, 0), std::to_string(m.kills), std::to_string(m.deaths),
                      std::to_string(m.escort.threats), std::to_string(m.fighter.shots),
                      std::to_string(m.escort.shots_held),
                      to_fixed(static_cast<double>(m.escort.gave_way) * 14 / 1000.0, 1),
                      to_fixed(bored, 3), acts});
    const std::string key = "coop." + m.name;
    lab.metric(key + ".near-share", share, "share");
    lab.metric(key + ".dist-p90", p90, "units");
    lab.metric(key + ".farthest", far, "units");
    lab.metric(key + ".kills", m.kills, "count");
    lab.metric(key + ".deaths", m.deaths, "count");
    lab.metric(key + ".shots-held", static_cast<double>(m.escort.shots_held), "count");
    lab.metric(key + ".idle-acts", static_cast<double>(act_count), "count");
  }
  lab.metric(
      "coop.button-delay",
      r.button_pressed_at >= 0 && waits_by_door >= 0 ? r.button_pressed_at - waits_by_door : -1,
      "s");
  lab.metric("coop.door-open-at", r.door_open_at, "s");
  lab.metric("coop.killed", static_cast<double>(r.kills.size()), "count");
  lab.metric("coop.monsters", r.monsters_total, "count");
  lab.metric("coop.leader-deaths", r.leader_deaths, "count");
  lab.metric("coop.faults", static_cast<double>(r.faults), "count");
  lab.table(t);

  lab.shot({"hall", "THE COOP HALL FROM ABOVE",
            "To scale from above, " + to_fixed(r.t.empty() ? 0 : r.t.back(), 0) +
                " s of our campaign in coop: the leader's tour (red) from the start in the west "
                "room to the shut door, into the east room's monsters and back, and each "
                "companion's trail beside him. The monsters' starts are crosses, where each fell "
                "a ring; the button (amber) on the dividing wall opens the door.",
            "the companion's trail hugs the leader's and goes to the button once; every ring "
            "near the doorway or the leader, none far into the east room before he went in",
            in},
           top_view(r));

  gl::LineChart ch;
  ch.spikes = false;
  ch.frame = {"EACH COMPANION'S DISTANCE FROM THE LEADER", "seed " + lab.arg("seed"), "t (s)",
              "distance (units)"};
  for (const fb::CoopMate& m : r.mates) {
    gl::Trace tr{m.name + " (" + sh::aim_skill_name(m.skill) + ")", {}, {}};
    for (size_t i = 0; i < m.dist.size(); i++) {
      tr.x.push_back(r.t[i]);
      tr.y.push_back(m.dist[i]);
    }
    ch.traces.push_back(tr);
  }
  ch.rules = {fb::kEscortNear, fb::kEscortFar};
  constexpr size_t kLegs = sizeof(fb::kCoopTour) / sizeof(fb::kCoopTour[0]);
  for (size_t i = 0; i < r.leg.size();) {
    size_t j = i;
    while (j < r.leg.size() && r.leg[j] == r.leg[i]) j++;
    const fb::TourLeg& leg = fb::kCoopTour[std::min<size_t>(r.leg[i], kLegs - 1)];
    if (!leg.walk) ch.bands.push_back({r.t[i], r.t[j - 1], leg.name});
    i = j;
  }
  if (r.button_pressed_at >= 0) ch.marks.push_back({r.button_pressed_at, "button"});
  if (r.door_open_at >= 0) ch.marks.push_back({r.door_open_at, "door"});
  for (const fb::CoopKill& k : r.kills) ch.marks.push_back({k.t, "kill"});
  lab.shot({"follow", ch.frame.title,
            "How far each companion stood from the leader at every frame. Shaded: the leader "
            "waiting (his tour's waits); dashed: the shoulder spot (" +
                to_fixed(fb::kEscortNear, 0) + ") and the follow distance (" +
                to_fixed(fb::kEscortFar, 0) +
                "); marked: the button pressed, the door open and each kill.",
            "the line under the upper dash nearly all the time; a rise only on the button, a "
            "bored hop and the leader setting off; the kills soon after the door opens",
            in},
           gl::to_picture(gl::line_chart(ch).plot));

  lab.shot({"ribbon", "WHAT EACH COMPANION WAS DOING",
            "Against time, the leader's legs (walking dark, waiting light) and for each "
            "companion its mode a frame and, while bored, the idle act it was in.",
            "bored only inside the leader's waits and gone soon after each ends; the button "
            "inside the wait by the door; defend after the door opens",
            in},
           ribbon(r));
}

const gl::LabSpec kSpec{
    "se-coop",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "games/shooters_edge/bots/companion.hpp (escort_think, escort_command) and "
    "plugins/shooter/idle.hpp through coop_run.hpp: our campaign in coop on the coop hall",
    "bots",
    "Does a co-op companion keep close to the person it plays with, clear only what comes for "
    "him, press the buttons he needs, and fidget like a person while he waits, rather than play "
    "the level for him?",
    {
        {"skills", "expert",
         "one companion a skill, a comma list: novice, average, expert, perfect"},
        {"traits", "", "companion k's traits over its skill's preset, a comma list (traits.hpp)"},
        {"seconds", "60", "the run's length (the tour ends at about 52 s)"},
        {"seed", "1", "the campaign's and the companions' seed"},
        {"skill", "1", "the campaign's skill (0..3): how many monsters and how hard"},
        {"root", ".", "the repository's root (the campaign's sources are under it)"},
    },
    "--seconds=40",
    run,
};
GE_LAB(kSpec);

}  // namespace
