// LAB `se-bots`: do the fighters play the baseline mod's deathmatch like players? Four fighters
// (fight_run.hpp: an expert, two average and a novice aim by default) fill the arena's client
// slots on the mod compiled from its QC sources, and play for `--frames` server frames. Pictured:
// the arena from above with every trail, every item and every death; each fighter's mode through
// the match as a strip (roam, fight, chase, retreat, dead); and the frags against time. The table
// gives each fighter's frags, deaths, shots, pickups, weapon switches, its share of time in each
// mode and the weapon it held most.
//
// ge:summary Lab: fighters' deathmatch on the baseline mod: trails, deaths, modes, frags by skill
// ge:keywords lab se-bots fighters deathmatch frags deaths modes items baseline-mod arena
// ge:skill shooter bots labs
// ge:test tests/shooters_edge_fight_test.cpp
#include <algorithm>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "nav_top.hpp"
#include "shooters_edge/bots/fight_run.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/server/mod_match.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace fb = shooters_edge::bots;
namespace sl = shooters_edge::labs;
using ge::core::num::to_fixed;
using In = std::vector<std::pair<std::string, std::string>>;

const gl::Rgba kPlayer[4] = {
    {30, 90, 210, 255}, {210, 60, 40, 255}, {40, 150, 70, 255}, {150, 70, 190, 255}};
const gl::Rgba kMode[5] = {{150, 160, 170, 255},  // roam
                           {220, 60, 40, 255},    // fight
                           {240, 160, 20, 255},   // chase
                           {40, 130, 220, 255},   // retreat
                           {40, 40, 40, 255}};    // dead
const gl::Rgba kInk{30, 30, 30, 255};

gl::Rgba player_colour(size_t k) { return kPlayer[k % 4]; }

