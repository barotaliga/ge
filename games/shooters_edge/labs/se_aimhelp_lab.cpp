// LAB `se-aimhelp`: THE AIM HELP, GRANTED TO ONE PLAYER BY A VOTE (server/shot_help.hpp,
// plugins/shooter/aim_help.hpp, engine/server/aids.hpp). Two parts:
//
// - THE ZONE: on the test arena, ann looks at bob 500 units away and her crosshair is swept over a
//   grid of offsets round him (`--reach` each way, `--step` apart); at each, one hitscan line is
//   traced as a mod's `traceline` would, bent by the aim help when she is granted it. The scatter
//   shows where her crosshair hits him without the help (his box) and where it hits only with it
//   (the box grown by `sv_aimhelp_x` sideways and `sv_aimhelp_y` up and down), and the metrics
//   count both.
// - THE SESSION: Shooter's Edge's dedicated server on the simulated network (`--map`, a real one
//   with `--root`), three players in; anna calls `vote aimhelp bert on`, cara says yes, the vote
//   passes and bert is granted it, disclosed as `*aimhelp` in his info; dora joins later and is
//   told; anna types `users`, which lists every player's aids, allowed and used. Her console is
//   pictured over `--backdrop` (a se-client shot of the map).
//
// ge:summary Lab: the aim help's hit zone with and without it, and a vote granting it to one player
// ge:keywords lab se-aimhelp aim-help aimhelp vote users player-aids join-line leeway hitscan
// ge:keywords sv_aimhelp_x sv_aimhelp_y accessibility touch-controls
// ge:skill shooter net labs
// ge:test tests/shooters_edge_shot_help_test.cpp tests/shooters_edge_classic_admin_test.cpp
#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "arena_chart.hpp"
#include "console/console.hpp"
#include "console/view.hpp"
#include "core/libm/libm.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/png.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "server/server.hpp"
#include "shooter/server.hpp"
#include "shooter/trace.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/net/classic_info.hpp"
#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "shooters_edge/server/shot_help.hpp"
#include "shooters_edge/server/test_mod.hpp"
#include "ui/console.hpp"
#include "ui/raster.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace gs = ge::server;
namespace se = shooters_edge::net;
namespace sv = shooters_edge::server;
namespace sh = ge::shooter;
namespace pg = ge::progs;
namespace con = ge::console;
namespace la = shooters_edge::labs;

// ---- The zone ----

struct Zone {
  gl::Trace hit{"hits without the help", {}, {}};
  gl::Trace helped{"hits only with the help", {}, {}};
  gl::Trace missed{"misses", {}, {}};
  int plain = 0, with = 0, cells = 0;
  float width = 0, height = 0;  // bob's box
};

// One line from ann's eye through `at` (bob's plane), helped or not: whether it hits bob.
bool hits(sv::QcServer& s, sh::Vec3f at, bool helped) {
  sv::set_user_info(s, 0, sv::kAimHelpKey, helped ? "1" : "");
  sh::Edict& ann = s.sv.edicts[1];
  const sh::Vec3f eye = ann.origin + ann.view_ofs;
  sh::Vec3f dir = at - eye;
  sh::normalize(dir);
  const double flat = std::sqrt(double(dir.x) * dir.x + double(dir.y) * dir.y);
  const double kDeg = 57.29577951308232;
  ann.v_angle = {float(-ge::core::libm::atan2(double(dir.z), flat) * kDeg),
                 float(ge::core::libm::atan2(double(dir.y), double(dir.x)) * kDeg), 0};
  sh::Vec3f end = eye + sh::angle_vectors(ann.v_angle).forward * 2048;
  sv::help_traced_shot(s, 1, eye, end);
  const sh::HullTrace tr = sh::trace_move(s.sv, eye, {}, {}, end, sh::kTraceNormal, 1);
  return tr.ent == 2;
}

