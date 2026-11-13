// The sequel's bots lab: the one bot (bots/fighter.hpp) playing a deathmatch under the sequel's
// rules on a version 38 hall built here (no game files), reading the sequel's arsenal
// (bots/bot_arsenal.hpp). Two halves joined by a doorway, a ledge a plat rises to with the railgun
// on it, a teleporter pad, the shotgun, the chaingun and the rocket launcher with their ammo, body
// armour and health. Four bots for a while: their frags and deaths, what they took, which weapon
// was in hand how long, the steps it fired in, and every bot's path drawn over the hall's plan.
// Every number is deterministic.
//
// ge:summary Lab: the bots in a sequel deathmatch on a version 38 hall: frags, pickups, weapons
// ge:keywords lab sequel bots deathmatch arsenal se-sequel-bots fighter version-38 frags pickups
// ge:skill bots shooter labs
// ge:test tests/shooters_edge_sequel_nav_test.cpp
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "classicenhance/profile.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooter/features.hpp"
#include "shooter/sequel_move.hpp"
#include "shooter/traits.hpp"
#include "shooters_edge/bots/bot_arsenal.hpp"
#include "shooters_edge/bots/fighter.hpp"
#include "shooters_edge/bots/map_nav.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/server/sequel_server.hpp"

namespace {

namespace br = ge::brush;
namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace bt = shooters_edge::bots;
namespace cl = shooters_edge::client;
namespace pf = shooters_edge::profiles;

constexpr int kBots = 4;
constexpr double kBotHz = 1000.0 / bt::kPilotMsec;
const char* const kNames[kBots] = {"Ana", "Ben", "Cy", "Dee"};

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
}

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

br::Entity item(const char* classname, const char* origin) {
  return entity({{"classname", classname}, {"origin", origin}});
}

// West and east halves joined by a doorway at x = 0; a ledge 96 high at the east end with a plat
// (model 1) beside it and the railgun on it; a pad in the west half sending players east.
std::shared_ptr<const br::Map> hall(std::string& error) {
  br::BrushWorld w;
  w.models.resize(2);
  auto& m = w.models[0];
  m.push_back(box(-512, -256, -16, 512, 256, 0));    // floor
  m.push_back(box(-512, -256, 256, 512, 256, 272));  // ceiling
  m.push_back(box(-528, -256, 0, -512, 256, 256));   // walls round
  m.push_back(box(512, -256, 0, 528, 256, 256));
  m.push_back(box(-512, -272, 0, 512, -256, 256));
  m.push_back(box(-512, 256, 0, 512, 272, 256));
  m.push_back(box(-8, -256, 0, 8, -48, 256));  // the dividing wall, a doorway in it
  m.push_back(box(-8, 48, 0, 8, 256, 256));
  m.push_back(box(384, -256, 0, 512, 256, 96));          // the ledge
  w.models[1].push_back(box(304, -32, 0, 368, 32, 96));  // the plat, drawn raised
  w.entities.push_back(entity({{"classname", "info_player_deathmatch"}, {"origin", "-300 0 24"}}));
  w.entities.push_back(entity({{"classname", "info_player_deathmatch"}, {"origin", "200 0 24"}}));
  w.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "-300 150 24"}}));
  w.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "200 -150 24"}}));
  w.entities.push_back(entity({{"classname", "func_plat"}, {"model", "*1"}}));
  w.entities.push_back(
      entity({{"classname", "misc_teleporter"}, {"origin", "-400 -180 16"}, {"target", "t1"}}));
  w.entities.push_back(entity(
      {{"classname", "misc_teleporter_dest"}, {"origin", "200 180 16"}, {"targetname", "t1"}}));
  w.entities.push_back(item("weapon_shotgun", "-200 -150 16"));
  w.entities.push_back(item("weapon_chaingun", "-420 160 16"));
  w.entities.push_back(item("ammo_bullets", "-380 160 16"));
  w.entities.push_back(item("weapon_rocketlauncher", "150 0 16"));
  w.entities.push_back(item("ammo_rockets", "100 200 16"));
  w.entities.push_back(item("weapon_railgun", "448 0 112"));
  w.entities.push_back(item("ammo_slugs", "448 120 112"));
  w.entities.push_back(item("item_armor_body", "-100 0 16"));
  w.entities.push_back(item("item_health", "250 -200 16"));
  w.entities.push_back(item("item_health", "-300 -200 16"));
  std::vector<uint8_t> bytes;
  auto map = std::make_shared<br::Map>();
  if (!br::write_brush_world(w, &bytes, &error) || !br::read_bsp(bytes, map.get(), &error))
    return nullptr;
  return map;
}