void run(gl::Lab& lab) {
  const int frames = static_cast<int>(lab.arg_num("frames"));
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  if (frames < 1) {
    lab.fail("se-bots: --frames must be at least 1", "next: make lab LAB=se-bots ARGS=--help");
    return;
  }
  const In in = {{"frames", lab.arg("frames")}, {"seed", lab.arg("seed")}};
  fb::FightOptions o;
  o.frames = frames;
  o.seed = seed;
  o.root = lab.arg("root");
  // The skills and temperaments, one a fighter, comma separated (empty: the defaults).
  const auto split = [](const std::string& list) {
    std::vector<std::string> out;
    size_t at = 0;
    while (!list.empty() && at <= list.size()) {
      const size_t end = std::min(list.find(',', at), list.size());
      out.push_back(list.substr(at, end - at));
      at = end + 1;
    }
    return out;
  };
  if (!lab.arg("skills").empty()) {
    o.skills.clear();
    for (const std::string& w : split(lab.arg("skills"))) {
      bool named = false;
      sh::AimSkill k = sh::AimSkill::average;
      for (sh::AimSkill c : {sh::AimSkill::novice, sh::AimSkill::average, sh::AimSkill::expert,
                             sh::AimSkill::perfect})
        if (w == sh::aim_skill_name(c)) {
          k = c;
          named = true;
        }
      if (!named) {
        lab.fail("se-bots: --skills: '" + w + "' is no skill",
                 "next: novice, average, expert or perfect, one a fighter");
        return;
      }
      o.skills.push_back(k);
    }
  }
  for (const std::string& w : split(lab.arg("tempers"))) {
    sh::Temper tp;
    if (!sh::temper_of(w, tp)) {
      lab.fail("se-bots: --tempers: '" + w + "' is no temperament",
               "next: aggressive, balanced or coward (a, b, c), one a fighter");
      return;
    }
    o.tempers.push_back(tp);
  }
  const fb::FightReport r = fb::run_fight(o);
  if (!r.ok) {
    lab.fail("se-bots: " + r.error, "next: run tests/shooters_edge_fight_test.cpp");
    return;
  }
  const double secs = r.frames * fb::kPilotMsec * 0.001;

  gl::LabTable t{"each fighter over the match",
                 {"fighter", "aim", "temperament", "frags", "deaths", "shots", "pickups",
                  "switches", "roam", "fight", "chase", "retreat", "held most"},
                 {}};
  static const char* kWeapon[9] = {
      "", "blade", "scattergun", "double", "spiker", "heavy spiker", "grenades", "rockets", "arc"};
  float total = 0;
  for (size_t k = 0; k < r.players.size(); k++) {
    const fb::FightPlayer& p = r.players[k];
    const fb::FighterStats& s = p.stats;
    int64_t alive = 0;
    for (int m = 0; m < 4; m++) alive += s.mode_commands[m];
    const auto share = [&](int m) {
      return to_fixed(
          alive > 0 ? static_cast<double>(s.mode_commands[m]) / static_cast<double>(alive) : 0, 2);
    };
    int most = 1;
    for (int w = 1; w <= 8; w++)
      if (s.weapon_commands[w] > s.weapon_commands[most]) most = w;
    t.rows.push_back({p.name, sh::aim_skill_name(p.skill), sh::temper_name(p.temper),
                      to_fixed(p.frags, 0), std::to_string(p.deaths), std::to_string(s.shots),
                      std::to_string(s.pickups), std::to_string(s.switches), share(0), share(1),
                      share(2), share(3), kWeapon[most]});
    const std::string key = "fight." + p.name;
    lab.metric(key + ".frags", p.frags, "count");
    lab.metric(key + ".deaths", p.deaths, "count");
    lab.metric(key + ".pickups", static_cast<double>(s.pickups), "count");
    lab.metric(key + ".taken-seen", static_cast<double>(s.taken), "count");
    total += p.frags;
  }
  lab.metric("fight.frags-per-minute", total / (secs / 60), "per-min");
  lab.metric("fight.faults", static_cast<double>(r.faults), "count");
  lab.table(t);

  // The arena from above.
  sh::MoveWorld world;
  const std::vector<sh::BrushModel> models = shooters_edge::server::arena_models(true);
  sh::map_world(models[0], world);
  sl::TopView v{r.lo, r.hi};
  v.hi.z = 248;  // traced from under the roof (z 256), which would cover the floor
  gl::Picture p = sl::floor_picture(world, v);
  for (size_t k = 0; k < r.players.size(); k++) {
    const std::vector<sh::Vec3f>& path = r.players[k].path;
    gl::Rgba c = player_colour(k);
    c.a = 150;
    for (size_t i = 0; i + 1 < path.size(); i++) {
      const sh::Vec3f a = path[i], b = path[i + 1];
      if ((a - b).x * (a - b).x + (a - b).y * (a - b).y > 64 * 64) continue;  // a respawn
      gl::draw_line(p, v.px(a.x), v.py(a.y), v.px(b.x), v.py(b.y), c, 1);
    }
  }
  for (const fb::FightItem& it : r.items) {
    gl::fill_rect(p, v.px(it.at.x) - 5, v.py(it.at.y) - 5, 10, 10, {250, 250, 240, 255});
    gl::draw_ring(p, v.px(it.at.x), v.py(it.at.y), 7, kInk);
    std::string name = it.name;
    for (const char* pre : {"weapon_", "item_"})
      if (name.rfind(pre, 0) == 0) name = name.substr(std::string(pre).size());
    gl::draw_text(p, v.px(it.at.x) + 9, v.py(it.at.y) - 4, name, kInk);
  }
  for (const fb::FightDeath& d : r.deaths) {
    const gl::Rgba c = player_colour(static_cast<size_t>(d.victim));
    gl::draw_line(p, v.px(d.at.x) - 6, v.py(d.at.y) - 6, v.px(d.at.x) + 6, v.py(d.at.y) + 6, c, 3);
    gl::draw_line(p, v.px(d.at.x) - 6, v.py(d.at.y) + 6, v.px(d.at.x) + 6, v.py(d.at.y) - 6, c, 3);
  }
  int x = sl::kTopMargin;
  for (size_t k = 0; k < r.players.size(); k++) {
    const std::string label = r.players[k].name + " " + sh::aim_skill_name(r.players[k].skill);
    gl::fill_rect(p, x, 8, 14, 6, player_colour(k));
    gl::draw_text(p, x + 18, 7, label, kInk);
    x += 18 + gl::text_width(label) + 16;
  }
  gl::draw_text(p, x, 7, "X A DEATH", kInk);
  lab.shot({"arena", "THE DEATHMATCH FROM ABOVE",
            "To scale from above, the arena on the baseline mod for " + std::to_string(r.frames) +
                " server frames (" + to_fixed(secs, 0) +
                " s): every fighter's trail in its colour, the items as the fighters know them, "
                "and a cross where each one died (" +
                std::to_string(r.deaths.size()) + " deaths).",
            "trails through the door, up to the ledge on the plat and across to the items; "
            "deaths spread over both rooms, not piled where a bot sticks",
            in},
           p);

  // Each fighter's mode through the match.
  const int strip_w = 900, row_h = 22, pad = 90;
  gl::Picture m = gl::create_picture(
      strip_w + pad + 16, row_h * static_cast<int>(r.players.size()) + 48, {246, 246, 242, 255});
  for (size_t k = 0; k < r.players.size(); k++) {
    const std::vector<uint8_t>& md = r.players[k].mode;
    const int y = 30 + static_cast<int>(k) * row_h;
    gl::draw_text(m, 8, y + 6, r.players[k].name + " " + sh::aim_skill_name(r.players[k].skill),
                  kInk);
    for (int px = 0; px < strip_w; px++) {
      const size_t i = md.size() * static_cast<size_t>(px) / static_cast<size_t>(strip_w);
      if (i >= md.size()) break;
      const uint8_t mode = md[i];
      gl::fill_rect(m, pad + px, y, 1, row_h - 4, kMode[mode == 255 ? 4 : std::min<int>(mode, 3)]);
    }
  }
  int lx = pad;
  static const char* kModeName[5] = {"ROAM", "FIGHT", "CHASE", "RETREAT", "DEAD"};
  for (int i = 0; i < 5; i++) {
    gl::fill_rect(m, lx, 10, 14, 8, kMode[i]);
    gl::draw_text(m, lx + 18, 9, kModeName[i], kInk);
    lx += 18 + gl::text_width(kModeName[i]) + 16;
  }
  lab.shot({"modes", "EACH FIGHTER'S MODE THROUGH THE MATCH",
            "A strip a fighter, left to right over the " + to_fixed(secs, 0) +
                " s: what it chose to do at each moment (roam for the item worth most, fight a "
                "contact in sight, chase one lost a moment ago, retreat for health or armour when "
                "weak) and the moments it lay dead.",
            "fights in short bursts broken by chases and retreats; a death after most retreats "
            "that came too late; no fighter stuck in one mode",
            in},
           m);

  // The frags against time.
  gl::LineChart ch;
  ch.spikes = false;
  ch.frame = {"FRAGS THROUGH THE MATCH", "seed " + lab.arg("seed"), "t (s)", "frags"};
  for (size_t k = 0; k < r.players.size(); k++) {
    gl::Trace tr{r.players[k].name + " " + sh::aim_skill_name(r.players[k].skill), {}, {}};
    const std::vector<float>& f = r.players[k].frags_at;
    for (size_t i = 0; i < f.size(); i += 13) {
      tr.x.push_back(static_cast<double>(i) * fb::kPilotMsec * 0.001);
      tr.y.push_back(f[i]);
    }
    ch.traces.push_back(tr);
  }
  lab.shot({"frags", ch.frame.title,
            "Each fighter's frags (kills less suicides) against the match's time.",
            "every fighter scoring; the expert's line above the novice's over a long match", in},
           gl::to_picture(gl::line_chart(ch).plot));
}

const gl::LabSpec kSpec{
    "se-bots",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "games/shooters_edge/bots/fighter.hpp (the mode, the goal, the weapon, the trigger) through "
    "fight_run.hpp on the baseline mod over the server's arena",
    "bots",
    "Do the fighters play the baseline mod's deathmatch like players: do they score, die, pick "
    "up items, switch weapons and move between roaming, fighting, chasing and retreating, and "
    "does a better aim score more?",
    {
        {"frames", "9000", "the match's server frames (13 ms each)"},
        {"seed", "1", "the mod's and the fighters' seed"},
        {"root", ".", "the repository's root (the mod's sources are under it)"},
        {"skills", "",
         "the fighters' aims, comma separated (empty: expert, average, average, novice)"},
        {"tempers", "",
         "the fighters' temperaments, comma separated: aggressive, balanced or coward (a, b, c); "
         "missing: balanced"},
    },
    "--frames=2000",
    run,
};
GE_LAB(kSpec);

}  // namespace