Zone sweep(gl::Lab& lab) {
  Zone z;
  pg::Progs progs;
  std::string error;
  sv::QcServerOptions o;
  o.loop.max_edicts = 1024;
  std::unique_ptr<sv::QcServer> s;
  if (sv::build_test_mod(progs, error)) s = sv::make_qc_server(progs, o, error);
  sv::MapSpec map;
  map.models = sv::arena_models();
  map.entities = sv::kArenaEntities;
  if (!s || !sv::load_map(*s, map, error)) {
    lab.fail("the test arena did not start: " + error,
             "run tests/shooters_edge_shot_help_test.cpp");
    return z;
  }
  sv::connect(*s, 0, "ann", false, true);
  sv::connect(*s, 1, "bob", false, true);
  for (int i = 0; i < 10; i++) sv::qc_frame(*s, nullptr, 0);
  // Both in the west room, the door's wall behind bob.
  sh::set_origin(s->sv, 1, {-420, 0, 24});
  sh::set_origin(s->sv, 2, {80, 0, 24});
  s->sv.edicts[2].flags |= sh::kFlagOnGround;
  s->sv.set.aimhelp_x = static_cast<float>(lab.arg_num("x"));
  s->sv.set.aimhelp_y = static_cast<float>(lab.arg_num("y"));
  const sh::Edict& bob = s->sv.edicts[2];
  const sh::Vec3f mid = (bob.absmin + bob.absmax) * 0.5f;
  z.width = bob.absmax.y - bob.absmin.y;
  z.height = bob.absmax.z - bob.absmin.z;
  const auto reach = static_cast<float>(lab.arg_num("reach"));
  const auto step = static_cast<float>(lab.arg_num("step"));
  const int n = static_cast<int>(reach / step);
  for (int iy = -n; iy <= n; iy++)
    for (int iz = -n; iz <= n; iz++) {
      const float dy = static_cast<float>(iy) * step, dz = static_cast<float>(iz) * step;
      const sh::Vec3f at = mid + sh::Vec3f{0, -dy, dz};  // dy + to ann's right (−y)
      const bool plain = hits(*s, at, false);
      const bool with = hits(*s, at, true);
      z.cells++;
      z.plain += plain ? 1 : 0;
      z.with += with ? 1 : 0;
      gl::Trace& t = plain ? z.hit : with ? z.helped : z.missed;
      t.x.push_back(dy);
      t.y.push_back(dz);
    }
  return z;
}

// ---- The session ----

constexpr uint16_t kPort = 27500, kClientPort = 27001;
constexpr const char* kNames[] = {"anna", "bert", "cara", "dora"};
constexpr size_t kClients = 4;

struct Session {
  std::string error;
  int active = 0;
  bool passed = false, granted = false, told_all = false, dora_told = false, listed = false;
  std::string key_seen;              // bert's `*aimhelp` as cara sees it
  std::vector<std::string> console;  // anna's console: what she typed and what she was told
  std::vector<std::string> dora;     // what dora was told on joining
};

std::vector<std::string> lines_of(const std::vector<std::string>& texts) {
  std::vector<std::string> out;
  for (const std::string& t : texts)
    for (size_t at = 0; at < t.size();) {
      const size_t end = std::min(t.find('\n', at), t.size());
      if (end > at) out.push_back(t.substr(at, end - at));
      at = end + 1;
    }
  return out;
}

bool any_has(const std::vector<std::string>& lines, const std::string& what) {
  for (const std::string& l : lines)
    if (l.find(what) != std::string::npos) return true;
  return false;
}

