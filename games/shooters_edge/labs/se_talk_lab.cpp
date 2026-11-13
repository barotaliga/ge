// LAB `se-talk`: who hears a player near him? The classic server (net/classic_server.hpp) plays
// the baseline mod on a map with one client on each deathmatch spot; the first is the talker. He
// says a line with `say_near` and talks with his voice aimed at the players near
// (`voicetarg near`: net/classic_talk.hpp), and each other client counts the lines he read and the
// voice messages he was sent. The rule: within `--earshot` map units AND inside the hearing set of
// the talker's place (no wall sound cannot pass). By default on the fixture halls (plugins/brush,
// a map written in code, its one spot joined by places at set distances from it); `--basedir`
// with `--map` plays a map of a game folder's (the pictures
// use the classic deathmatch maps of the registered pack, dm3 by default).
//
// Pictured: the map from above to scale, floor shaded by height, the earshot as a ring round the
// talker, each listener a disc coloured by what reached him (green the line and the voice, red
// within earshot but behind a wall, grey too far) with his distance. The table gives each
// listener's distance, whether his place is in the talker's hearing set, the nearby lines read and
// the voice messages heard; a listener outside the rule that heard anything fails the lab.
//
// ge:summary Lab: nearby chat and voice on a map from above: earshot ring, who heard, who not
// ge:keywords lab se-talk say_near voicetarg voice-near nearby proximity earshot hearing-set
// ge:keywords top-view talk chat voice
// ge:skill net shooter labs
// ge:test tests/shooters_edge_classic_talk_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "nav_top.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "shooter/hull.hpp"
#include "shooter/nav.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_server.hpp"
#include "shooters_edge/net/classic_talk.hpp"
#include "shooters_edge/server/bsp_map.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;
namespace sl = shooters_edge::labs;
using ge::core::num::to_fixed;

constexpr int kMostListeners = 15;
constexpr const char* kLine = "rope down on the left";

constexpr int64_t kTick = 13000;  // a server frame, us

bool always(int, int) { return true; }

struct Listener {
  std::string name;
  sh::Vec3f at;
  double distance = 0;
  bool in_set = true;  // the place is in the talker's hearing set
  bool read = false;   // the nearby line reached him
  int64_t heard = 0;   // voice messages from the talker
};

struct Talk {
  std::string error;
  sh::Vec3f talker;
  std::vector<Listener> listeners;
  std::unique_ptr<cl::LoadedMap> map;  // the world points into it
  sh::MoveWorld world;
  sh::Vec3f lo, hi;
};

// The deathmatch spots of the map's entity text, in its order.
std::vector<sh::Vec3f> spots_of(const ge::brush::Map& m) {
  std::vector<sh::Vec3f> out;
  for (const char* kind : {"info_player_deathmatch", "info_player_start"}) {
    for (const ge::brush::Entity& e : m.entities) {
      if (e.get("classname") != kind) continue;
      sh::Vec3f p;
      if (std::sscanf(e.get("origin").c_str(), "%f %f %f", &p.x, &p.y, &p.z) == 3) out.push_back(p);
    }
    if (!out.empty()) break;
  }
  return out;
}

