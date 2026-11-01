// LAB `se-team`: does a team game of bots play like a team game? Two teams of four fighters
// (classic_team.hpp: a client each over our server, the team's plan bots/team.hpp) play the
// baseline mod with `teamplay 1` for `--minutes`, recorded as a multiview demo. By default on the
// fixture halls (plugins/brush, a map written in code); `--basedir` with `--map` plays a map of a
// game folder's instead (the classic small team map is `e1m2` of the shareware pack).
//
// Pictured: the map from above (the nav graph's floor shaded by height) with every player's trail
// in his team's colour, the items and the two key areas; each player's role through the match as a
// strip (hold, rush, carry, free, dead); and each team's frags against time. The table gives each
// player's frags, deaths, shots, shots held for a teammate, backpacks, his share of time in each
// role and the weapon he held most; the teams' rows their totals, rushes and quads. The recording
// is attached (`match.mvd`); `--out` writes it to a path as well.
//
// ge:summary Lab: a recorded 4on4 of bots on teamplay: roles, key areas, frags by team, the MVD
// ge:keywords lab se-team team-game 4on4 teamplay roles hold rush carry quad key-area mvd record
// ge:skill shooter bots labs
// ge:test tests/shooters_edge_team_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "classicenhance/profile.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "shooter/nav.hpp"
#include "shooters_edge/bots/knowledge.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_team.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace cn = shooters_edge::net;
namespace bt = shooters_edge::bots;
namespace cl = shooters_edge::client;
using ge::core::num::to_fixed;
using In = std::vector<std::pair<std::string, std::string>>;

const gl::Rgba kTeam[2] = {{200, 50, 40, 255}, {40, 90, 210, 255}};
const gl::Rgba kRole[5] = {{150, 160, 170, 255},  // free
                           {40, 150, 70, 255},    // hold
                           {240, 160, 20, 255},   // rush
                           {150, 60, 200, 255},   // carry
                           {40, 40, 40, 255}};    // dead
const gl::Rgba kInk{30, 30, 30, 255};

// The map scaled to fit `size` pixels across its longer side.
struct View {
  sh::Vec3f lo, hi;
  double scale = 1;
  int margin = 28;
  double px(double x) const { return margin + (x - lo.x) * scale; }
  double py(double y) const { return margin + (hi.y - y) * scale; }
};