Session run_session(gl::Lab& lab) {
  Session r;
  gn::LinkConditions link;
  gn::conditions_preset("lan", link);
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  gn::SimNetwork net = gn::make_sim_network(seed, link);
  gn::Transport port;
  gn::open_sim_transport(port, net, gn::sim_address(0, kPort));
  gs::ServerConfig c;
  c.hostname = "Friday Night";
  c.map = lab.arg("map");
  c.root = lab.arg("root").empty() ? "." : lab.arg("root");
  c.maxclients = 8;
  c.seed = seed;
  gs::Server s;
  if (!gs::server_open(s, se::classic_game(), c, &port, nullptr, r.error)) return r;
  std::unique_ptr<gn::Transport> ct[kClients];
  std::unique_ptr<se::ClassicClientEnd> ends[kClients];
  auto join = [&](size_t i) {
    ct[i] = std::make_unique<gn::Transport>();
    gn::open_sim_transport(*ct[i], net, gn::sim_address(static_cast<uint32_t>(i + 1), kClientPort));
    ends[i] = std::make_unique<se::ClassicClientEnd>();
    se::ClientConfig cc;
    cc.name = kNames[i];
    cc.files = {"skins/base.pcx"};
    cc.seed = static_cast<int>(i) + 1;
    cc.qport = static_cast<uint16_t>(1000 + i);
    se::client_start(*ends[i], *ct[i], gn::sim_address(0, kPort), cc,
                     static_cast<double>(s.now_us) / 1e6);
  };
  auto step = [&](int frames) {
    for (int f = 0; f < frames; f++) {
      gn::sim_advance(net, s.now_us);
      for (auto& e : ends)
        if (e) se::client_tick(*e, s.now_us);
      gs::server_frame(s);
    }
  };
  const int second = static_cast<int>(1.0 / s.frame_s);
  for (size_t i = 0; i < 3; i++) join(i);
  step(4 * second);
  se::ClassicClientEnd& anna = *ends[0];
  auto say = [&](se::ClassicClientEnd& who, const std::string& line) {
    if (&who == &anna) r.console.push_back("]" + line);
    se::client_command(who, line);
    step(second / 2);
  };
  for (auto& e : ends)
    if (e) e->prints.clear();
  say(anna, "vote aimhelp bert on");
  say(*ends[2], "vote yes");
  step(second);
  for (const std::string& l : lines_of(anna.prints)) r.console.push_back(l);
  anna.prints.clear();
  r.passed = any_has(r.console, "Vote passed: aimhelp bert on");
  r.granted = any_has(r.console, "bert is granted the aim help");
  r.told_all = r.granted && any_has(lines_of(ends[1]->prints), "bert is granted the aim help") &&
               any_has(lines_of(ends[2]->prints), "bert is granted the aim help");
  const int bert = ends[1]->playernum;
  if (bert >= 0) r.key_seen = se::info_get(ends[2]->userinfos[bert], sv::kAimHelpKey);
  join(3);
  step(4 * second);
  r.dora = lines_of(ends[3]->prints);
  r.dora_told = any_has(r.dora, "Player aids on this server: bert has aim help");
  say(anna, "users");
  const std::vector<std::string> users = lines_of(anna.prints);
  for (const std::string& l : users) r.console.push_back(l);
  r.listed = any_has(users, "aim help (granted");
  for (auto& e : ends) r.active += e && e->stage == se::ClientStage::active ? 1 : 0;
  return r;
}

void draw(gl::Picture& p, const con::ConsoleView& v) {
  ge::ui::Image img;
  img.width = p.width, img.height = p.height;
  img.rgba = std::move(p.rgba);
  ge::ui::DrawList dl;
  dl.begin(img.width, img.height);
  ge::ui::draw_console(dl, v);
  ge::ui::render(dl, img);
  p.rgba = std::move(img.rgba);
}

// The game behind the console: `--backdrop`'s picture (its top 16:9), scaled; else a dark wall.
gl::Picture backdrop(gl::Lab& lab, int w, int h) {
  gl::Picture p = gl::create_picture(w, h, {24, 20, 16});
  if (lab.arg("backdrop").empty()) return p;
  const std::optional<gl::Picture> from = gl::read_png(lab.arg("backdrop"));
  if (!from || from->width <= 0) {
    lab.fail("--backdrop " + lab.arg("backdrop") + " did not read as a PNG",
             "give a picture a lab wrote (previews/se-client@<tag>/enhanced-0.png); skill: labs.");
    return p;
  }
  const int fh = std::min(from->height, from->width * 9 / 16);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      const size_t src = (static_cast<size_t>(y * fh / h) * static_cast<size_t>(from->width) +
                          static_cast<size_t>(x * from->width / w)) *
                         4;
      const size_t dst =
          (static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 4;
      for (int k = 0; k < 4; k++) p.rgba[dst + k] = from->rgba[src + k];
    }
  return p;
}