Talk run_talk(const std::vector<uint8_t>& bytes, const std::string& name, int earshot) {
  Talk r;
  r.map = cl::map_from_bytes(bytes, name, r.error);
  if (!r.map) return r;
  cl::LoadedMap* map = r.map.get();
  ge::progs::Progs progs;
  if (!cl::baseline_progs(progs, r.error)) return r;
  sv::QcServerOptions so;
  so.loop.max_edicts = 1024;
  so.loop.maxclients = kMostListeners + 1;
  std::unique_ptr<sv::QcServer> qc = sv::make_qc_server(progs, so, r.error);
  if (!qc || !sv::load_map(*qc, map->spec, r.error)) return r;
  sv::RulesRunner rules = sv::qc_runner_over(*qc);
  sv::use_map_sets(*rules, map->bsp);
  std::vector<sh::Vec3f> spots = spots_of(map->bsp.map);
  if (spots.empty()) {
    r.error = "the map has no player spot";
    return r;
  }
  // A map with few spots (the fixture halls' one): places at set distances from the first.
  if (spots.size() < 4)
    for (const sh::Vec3f d : {sh::Vec3f{300, 0, 0}, sh::Vec3f{0, 700, 0}, sh::Vec3f{-1300, 0, 0},
                              sh::Vec3f{0, -2500, 0}})
      spots.push_back({spots[0].x + d.x, spots[0].y + d.y, spots[0].z + d.z});
  const int n = static_cast<int>(std::min<size_t>(spots.size(), kMostListeners + 1));
  gn::LinkConditions lan;
  gn::conditions_preset("lan", lan);
  lan.jitter_ms = 0;
  gn::SimNetwork net = gn::make_sim_network(1, lan);
  gn::Transport st;
  gn::open_sim_transport(st, net, gn::sim_address(0, cn::kPortServer));
  cn::ServerConfig sc;
  sc.voice = true;
  sc.earshot = earshot;
  cn::ClassicServer server;
  if (!cn::classic_start(server, rules, st, sc, r.error)) return r;
  std::vector<std::unique_ptr<gn::Transport>> ts(static_cast<size_t>(n));
  std::vector<std::unique_ptr<cn::ClassicClientEnd>> c(static_cast<size_t>(n));
  for (int i = 0; i < n; i++) {
    ts[static_cast<size_t>(i)] = std::make_unique<gn::Transport>();
    c[static_cast<size_t>(i)] = std::make_unique<cn::ClassicClientEnd>();
    gn::open_sim_transport(*ts[static_cast<size_t>(i)], net,
                           gn::sim_address(static_cast<uint32_t>(i + 1), cn::kPortClient));
    cn::ClientConfig cc;
    cc.name = i == 0 ? "talker" : "p" + std::to_string(i);
    cc.voice = true;
    cc.files = {"skins/base.pcx"};
    if (i == 0) {
      cc.talk = always;
      cc.voice_to = "near";
    }
    cc.qport = static_cast<uint16_t>(100 + i);
    cn::client_start(*c[static_cast<size_t>(i)], *ts[static_cast<size_t>(i)],
                     gn::sim_address(0, cn::kPortServer), cc, 0);
  }
  // Each client held on his spot, every frame (the mod would have them anywhere).
  const auto pin = [&] {
    for (int i = 0; i < n; i++) {
      const int slot = c[static_cast<size_t>(i)]->playernum;
      if (slot < 0) continue;
      sh::Edict& e = rules->sv.edicts[static_cast<size_t>(slot) + 1];
      e.origin = spots[static_cast<size_t>(i)];
      e.velocity = {};
      std::vector<sv::WorldEntity>& w = rules->world.entities;
      if (static_cast<size_t>(slot) + 1 < w.size())
        w[static_cast<size_t>(slot) + 1].origin = e.origin;
    }
  };
  int64_t now = 0;
  const auto frame = [&] {
    now += kTick;
    gn::sim_advance(net, now);
    pin();
    cn::classic_frame(server, now);
    for (auto& e : c) cn::client_tick(*e, now);
  };
  for (int f = 0; f < 1500; f++) {
    frame();
    bool all = true;
    for (const auto& e : c) all = all && e->stage == cn::ClientStage::active;
    if (all) break;
  }
  for (int f = 0; f < 80; f++) frame();
  // Counted from here: what came before the talker stood on his spot is not his.
  std::vector<int64_t> before(static_cast<size_t>(n));
  const int me = c[0]->playernum;
  if (me < 0) {
    r.error = "the talker never joined";
    return r;
  }
  for (int i = 0; i < n; i++)
    before[static_cast<size_t>(i)] = c[static_cast<size_t>(i)]->voice_heard[me];
  cn::client_command(*c[0], std::string("say_near \"") + kLine + "\"");
  for (int f = 0; f < 160; f++) frame();
  r.talker = spots[0];
  for (int i = 1; i < n; i++) {
    const cn::ClassicClientEnd& e = *c[static_cast<size_t>(i)];
    Listener l;
    l.name = e.cfg.name;
    l.at = spots[static_cast<size_t>(i)];
    const double dx = l.at.x - r.talker.x, dy = l.at.y - r.talker.y, dz = l.at.z - r.talker.z;
    l.distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    l.in_set = rules->vis == nullptr || rules->vis(rules->vis_ctx, r.talker, l.at, false);
    for (const std::string& p : e.prints)
      if (p.find(kLine) != std::string::npos) l.read = true;
    l.heard = e.voice_heard[me] - before[static_cast<size_t>(i)];
    r.listeners.push_back(l);
  }
  sh::map_world(map->spec.models[0], r.world);
  const ge::brush::Model& w0 = map->bsp.map.models[0];
  r.lo = {w0.mins[0], w0.mins[1], w0.mins[2]};
  r.hi = {w0.maxs[0], w0.maxs[1], w0.maxs[2]};
  return r;
}

