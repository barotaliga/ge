// LAB `se-admin`: WHO IS ON, AND THE EASY JUMP FOR ONE PLAYER (net/classic_admin.hpp). Shooter's
// Edge's dedicated server on the engine's core, on the simulated network, with `rcon_password`
// set and `--bots` bots seated; three classic clients join (bert asking for the easy jump with
// `cl_speedjump 1`), on the arena or a real map (`--root=<install> --map=dm4`). The first client
// is the administrator: from his window he sends a wrong password, then `rcon <password> status`,
// `rcon <password> speedjump <bert's id> on` and `status` again. It answers whether every player
// is listed with his user id, whether the grant reaches bert and every other client as the
// disclosed `*speedjump` key, whether everyone is told, and whether a wrong password is refused.
// The administrator's console is pictured as the window draws it (engine/console's view through
// the 2D layer), down over the game, holding the session.
//
// ge:summary Lab: users and status by user id, the easy jump granted to one player over rcon
// ge:keywords lab se-admin users status speedjump grant rcon rcon_password easy-jump
// ge:keywords accessibility console user-id dedicated-server
// ge:skill net shooter labs
// ge:test tests/shooters_edge_classic_admin_test.cpp
#include <algorithm>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "console/console.hpp"
#include "console/view.hpp"
#include "lab/lab.hpp"
#include "lab/png.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "server/server.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/net/classic_info.hpp"
#include "ui/console.hpp"
#include "ui/raster.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace gs = ge::server;
namespace se = shooters_edge::net;
namespace con = ge::console;

constexpr uint16_t kPort = 27500, kClientPort = 27001;
constexpr const char* kNames[] = {"anna", "bert", "cara"};
constexpr size_t kClients = 3;

// One administrator's console line sent and its answer, as his window shows them.
struct Said {
  std::string typed;
  std::vector<std::string> answer;
};

struct Session {
  std::string error;
  std::vector<Said> said;
  std::vector<std::string> told[kClients];  // the server's prints each client got
  std::string key_seen[kClients];           // bert's `*speedjump` as each client sees it
  int bert_id = -1;
  int active = 0;
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
  c.bots = static_cast<int32_t>(lab.arg_num("bots"));
  c.seed = seed;
  c.rcon_password = lab.arg("password");
  gs::Server s;
  if (!gs::server_open(s, se::classic_game(), c, &port, nullptr, r.error)) return r;
  std::unique_ptr<gn::Transport> ct[kClients];
  std::unique_ptr<se::ClassicClientEnd> ends[kClients];
  for (size_t i = 0; i < kClients; i++) {
    ct[i] = std::make_unique<gn::Transport>();
    gn::open_sim_transport(*ct[i], net, gn::sim_address(static_cast<uint32_t>(i + 1), kClientPort));
    ends[i] = std::make_unique<se::ClassicClientEnd>();
    se::ClientConfig cc;
    cc.name = kNames[i];
    cc.files = {"skins/base.pcx"};
    cc.seed = static_cast<int>(i) + 1;
    cc.qport = static_cast<uint16_t>(1000 + i);
    cc.speedjump = i == 1;
    se::client_start(*ends[i], *ct[i], gn::sim_address(0, kPort), cc, 0);
  }
  auto step = [&](int frames) {
    for (int f = 0; f < frames; f++) {
      gn::sim_advance(net, s.now_us);
      for (auto& e : ends) se::client_tick(*e, s.now_us);
      gs::server_frame(s);
    }
  };
  const int second = static_cast<int>(1.0 / s.frame_s);
  step(4 * second);
  for (auto& e : ends) r.active += e->stage == se::ClientStage::active ? 1 : 0;
  se::ClassicClientEnd& admin = *ends[0];
  auto rcon = [&](const std::string& password, const std::string& line) {
    admin.oob_prints.clear();
    se::client_rcon(admin, password, line);
    step(second / 2);
    r.said.push_back({"rcon " + password + " " + line, lines_of(admin.oob_prints)});
  };
  rcon("guess", "status");
  rcon(lab.arg("password"), "status");
  r.bert_id = -1;
  for (const std::string& row : r.said.back().answer)  // `slot id  name ...`: bert's id
    if (row.find(" bert ") != std::string::npos) r.bert_id = std::atoi(row.c_str() + 4);
  for (auto& e : ends) e->prints.clear();
  rcon(lab.arg("password"), "speedjump " + std::to_string(r.bert_id) + " on");
  step(second / 2);
  rcon(lab.arg("password"), "status");
  for (size_t i = 0; i < kClients; i++) {
    r.told[i] = lines_of(ends[i]->prints);
    const int bert = ends[1]->playernum;
    if (bert >= 0) r.key_seen[i] = se::info_get(ends[i]->userinfos[bert], "*speedjump");
  }
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

// The game behind the console: `--backdrop`'s picture (its top 16:9, so a lab shot's picture
// without its caption band), scaled to w x h; else a dark wall.
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
  const Session r = run_session(lab);
  if (!r.error.empty()) {
    lab.fail("the dedicated server did not open: " + r.error,
             "check --map names a map (arena is built in; a real one needs --root=<install>); "
             "skill: net.");
    return;
  }
  int listed = 0, granted_rows = 0, refused = 0, told = 0, key_seen = 0;
  for (const Said& s : r.said)
    for (const std::string& line : s.answer) {
      if (line.find("Bad rcon_password.") != std::string::npos) refused++;
      if (line.find("easy jump (granted 0.8") != std::string::npos) granted_rows++;
    }
  if (r.said.size() >= 2)
    for (const std::string& line : r.said[1].answer)
      for (const char* n : kNames)
        if (line.find(std::string(" ") + n + " ") != std::string::npos) listed++;
  for (size_t i = 0; i < kClients; i++) {
    for (const std::string& line : r.told[i])
      if (line.find("bert is granted the easy jump") != std::string::npos) told++;
    if (r.key_seen[i] == "0.8") key_seen++;
  }
  lab.metric("clients_playing", r.active, "clients");
  lab.metric("players_listed", listed, "players");
  lab.metric("bert_id", r.bert_id, "id");
  lab.metric("wrong_password_refused", refused, "answers");
  lab.metric("granted_rows_in_status", granted_rows, "rows");
  lab.metric("clients_told", told, "clients");
  lab.metric("clients_seeing_the_key", key_seen, "clients");
  if (listed != static_cast<int>(kClients) || refused != 1 || granted_rows != 1 ||
      told != static_cast<int>(kClients) || key_seen != static_cast<int>(kClients))
    lab.fail("the session did not go as the administrator's words say",
             "run tests/shooters_edge_classic_admin_test.cpp; skill: net.");