// The map's entities as the text a fighter learns its items from.
std::string entity_text(const br::Map& map) {
  std::string out;
  for (const br::Entity& e : map.entities) {
    out += "{\n";
    for (const auto& [k, v] : e.pairs) {
      out += '"';
      out += k;
      out += "\" \"";
      out += v;
      out += "\"\n";
    }
    out += "}\n";
  }
  return out;
}

std::string fixed(double v, int places) { return ge::core::num::to_fixed(v, places); }

struct Seat {
  bt::Fighter fighter;
  sh::MoveCmd cmds[3];
  uint32_t sequence = 0;
  double held_s[10] = {};  // by weapon index: time in hand while alive
  int deaths = 0;
  bool was_alive = false;
  gl::Trace path;
};

// The map asked for (`--game` and `--map`: the player's own packs), or the hall.
std::shared_ptr<const br::Map> lab_map(gl::Lab& lab, std::string& name, std::string& error) {
  if (lab.arg("game").empty()) {
    name = "hall";
    return hall(error);
  }
  const pf::GameProfile& game = *pf::find_game("sequel");
  const ge::classicenhance::Profile& content = pf::content_of(game);
  cl::SearchPath files;
  if (!cl::open_game_dir(lab.arg("game"), std::string(content.base_dir), files, error, content))
    return nullptr;
  name = lab.arg("map");
  std::unique_ptr<cl::LoadedMap> m = cl::load_named_map(files, name, error);
  auto map = std::make_shared<br::Map>();
  if (!m || !br::read_bsp(m->file, map.get(), &error)) return nullptr;
  return map;
}