// A closed map's floor from above: under each point (every second unit) the first open space
// found coming down from the roof, traced down to its floor and shaded by height; walls and the
// rock round the map dark. (nav_top.hpp's traces from the roof, which a closed map's rock fills.)
gl::Picture floor_of(const sh::MoveWorld& world, const sl::TopView& v) {
  const int w = static_cast<int>(v.hi.x - v.lo.x) + 2 * sl::kTopMargin;
  const int h = static_cast<int>(v.hi.y - v.lo.y) + 2 * sl::kTopMargin;
  gl::Picture p = gl::create_picture(w, h, {245, 245, 240, 255});
  const sh::Hull& hull = world.solids[0].point_hull;
  const float span = std::max(1.0f, v.hi.z - v.lo.z);
  for (int py = sl::kTopMargin; py < h - sl::kTopMargin; py += 2)
    for (int px = sl::kTopMargin; px < w - sl::kTopMargin; px += 2) {
      const float x = v.lo.x + static_cast<float>(px - sl::kTopMargin) + 1.0f;
      const float y = v.hi.y - static_cast<float>(py - sl::kTopMargin) - 1.0f;
      gl::Rgba c{70, 74, 72, 255};
      for (int k = 0;; k++) {
        const float z = v.hi.z - 8 - 24.0f * static_cast<float>(k);
        if (z <= v.lo.z) break;
        if (sh::point_contents(hull, {x, y, z}) == sh::kContentsSolid) continue;
        const sh::HullTrace tr = sh::trace_hull(hull, {x, y, z}, {x, y, v.lo.z});
        const float t = (tr.endpos.z - v.lo.z) / span;  // 0 low .. 1 high
        const auto g = static_cast<uint8_t>(150 + 95 * t);
        c = {static_cast<uint8_t>(g - 20), g, static_cast<uint8_t>(g - 35), 255};
        break;
      }
      gl::fill_rect(p, px, py, 2, 2, c);
    }
  return p;
}

gl::Picture top_view(const Talk& r, int earshot) {
  sl::TopView v{r.lo, r.hi};
  gl::Picture p = floor_of(r.world, v);
  const double tx = v.px(r.talker.x), ty = v.py(r.talker.y);
  for (int k = 0; k < 3; k++) draw_ring(p, tx, ty, earshot + k * 0.5, {40, 110, 220, 255});
  for (const Listener& l : r.listeners) {
    const bool near = l.distance <= earshot;
    const gl::Rgba col = (l.read && l.heard > 0) ? gl::Rgba{40, 170, 70, 255}
                         : near                  ? gl::Rgba{210, 50, 40, 255}
                                                 : gl::Rgba{130, 130, 130, 255};
    const double x = v.px(l.at.x), y = v.py(l.at.y);
    draw_line(p, tx, ty, x, y, {col.r, col.g, col.b, 90});
    fill_disc(p, x, y, 14, col);
    draw_text(p, x + 18, y - 8, l.name + " " + to_fixed(l.distance, 0), {20, 20, 20, 255}, 2);
  }
  fill_disc(p, tx, ty, 16, {40, 110, 220, 255});
  draw_text(p, tx + 20, ty - 8, "talker", {40, 110, 220, 255}, 2);
  draw_text(p, 8, 4,
            "blue ring: earshot " + std::to_string(earshot) +
                "  green: read and heard  red: near, behind a wall  grey: too far",
            {20, 20, 20, 255}, 2);
  return p;
}