void run(gl::Lab& lab) {
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  const double minutes = lab.arg_num("minutes");
  const std::string basedir = lab.arg("basedir"), map_name = lab.arg("map");
  if (minutes <= 0) {
    lab.fail("se-team: --minutes must be over 0", "next: make lab LAB=se-team ARGS=--help");
    return;
  }
  std::string error;
  std::vector<uint8_t> bytes;
  std::string name = "halls";
  std::string locs_text, locs_from;
  if (!lab.arg("locs").empty()) {
    std::FILE* lf = std::fopen(lab.arg("locs").c_str(), "rb");
    if (!lf) {
      lab.fail("se-team: cannot read " + lab.arg("locs"),
               "next: pass --locs=<a .loc file that exists>, or leave it out to name places off "
               "the items");
      return;
    }
    char buf[4096];
    size_t got = 0;
    while ((got = std::fread(buf, 1, sizeof buf, lf)) > 0) locs_text.append(buf, got);
    std::fclose(lf);
    locs_from = lab.arg("locs");
  }
  if (!basedir.empty()) {
    cl::SearchPath files;
    if (!cl::open_game_dir(basedir, lab.arg("game"), files, error)) {
      lab.fail(error, "Pass --basedir=<the folder that holds the game folder>. Skill: shooter.");
      return;
    }
    name = map_name.empty() ? "e1m2" : map_name;
    std::vector<uint8_t> store;
    const std::span<const uint8_t> f = cl::find_file(files, "maps/" + name + ".bsp", store);
    if (f.empty()) {
      lab.fail("se-team: no maps/" + name + ".bsp in the game folder",
               "Name a map the game folder holds (--map). Skill: shooter.");
      return;
    }
    bytes.assign(f.begin(), f.end());
    // The map's place names, when the game folder holds them (locs/<map>.loc).
    std::vector<uint8_t> loc_store;
    const std::span<const uint8_t> lf = cl::find_file(files, "locs/" + name + ".loc", loc_store);
    if (!lf.empty()) {
      locs_text.assign(lf.begin(), lf.end());
      locs_from = "locs/" + name + ".loc";
    }
  } else if (!ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bytes,
                                   &error)) {
    lab.fail("se-team: the fixture halls: " + error, "next: run tests/brush_write_test.cpp");
    return;
  }
  ge::progs::Progs progs;
  if (!cl::baseline_progs(progs, error)) {
    lab.fail("se-team: the baseline mod: " + error,
             "next: run tests/shooters_edge_baseline_mod_test.cpp");
    return;
  }
  const In in = {{"map", name}, {"minutes", lab.arg("minutes")}, {"seed", lab.arg("seed")}};
  cn::TeamMatchOptions o;
  o.map = bytes;
  o.map_name = name;
  if (basedir.empty()) o.entities = cn::kTeamHallsEntities;
  o.locs = locs_text;
  o.locs_from = locs_from;
  o.progs = &progs;
  o.seed = seed;
  o.frames = static_cast<int>(std::lround(minutes * 60 * 1000 / shooters_edge::bots::kPilotMsec));
  const cn::TeamMatchReport r = cn::run_team_match(o);
  if (!r.ok) {
    lab.fail("se-team: " + r.error, "next: run tests/shooters_edge_team_test.cpp");
    return;
  }
  const double secs = r.seconds;

  // The table.
  static const char* kWeapon[9] = {
      "", "blade", "scattergun", "double", "spiker", "heavy spiker", "grenades", "rockets", "arc"};
  gl::LabTable t{"each player over the match",
                 {"player", "team", "aim", "frags", "deaths", "shots", "held for a mate", "packs",
                  "hold", "rush", "carry", "free", "held most"},
                 {}};
  for (const cn::TeamPlayer& p : r.players) {
    int64_t alive = 0;
    for (int k = 0; k < 4; k++) alive += p.role_commands[k];
    const auto share = [&](int k) {
      return to_fixed(
          alive > 0 ? static_cast<double>(p.role_commands[k]) / static_cast<double>(alive) : 0, 2);
    };
    int most = 1;
    for (int w = 1; w <= 8; w++)
      if (p.weapon_commands[w] > p.weapon_commands[most]) most = w;
    t.rows.push_back({p.name, o.teams[p.team].name, sh::aim_skill_name(p.skill),
                      to_fixed(p.frags, 0), std::to_string(p.deaths), std::to_string(p.shots),
                      std::to_string(p.held_fire), std::to_string(p.packs), share(1), share(2),
                      share(3), share(0), kWeapon[most]});
    lab.metric("team." + p.name + ".frags", p.frags, "count");
    lab.metric("team." + p.name + ".deaths", p.deaths, "count");
  }
  lab.table(t);
  gl::LabTable pt{
      "each player's legs",
      {"player", "travelled", "plans", "stuck", "lost", "waits", "barred", "dives", "given up",
       "dive s", "off graph", "walk", "wait", "arrived", "idle", "air", "hop"},
      {}};
  for (const cn::TeamPlayer& p : r.players) {
    int64_t live = 0;
    for (int k = 0; k < sh::kPilotModes; k++) live += p.pilot_modes[k];
    const auto share = [&](int k) {
      return to_fixed(
          live > 0 ? static_cast<double>(p.pilot_modes[k]) / static_cast<double>(live) : 0, 2);
    };
    pt.rows.push_back(
        {p.name, to_fixed(p.travelled, 0), std::to_string(p.plans), std::to_string(p.stuck),
         std::to_string(p.lost), std::to_string(p.waits), std::to_string(p.barred),
         std::to_string(p.dives), std::to_string(p.dives_given_up), to_fixed(p.dive_wet_s, 0),
         std::to_string(p.off_graph), share(1), share(5), share(8), share(0), share(4), share(9)});
  }
  lab.table(pt);
  gl::LabTable tt{
      "each team",
      {"team", "frags", "deaths", "quads taken", "rushes", "shots held", "callouts", "messages"},
      {}};
  for (int k = 0; k < 2; k++) {
    const cn::TeamTotals& tm = r.teams[k];
    tt.rows.push_back({o.teams[k].name, to_fixed(tm.frags, 0), std::to_string(tm.deaths),
                       std::to_string(tm.quad_pickups), std::to_string(tm.rushes),
                       std::to_string(tm.held_shots), std::to_string(tm.callouts),
                       std::to_string(tm.messages)});
    const std::string key = "team." + o.teams[k].name;
    lab.metric(key + ".frags", tm.frags, "count");
    lab.metric(key + ".quads", static_cast<double>(tm.quad_pickups), "count");
    lab.metric(key + ".messages", static_cast<double>(tm.messages), "count");
  }
  lab.table(tt);
  // What each player knew of the map's items, at the start, at the half and at the end.
  gl::LabTable kt{"each player's map: items known (recalled from anywhere)",
                  {"player", "knows the map as", "start", "half", "end", "of", "messages"},
                  {}};
  for (const cn::TeamPlayer& p : r.players) {
    const auto at = [&](size_t k) {
      return p.known.empty() ? 0 : p.known[std::min(k, p.known.size() - 1)];
    };
    kt.rows.push_back({p.name, bt::map_sense_name(bt::map_sense_of(p.skill)), std::to_string(at(0)),
                       std::to_string(at(p.known.size() / 2)), std::to_string(at(p.known.size())),
                       std::to_string(r.item_at.size()), std::to_string(p.said)});
  }
  lab.table(kt);
  gl::LabTable bt_{"the map's buttons: where, how, and what each opens",
                   {"button at (x y z)", "how", "doors", "opens", "used from (x y z)"},
                   {}};
  const auto xyz = [](sh::Vec3f a) {
    return to_fixed(a.x, 0) + " " + to_fixed(a.y, 0) + " " + to_fixed(a.z, 0);
  };
  for (const cn::TeamButton& b : r.buttons) {
    std::string opens;
    for (const std::string& n : b.opens) opens += (opens.empty() ? "" : ", ") + n;
    bt_.rows.push_back({xyz(b.at), b.shoot ? "shoot" : "touch", std::to_string(b.doors.size()),
                        opens.empty() ? "(a way through)" : opens, xyz(b.from)});
  }
  lab.table(bt_);
  lab.metric("team.buttons", static_cast<double>(r.buttons.size()), "count");
  gl::LabTable et{
      "each player's ears and area play",
      {"player", "ears", "sounds", "taken in", "items heard taken", "spawns heard", "spam rockets"},
      {}};
  int64_t heard_taken = 0, spawns = 0, sounds = 0, heard = 0;
  for (const cn::TeamPlayer& p : r.players) {
    et.rows.push_back({p.name, bt::map_sense_name(bt::map_sense_of(p.skill)),
                       std::to_string(p.sounds), std::to_string(p.heard),
                       std::to_string(p.heard_taken), std::to_string(p.heard_spawns),
                       std::to_string(p.spam_shots)});
    heard_taken += p.heard_taken;
    spawns += p.heard_spawns;
    sounds += p.sounds;
    heard += p.heard;
  }
  lab.table(et);
  lab.metric("team.sounds-taken-in", static_cast<double>(heard), "count");
  lab.metric("team.sounds-missed", static_cast<double>(sounds - heard), "count");
  lab.metric("team.items-heard-taken", static_cast<double>(heard_taken), "count");
  lab.metric("team.spawns-heard", static_cast<double>(spawns), "count");
  lab.metric("team.areas-lost-seen",
             static_cast<double>(r.teams[0].lost_seen + r.teams[1].lost_seen), "count");
  lab.metric("team.retakes", static_cast<double>(r.teams[0].retakes + r.teams[1].retakes), "count");
  lab.metric("team.spam-rockets",
             static_cast<double>(r.teams[0].spam_shots + r.teams[1].spam_shots), "count");
  // The team messages: the first lines in the report, all of them as a file.
  {
    std::string all;
    for (const std::string& l : r.chat) all += l + "\n";
    lab.attach("chat.txt", std::vector<uint8_t>(all.begin(), all.end()), gl::Det::deterministic);
    for (size_t k = 0; k < r.chat.size() && k < 24; k++) lab.note(r.chat[k]);
  }
  lab.metric("team.frags-per-minute", (r.teams[0].frags + r.teams[1].frags) / (secs / 60),
             "per-min");
  lab.metric("team.faults", static_cast<double>(r.faults), "count");
  lab.metric("team.place-names", static_cast<double>(r.locs), "count");
  lab.note("place names: " + std::to_string(r.locs) + " from " + r.locs_from);
  lab.metric("team.lines-read", static_cast<double>(r.lines_read), "count");
  lab.metric("team.lines-missed", static_cast<double>(r.lines_missed), "count");
  lab.metric("team.nav-nodes", static_cast<double>(r.nav_nodes), "count");
  lab.metric("team.teleport-links", static_cast<double>(r.teleport_links), "count");
  lab.metric("team.movers", static_cast<double>(r.movers), "count");
  lab.metric("team.closed-links", static_cast<double>(r.closed_links), "count");
  lab.metric("team.swim-wet-cells", static_cast<double>(r.swim_wet), "count");
  lab.metric("team.swim-leading-cells", static_cast<double>(r.swim_leading), "count");
  lab.metric("team.dive-pockets", static_cast<double>(r.dives), "count");
  {
    int64_t begun = 0, given_up = 0;
    double wet_s = 0;
    for (const cn::TeamPlayer& p : r.players) {
      begun += p.dives;
      given_up += p.dives_given_up;
      wet_s += p.dive_wet_s;
    }
    lab.metric("team.dives", static_cast<double>(begun), "count");
    lab.metric("team.dives-given-up", static_cast<double>(given_up), "count");
    lab.metric("team.dive-wet-s", wet_s, "s");
  }
  lab.metric("team.nav-build-ms", r.nav_build_ms, "ms", gl::Det::measured);
  lab.metric("team.mvd-bytes", static_cast<double>(r.mvd.size()), "bytes");
  for (int key = 0; key < static_cast<int>(r.item_at.size()); key++) {
    if (key != r.hold_item && key != r.quad_item && lab.arg("items") != "all") continue;
    const sh::Vec3f a = r.item_at[static_cast<size_t>(key)],
                    b = r.item_node_at[static_cast<size_t>(key)];
    float nearest = 1e9f;
    for (const cn::TeamPlayer& pl : r.players)
      for (const sh::Vec3f& q : pl.path) {
        const sh::Vec3f d = q - a;
        nearest = std::min(nearest, std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z));
      }
    lab.note(r.item_name[static_cast<size_t>(key)] + " at " + to_fixed(a.x, 0) + " " +
             to_fixed(a.y, 0) + " " + to_fixed(a.z, 0) + ", its node at " + to_fixed(b.x, 0) + " " +
             to_fixed(b.y, 0) + " " + to_fixed(b.z, 0) + ", the nearest a player came " +
             to_fixed(nearest, 0));
  }
  std::string unrouted;
  for (size_t i = 0; i < r.item_routed.size(); i++)
    if (!r.item_routed[i]) unrouted += (unrouted.empty() ? "" : ", ") + r.item_name[i];
  lab.note("items no route leads to from the map's main part: " +
           (unrouted.empty() ? "none" : unrouted));
  for (const cn::TeamPlayer& pl : r.players) lab.note(pl.name + ": " + pl.last);
  char hex[24];
  std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(r.digest));
  lab.note(std::string("digest ") + hex);

  // The map from above.
  View v{r.lo, r.hi};
  const double span = std::max(r.hi.x - r.lo.x, r.hi.y - r.lo.y);
  v.scale = span > 0 ? 1400.0 / span : 1;
  const int w = static_cast<int>((r.hi.x - r.lo.x) * v.scale) + 2 * v.margin;
  const int h = static_cast<int>((r.hi.y - r.lo.y) * v.scale) + 2 * v.margin;
  gl::Picture p = gl::create_picture(w, h, {236, 236, 230, 255});
  // The floor: every nav node, shaded by height; red where the first spawn cannot reach it.
  {
    const float zspan = std::max(1.f, r.hi.z - r.lo.z);
    for (size_t i = 0; i < r.nav_points.size(); i++) {
      const sh::Vec3f a = r.nav_points[i];
      const auto g = static_cast<uint8_t>(205 - 80 * std::clamp((a.z - r.lo.z) / zspan, 0.f, 1.f));
      const bool reached = i < r.nav_reached.size() && r.nav_reached[i];
      const gl::Rgba c = reached ? gl::Rgba{g, g, static_cast<uint8_t>(g - 10), 255}
                                 : gl::Rgba{240, 170, 160, 255};
      const int sz = std::max(2, static_cast<int>(32 * v.scale) - 1);
      gl::fill_rect(p, static_cast<int>(v.px(a.x - 16)), static_cast<int>(v.py(a.y + 16)), sz, sz,
                    c);
    }
  }
  for (const cn::TeamPlayer& pl : r.players) {
    gl::Rgba c = kTeam[pl.team];
    c.a = 110;
    for (size_t i = 0; i + 1 < pl.path.size(); i++) {
      const sh::Vec3f a = pl.path[i], b = pl.path[i + 1];
      if (pl.roles[i] == 255 || pl.roles[i + 1] == 255) continue;
      const sh::Vec3f d = a - b;
      if (d.x * d.x + d.y * d.y > 160 * 160) continue;  // a respawn or a teleport
      gl::draw_line(p, v.px(a.x), v.py(a.y), v.px(b.x), v.py(b.y), c, 2);
    }
  }
  for (size_t i = 0; i < r.item_at.size(); i++) {
    const sh::Vec3f a = r.item_at[i];
    const bool key = static_cast<int>(i) == r.hold_item || static_cast<int>(i) == r.quad_item;
    const int s = key ? 7 : 3;
    gl::fill_rect(p, static_cast<int>(v.px(a.x)) - s, static_cast<int>(v.py(a.y)) - s, 2 * s, 2 * s,
                  key ? gl::Rgba{250, 210, 40, 255} : gl::Rgba{250, 250, 240, 255});
    gl::draw_ring(p, v.px(a.x), v.py(a.y), s + 2, kInk);
    std::string n = r.item_name[i];
    for (const char* pre : {"weapon_", "item_"})
      if (n.rfind(pre, 0) == 0) n = n.substr(std::string(pre).size());
    if (key || n.find("armor") != std::string::npos || n.find("launcher") != std::string::npos ||
        n.find("super") != std::string::npos || n == "mega health")
      gl::draw_text(p, static_cast<int>(v.px(a.x)) + s + 4, static_cast<int>(v.py(a.y)) - 4, n,
                    kInk);
  }
  // The buttons: a square (red for a shot, blue for a touch), a line to each door it opens (a
  // ring) and on to every item behind it, and a dot where a player uses it from.
  for (const cn::TeamButton& b : r.buttons) {
    const gl::Rgba c = b.shoot ? gl::Rgba{220, 40, 40, 255} : gl::Rgba{40, 90, 220, 255};
    for (const sh::Vec3f& d : b.doors) {
      gl::draw_line(p, v.px(b.at.x), v.py(b.at.y), v.px(d.x), v.py(d.y), c, 2);
      gl::draw_ring(p, v.px(d.x), v.py(d.y), 9, c);
      for (size_t i = 0; i < r.item_at.size(); i++)
        for (const std::string& n : b.opens)
          if (n == r.item_name[i]) {
            const sh::Vec3f a = r.item_at[i];
            const sh::Vec3f e = a - d;
            if (e.x * e.x + e.y * e.y < 400 * 400)
              gl::draw_line(p, v.px(d.x), v.py(d.y), v.px(a.x), v.py(a.y), c, 1);
          }
    }
    gl::fill_rect(p, static_cast<int>(v.px(b.at.x)) - 6, static_cast<int>(v.py(b.at.y)) - 6, 12, 12,
                  c);
    gl::fill_rect(p, static_cast<int>(v.px(b.from.x)) - 2, static_cast<int>(v.py(b.from.y)) - 2, 4,
                  4, c);
  }
  int x = v.margin;
  for (int k = 0; k < 2; k++) {
    const std::string label = o.teams[k].name + " team";
    gl::fill_rect(p, x, 8, 14, 6, kTeam[k]);
    gl::draw_text(p, x + 18, 7, label, kInk);
    x += 18 + gl::text_width(label) + 16;
  }
  gl::fill_rect(p, x, 6, 10, 10, {250, 210, 40, 255});
  gl::draw_text(p, x + 14, 7, "THE KEY AREAS (THE HELD ARMOUR, THE QUAD)", kInk);
  lab.shot({"map", "THE TEAM GAME FROM ABOVE",
            "From above, " + name + " on the baseline mod with teamplay for " + to_fixed(secs, 0) +
                " s: every player's trail in his team's colour, the items as the fighters know "
                "them, the two key areas in yellow.",
            "both teams' trails thick round the held armour and the quad; the rest of the map "
            "crossed on the way to weapons, not ignored",
            in},
           p);

  // Each player's role through the match.
  const int strip_w = 900, row_h = 20, pad = 120;
  gl::Picture m = gl::create_picture(
      strip_w + pad + 16, row_h * static_cast<int>(r.players.size()) + 52, {246, 246, 242, 255});
  for (size_t k = 0; k < r.players.size(); k++) {
    const std::vector<uint8_t>& rl = r.players[k].roles;
    const int y = 32 + static_cast<int>(k) * row_h;
    gl::fill_rect(m, 6, y + 2, 8, 12, kTeam[r.players[k].team]);
    gl::draw_text(m, 18, y + 5, r.players[k].name + " " + sh::aim_skill_name(r.players[k].skill),
                  kInk);
    for (int px = 0; px < strip_w; px++) {
      const size_t i = rl.size() * static_cast<size_t>(px) / static_cast<size_t>(strip_w);
      if (i >= rl.size()) break;
      gl::fill_rect(m, pad + px, y, 1, row_h - 4,
                    kRole[rl[i] == 255 ? 4 : std::min<int>(rl[i], 3)]);
    }
  }
  int lx = pad;
  static const char* kRoleName[5] = {"FREE", "HOLD", "RUSH", "CARRY THE QUAD", "DEAD"};
  for (int i = 0; i < 5; i++) {
    gl::fill_rect(m, lx, 10, 14, 8, kRole[i]);
    gl::draw_text(m, lx + 18, 9, kRoleName[i], kInk);
    lx += 18 + gl::text_width(kRoleName[i]) + 16;
  }
  lab.shot({"roles", "EACH PLAYER'S ROLE THROUGH THE MATCH",
            "A strip a player, left to right over the " + to_fixed(secs, 0) +
                " s: holding the armour's area, rushing the quad as it comes due, carrying it to "
                "the enemy, gathering what he lacks, or dead.",
            "one holder a team at a time; a rush of two before each quad; the carry after a rush "
            "won; no team without a holder for long",
            in},
           m);

  // The teams' frags through the match.
  gl::LineChart ch;
  ch.spikes = false;
  ch.frame = {"TEAM FRAGS THROUGH THE MATCH", name + ", seed " + lab.arg("seed"), "t (s)", "frags"};
  for (int k = 1; k >= 0; k--) {  // blue first: the chart's palette opens blue, then red
    gl::Trace tr{o.teams[k].name, {}, {}};
    for (size_t i = 0; i < r.team_frags[k].size(); i++) {
      tr.x.push_back(static_cast<double>(i * static_cast<size_t>(o.path_every)) *
                     shooters_edge::bots::kPilotMsec * 0.001);
      tr.y.push_back(r.team_frags[k][i]);
    }
    ch.traces.push_back(tr);
  }
  lab.shot(
      {"frags", ch.frame.title, "Each team's frags (kills less suicides) against the match's time.",
       "both lines climbing; steps where a quad was carried", in},
      gl::to_picture(gl::line_chart(ch).plot));

  if (!r.mvd.empty()) {
    lab.attach("match.mvd", r.mvd, gl::Det::deterministic);
    const std::string out = lab.arg("save");
    if (!out.empty()) {
      std::ofstream f(out, std::ios::binary);
      f.write(reinterpret_cast<const char*>(r.mvd.data()),
              static_cast<std::streamsize>(r.mvd.size()));
      lab.note(f ? "wrote " + out : "could not write " + out);
    }
  }
}

const gl::LabSpec kSpec{
    "se-team",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "games/shooters_edge/bots/team.hpp (the roles, the key areas, the quad's timer) through "
    "net/classic_team.hpp: fighters on clients of our server, teamplay on the baseline mod",
    "bots",
    "Does a team game of bots play like a team game: does each team hold its key area, take the "
    "quad on its timer, leave items to the teammate who needs them and hold its fire for its "
    "own, and does the recording play back?",
    {
        {"minutes", "10", "the match's length"},
        {"seed", "1", "the mod's and the fighters' seed"},
        {"basedir", "", "a folder holding a game folder: play one of its maps"},
        {"game", ge::classicenhance::kProfiles[0].base_dir.data(),
         "the game folder under --basedir"},
        {"map", "", "with --basedir: the map (e1m2 when empty)"},
        {"save", "", "also write the recording to this path"},
        {"locs", "",
         "the map's place names (a .loc file); else the game folder's locs/<map>.loc, else the "
         "items'"},
        {"items", "key",
         "the items noted with the nearest a player came: key (the held armour, the quad) or all"},
    },
    "--minutes=0.5",
    run,
};
GE_LAB(kSpec);

}  // namespace