void run(gl::Lab& lab) {
  std::string error, name;
  const std::shared_ptr<const br::Map> map = lab_map(lab, name, error);
  if (map == nullptr) {
    lab.fail("the map did not load: " + error,
             "Read the reason above; --game is the folder holding the sequel's base folder, --map "
             "a map in it. Skill: brush.");
    return;
  }
  const bool hall_map = lab.arg("game").empty();
  // The bots take a turned view as their own (a spot's angle, a teleport's), as the window's
  // player does: so its turn is spent, not added to every view they send after.
  sv::SequelServerOptions opt;
  opt.clients_take_view = true;
  sv::RulesRunner r = sv::sequel_runner(sv::make_sequel_server(opt));
  r.console("deathmatch 1");
  sv::MapSpec spec;
  spec.name = name;
  spec.brushes = map;
  if (!r.start(spec, error)) {
    lab.fail("the sequel's rules did not start " + name + ": " + error,
             "Read the reason above. Skill: shooter.");
    return;
  }
  auto nav = std::make_unique<bt::MapNav>();
  nav->move = bt::sequel_nav_move(sv::sequel_air_accelerate(*sv::sequel_of(r)));
  if (!bt::build_map_nav(*r, spec, "info_player_deathmatch", *nav, error)) {
    lab.fail("the bots' nav did not build on " + name + ": " + error,
             "Read bots/map_nav.cpp's build_map_nav. Skill: bots.");
    return;
  }
  const std::string text = entity_text(*map);
  const bt::BotArsenal& arsenal = bt::sequel_arsenal();
  std::vector<std::unique_ptr<Seat>> seats;
  for (int i = 0; i < kBots; i++) {
    r.connect(i, kNames[i], false, true);
    auto seat = std::make_unique<Seat>();
    bt::Fighter& f = seat->fighter;
    const sh::AimSkill skill = i % 2 == 0 ? sh::AimSkill::expert : sh::AimSkill::average;
    f.arsenal = &arsenal;
    f.features = sh::features_of(skill);
    bt::fighter_init(f, nav->graph, *r, i, text.c_str(), skill,
                     sh::traits_of(skill, sh::Temper::balanced), kBotHz,
                     static_cast<uint32_t>(1000 + i));
    f.bot.yaw = r->sv.edicts[static_cast<size_t>(i) + 1].v_angle.y;
    f.bot.pilot.world = &nav->world;
    seat->path.label = kNames[i];
    seats.push_back(std::move(seat));
  }
  const double seconds = lab.arg_num("seconds");
  const int frames = static_cast<int>(seconds * kBotHz);
  const double dt = bt::kPilotMsec / 1000.0;
  sv::SequelServer& game = *sv::sequel_of(r);
  for (int frame = 0; frame < frames; frame++) {
    std::vector<sh::Packet> packets;
    for (int i = 0; i < kBots; i++) {
      Seat& s = *seats[static_cast<size_t>(i)];
      s.cmds[0] = s.cmds[1];
      s.cmds[1] = s.cmds[2];
      const bool turned = game.players[i].forced;  // a view the rules turned it to, unspent
      const sh::Vec3f view = r->sv.edicts[static_cast<size_t>(i) + 1].v_angle;
      if (turned) s.fighter.bot.yaw = view.y;
      s.cmds[2] = bt::fighter_command(s.fighter, *r, i, bt::kPilotMsec);
      if (turned) s.cmds[2].angles = view;
      sh::Packet p;
      p.client = i;
      p.sequence = ++s.sequence;
      for (int k = 0; k < 3; k++) p.cmds[k] = s.cmds[k];
      packets.push_back(p);
    }
    r.frame(packets.data(), static_cast<int>(packets.size()));
    for (int i = 0; i < kBots; i++) {
      Seat& s = *seats[static_cast<size_t>(i)];
      const sv::SequelPlayer& p = game.players[i];
      if (s.was_alive && !p.alive) s.deaths++;
      s.was_alive = p.alive;
      if (!p.alive) continue;
      const int w = game.players[i].hand.weapon;
      if (w >= 0 && w < 10) s.held_s[w] += dt;
      if (frame % 8 == 0) {
        s.path.x.push_back(p.move.origin[0] / 8.0);
        s.path.y.push_back(p.move.origin[1] / 8.0);
      }
    }
  }
  gl::LabTable table{
      "four bots, " + fixed(seconds, 0) + " s of deathmatch on " + name,
      {"bot", "frags", "deaths", "pickups", "firing steps", "switches", "most in hand"},
      {}};
  float frags = 0;
  int64_t pickups = 0, shots = 0;
  double off_blaster = 0, alive = 0;
  for (int i = 0; i < kBots; i++) {
    const Seat& s = *seats[static_cast<size_t>(i)];
    const sv::SequelPlayer& p = game.players[i];
    int most = 0;
    for (int w = 0; w < 10; w++) {
      if (s.held_s[w] > s.held_s[most]) most = w;
      alive += s.held_s[w];
      if (w != 0) off_blaster += s.held_s[w];
    }
    frags += p.frags;
    pickups += s.fighter.stats.pickups;
    shots += s.fighter.stats.shots;
    table.rows.push_back({kNames[i], fixed(p.frags, 0), std::to_string(s.deaths),
                          std::to_string(s.fighter.stats.pickups),
                          std::to_string(s.fighter.stats.shots),
                          std::to_string(s.fighter.stats.switches), arsenal.weapons[most].name});
    const std::string key = std::string("bot.") + kNames[i];
    lab.metric(key + ".frags", p.frags, "frags");
    lab.metric(key + ".deaths", s.deaths, "deaths");
    lab.metric(key + ".pickups", static_cast<double>(s.fighter.stats.pickups), "items");
    lab.metric(key + ".firing", static_cast<double>(s.fighter.stats.shots), "steps");
  }
  lab.table(table);
  gl::LabTable weapons{"time each weapon was in hand, all bots (s)", {"weapon", "s", "share"}, {}};
  for (int w = 0; w < 10; w++) {
    double t = 0;
    for (const auto& s : seats) t += s->held_s[w];
    weapons.rows.push_back({arsenal.weapons[static_cast<size_t>(w)].name, fixed(t, 1),
                            fixed(alive > 0 ? 100 * t / alive : 0, 0) + "%"});
    lab.metric(std::string("held.") + arsenal.weapons[static_cast<size_t>(w)].name, t, "s");
  }
  lab.table(weapons);
  lab.metric("frags", frags, "frags");
  lab.metric("pickups", static_cast<double>(pickups), "items");
  lab.metric("firing", static_cast<double>(shots), "steps");
  lab.metric("off-blaster", alive > 0 ? off_blaster / alive : 0, "share");
  lab.metric("nav.nodes", static_cast<double>(nav->graph.nodes.size()), "nodes");
  // The jumps the nav proved with the sequel's move (its air steers far less than the first
  // game's, so fewer jumps across a gap reach).
  for (const sh::NavKind k : {sh::NavKind::walk, sh::NavKind::drop, sh::NavKind::jump})
    lab.metric(std::string("nav.links.") + sh::nav_kind_name(k),
               static_cast<double>(nav->graph.counts.by_kind[static_cast<int>(k)]), "links");
  if (pickups == 0)
    lab.fail("no bot took an item on " + name,
             "Read the fighter's goals (bots/goals.cpp) and item_of with the sequel's arsenal "
             "(bots/knowledge.cpp). Skill: bots.");
  if (shots == 0)
    lab.fail("no bot fired a shot",
             "Read choose_weapon and the trigger in bots/fighter.cpp, and read_stats with the "
             "sequel's numbers (bots/bot_arsenal.cpp). Skill: bots.");
  if (off_blaster == 0)
    lab.fail("no bot held anything but the blaster",
             "Read choose_weapon in bots/fighter.cpp and the weapon numbers the sequel's rules "
             "take (server/sequel_combat.cpp). Skill: bots.");

  // The paths over the plan: the walls as lines, each bot's trail its own colour.
  gl::LineChart plan;
  plan.points = true;
  plan.spikes = false;
  std::string upper = name;
  for (char& c : upper) c = static_cast<char>(c >= 'a' && c <= 'z' ? c - 32 : c);
  plan.frame = {"FOUR BOTS ON " + upper + ", FROM ABOVE",
                "the sequel's rules on a version 38 map; the bots read its arsenal", "x (units)",
                "y (units)"};
  for (const auto& s : seats) plan.traces.push_back(s->path);
  lab.shot(
      {"paths",
       plan.frame.title,
       hall_map
           ? "Every bot's place every eighth step of the match, seen from above: the west "
             "half (x below 0), the doorway at y between -48 and 48, the east half, and "
             "the ledge past x 384 the plat rises to."
           : "Every bot's place every eighth step of the match on " + name + ", seen from above.",
       hall_map ? "trails in both halves and through the doorway, and some on the ledge past "
                  "x 384 (the railgun's, reached by the plat)"
                : "trails over the whole map's floors, not a few rooms; no trail stuck "
                  "pressed along one wall",
       {{"frags", fixed(frags, 0)}, {"pickups", std::to_string(pickups)}}},
      gl::to_picture(gl::line_chart(plan).plot));
}

const gl::LabSpec kSpec{
    "se-sequel-bots",
    gl::LabKind::drive,
    "game:shooters_edge",
    "the bots in a deathmatch under the sequel's rules, reading its arsenal (bots/bot_arsenal.hpp)",
    "bots",
    "Do the bots walk, ride, teleport, pick up, switch to and fire the sequel's weapons, and score "
    "frags on a version 38 map under its rules?",
    {{"seconds", "120", "seconds of deathmatch"},
     {"game", "", "the folder holding the sequel's base folder (none: the hall built here)"},
     {"map", "q2dm1", "the map played with --game"}},
    "--seconds=30",
    run,
};
GE_LAB(kSpec);

}  // namespace