void run(gl::Lab& lab) {
  const int earshot = static_cast<int>(lab.arg_num("earshot"));
  const std::string basedir = lab.arg("basedir");
  std::string name = "halls", error;
  std::vector<uint8_t> bytes;
  if (!basedir.empty()) {
    cl::SearchPath files;
    if (!cl::open_game_dir(basedir, lab.arg("game"), files, error)) {
      lab.fail(error, "Pass --basedir=<the folder that holds the game folder>. Skill: net.");
      return;
    }
    name = lab.arg("map");
    std::vector<uint8_t> store;
    const std::span<const uint8_t> f = cl::find_file(files, "maps/" + name + ".bsp", store);
    if (f.empty()) {
      lab.fail("se-talk: no maps/" + name + ".bsp in the game folder",
               "Name a map the game folder holds (--map). Skill: net.");
      return;
    }
    bytes.assign(f.begin(), f.end());
  } else if (!ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bytes,
                                   &error)) {
    lab.fail("se-talk: the fixture halls: " + error, "next: run tests/brush_write_test.cpp");
    return;
  }
  const Talk r = run_talk(bytes, name, earshot);
  if (!r.error.empty()) {
    lab.fail("se-talk: " + r.error,
             "next: run tests/shooters_edge_classic_talk_test.cpp. Skill: net.");
    return;
  }
  gl::LabTable t{
      "each listener against the talker",
      {"listener", "distance", "in hearing set", "read the line", "voice messages", "should hear"},
      {}};
  int reached = 0, wrong = 0;
  for (const Listener& l : r.listeners) {
    const bool should = l.distance <= earshot && l.in_set;
    const bool got = l.read || l.heard > 0;
    if (should && l.read && l.heard > 0) reached++;
    if (should != got) wrong++;
    t.rows.push_back({l.name, to_fixed(l.distance, 0), l.in_set ? "yes" : "no",
                      l.read ? "yes" : "no", std::to_string(l.heard), should ? "yes" : "no"});
  }
  lab.table(t);
  lab.metric("talk.listeners", static_cast<double>(r.listeners.size()), "count");
  lab.metric("talk.reached", reached, "count");
  lab.metric("talk.wrong", wrong, "count");
  if (wrong > 0)
    lab.fail("se-talk: " + std::to_string(wrong) +
                 " listeners got what the earshot and the hearing set say they should not (or "
                 "missed what they should get)",
             "Read talk_near in games/shooters_edge/net/classic_talk.cpp and run "
             "tests/shooters_edge_classic_talk_test.cpp. Skill: net.");
  lab.shot({"from-above",
            "WHO HEARS THE TALKER: " + name,
            "The map from above to scale. The talker (blue) stands on the first player spot and "
            "says \"" +
                std::string(kLine) +
                "\" with say_near while talking with his voice aimed at the players near "
                "(voicetarg near). Each other player stands on another spot.",
            "green discs inside the ring only, every disc outside it grey, a red disc inside the "
            "ring only where a wall stands between",
            {{"map", name}, {"earshot", std::to_string(earshot)}}},
           top_view(r, earshot));
}

const gl::LabSpec kSpec{
    "se-talk",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "games/shooters_edge/net/classic_talk.hpp (say_near, voicetarg near: earshot and the hearing "
    "set) on the classic server",
    "net",
    "Does a nearby line and a nearby voice reach exactly the players within earshot with no wall "
    "between, and nobody else?",
    {
        {"earshot", "1000", "sv_earshot: how near a player must stand, map units"},
        {"basedir", "", "the folder that holds the game folder (\"\": the fixture halls)"},
        {"game", "", "a mod folder over the game folder"},
        {"map", "dm3", "the map played, with --basedir"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