void run(gl::Lab& lab) {
  const std::vector<std::pair<std::string, std::string>> in = {
      {"sv_aimhelp_x", lab.arg("x")}, {"sv_aimhelp_y", lab.arg("y")}, {"map", lab.arg("map")}};
  const Zone z = sweep(lab);
  lab.metric("zone.cells", z.cells, "aims");
  lab.metric("zone.hits_without", z.plain, "aims");
  lab.metric("zone.hits_with", z.with, "aims");
  if (z.with <= z.plain)
    lab.fail("the aim help made no aim hit that missed",
             "run tests/shooters_edge_shot_help_test.cpp; skill: shooter.");
  gl::LineChart c;
  c.frame = {"WHERE THE CROSSHAIR HITS, 500 UNITS OUT",
             "sv_aimhelp_x " + lab.arg("x") + ", sv_aimhelp_y " + lab.arg("y") +
                 "; one line a dot, the target's box " + std::to_string(int(z.width)) + " x " +
                 std::to_string(int(z.height)),
             "the crosshair right of his middle (units at him)",
             "the crosshair above his middle (units at him)"};
  c.frame.height = 720;
  c.frame.width = 760;
  c.points = true;
  c.spikes = false;
  c.traces = {z.missed, z.hit, z.helped};
  la::shoot(lab, "zone", gl::line_chart(c),
            "The crosshair swept over a grid round a player 500 units away, one hitscan line a "
            "dot. Without the aim help only the dots on his box hit; granted the help, a line that "
            "passes within the leeway beside him or over or under him is bent onto him, so the "
            "zone that hits grows by sv_aimhelp_x each side and sv_aimhelp_y above and below.",
            "the helped dots forming a band round the box: wider above and below than at the "
            "sides; nothing helped beyond the leeway",
            in);

  const Session r = run_session(lab);
  if (!r.error.empty()) {
    lab.fail("the dedicated server did not open: " + r.error,
             "check --map names a map (arena is built in; a real one needs --root=<install>); "
             "skill: net.");
    return;
  }
  lab.metric("session.players", r.active, "clients");
  lab.metric("session.vote_passed", r.passed ? 1 : 0, "");
  lab.metric("session.everyone_told", r.told_all ? 1 : 0, "");
  lab.metric("session.key_seen_by_others", r.key_seen == "1" ? 1 : 0, "");
  lab.metric("session.newcomer_told", r.dora_told ? 1 : 0, "");
  lab.metric("session.users_lists_it", r.listed ? 1 : 0, "");
  if (!r.passed || !r.told_all || r.key_seen != "1" || !r.dora_told || !r.listed)
    lab.fail("the vote did not grant bert the aim help as the words say",
             "run tests/shooters_edge_classic_game_test.cpp and "
             "tests/shooters_edge_classic_admin_test.cpp; skill: net.");
  gl::LabTable t{"anna's console, then what dora was told on joining", {"who", "line"}, {}};
  for (const std::string& l : r.console) t.rows.push_back({"anna", l});
  for (const std::string& l : r.dora) t.rows.push_back({"dora", l});
  lab.table(std::move(t));

  con::Console console;
  con::ConsoleView v(console);
  console.execute("con_height 0.75");
  for (const std::string& l : r.console)
    v.add(l.starts_with("]") ? con::LineKind::command : con::LineKind::output, l);
  for (const std::string& l : r.dora) v.add(con::LineKind::server, "(dora) " + l);
  v.set_open(true);
  for (int f = 0; f < 600 && v.drop() < 1; f++) v.frame(1.0 / 60);
  gl::Picture shot = backdrop(lab, static_cast<int>(lab.arg_num("width")),
                              static_cast<int>(lab.arg_num("height")));
  draw(shot, v);
  lab.shot({"vote", "THE AIM HELP BY A VOTE",
            "anna calls a vote to give bert the aim help, cara says yes and it passes; everyone is "
            "told and bert's info carries *aimhelp. dora joins later and is told who has which "
            "aid. anna's users lists each player's aids: how each is allowed and whether it is "
            "used.",
            "Vote passed; bert is granted the aim help with its leeway; dora's line names bert; "
            "the users row for bert reads aim help (granted, ...)",
            in},
           shot);
}

const gl::LabSpec kSpec{
    "se-aimhelp",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/server/shot_help (the aim help) and its grant by a vote",
    "shooter",
    "Does the aim help bend a granted player's near misses onto the target within the leeway, "
    "and do the players grant it to one of them by a vote, disclosed to all and told to who joins?",
    {
        {"x", "24", "sv_aimhelp_x: the leeway sideways, units"},
        {"y", "48", "sv_aimhelp_y: the leeway up and down, units"},
        {"reach", "96", "the crosshair's sweep each way round the target's middle, units"},
        {"step", "4", "the sweep's spacing, units"},
        {"map", "arena", "the session's map (arena is built in; a real one needs --root)"},
        {"root", "", "the folder holding the game folder (id1) with its packs"},
        {"seed", "1", "the network's and the game's stream"},
        {"backdrop", "", "a PNG of the game behind the console (a se-client shot); empty: none"},
        {"width", "1280", "the console picture's width, px"},
        {"height", "720", "the console picture's height, px"},
    },
    "--step=8 --width=640 --height=360",
    run,
};
GE_LAB(kSpec);

}  // namespace