  gl::LabTable table{
      "the administrator's session: each line sent and its answer", {"sent", "answer"}, {}};
  for (const Said& s : r.said)
    for (size_t k = 0; k < s.answer.size() || k == 0; k++)
      table.rows.push_back({k == 0 ? s.typed : "", k < s.answer.size() ? s.answer[k] : ""});
  lab.table(std::move(table));

  // The administrator's console, down over the game, holding the session.
  con::Console console;
  con::ConsoleView v(console);
  console.execute("con_height 0.75");
  for (size_t k = 0; k < r.said.size(); k++) {
    v.add(con::LineKind::command, "]" + r.said[k].typed);
    for (const std::string& line : r.said[k].answer) v.add(con::LineKind::output, line);
    if (k == 2)  // what the server told everyone of the grant, in its own colour
      for (const std::string& line : r.told[0]) v.add(con::LineKind::server, line);
  }
  v.set_open(true);
  for (int f = 0; f < 600 && v.drop() < 1; f++) v.frame(1.0 / 60);
  const int w = static_cast<int>(lab.arg_num("width"));
  const int h = static_cast<int>(lab.arg_num("height"));
  gl::Picture shot = backdrop(lab, w, h);
  draw(shot, v);
  lab.shot({"console",
            "THE ADMINISTRATOR'S CONSOLE",
            "anna runs the server's remote console from her window: a wrong password refused, "
            "`status` listing every player and bot by user id with their ping, address and "
            "aids, `speedjump " +
                std::to_string(r.bert_id) +
                " on` granting bert alone the easy jump (pm_speedjump stays 0 for everyone else), "
                "and `status` again showing it granted. Everyone is told.",
            "bert's aids read easy jump (not allowed, cl_speedjump 1) before the grant and easy "
            "jump (granted 0.8, cl_speedjump 1) after; the bots' address column reads bot",
            {{"map", lab.arg("map")}, {"bots", lab.arg("bots")}}},
           shot);
}

const gl::LabSpec kSpec{
    "se-admin",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/net/classic_admin (users, status, the easy jump per player, rcon)",
    "net",
    "Does the server list every player by user id, and grant the easy jump to one chosen "
    "player over the remote console, disclosed to everyone, refusing a wrong password?",
    {
        {"map", "arena", "the map (arena is built in; a real one needs --root)"},
        {"root", "", "the folder holding the game folder (id1) with its packs"},
        {"bots", "2", "bots seated beside the three clients"},
        {"password", "secret", "the server's rcon_password"},
        {"seed", "1", "the network's and the game's stream"},
        {"backdrop", "", "a PNG of the game behind the console (a se-client shot); empty: none"},
        {"width", "1280", "the picture's width, px"},
        {"height", "720", "the picture's height, px"},
    },
    "--width=640 --height=360",
    run,
};
GE_LAB(kSpec);

}  // namespace
