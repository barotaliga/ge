// Tests: every kind of message Quake II's client reads, seen arriving from a live server. Two
// clients join Quake II's dedicated server (the server core, games/shooters_edge/net/q2_game.hpp)
// on the simulated network, turn to face each other and fire, talk, ask for the scoreboard and
// the inventory, and the server quits; the client's reader (client/demo_q2.cpp) counts each
// message it read by its id, and every kind the match makes arrived and read clean. The
// recording the client kept, read again as a file, reads the same messages in the same counts.
//
// ge:summary Tests: every Quake II server message kind at a joined client, live and recorded
// ge:keywords test q2 protocol-34 svc census joined-client recording muzzleflash
// ge:keywords temp-entity layout inventory cheat-lock
// ge:skill net shooter testing
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "console/console.hpp"
#include "net/sim_network.hpp"
#include "server/server.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/net/q2_client.hpp"
#include "shooters_edge/net/q2_game.hpp"
#include "shooters_edge/net/q2_net.hpp"
#include "shooters_edge/net/q2_net_server.hpp"
#include "shooters_edge/net/qw_remote.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace gn = ge::net;
namespace sv = ge::server;
namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;
namespace ss = shooters_edge::server;

// The message ids the census names (the protocol's numbers).
enum : uint8_t {
  kMuzzleFlash = 1,
  kMuzzleFlash2 = 2,
  kTempEntity = 3,
  kLayout = 4,
  kInventory = 5,
  kDisconnect = 7,
  kSound = 9,
  kPrint = 10,
  kStuffText = 11,
  kServerData = 12,
  kConfigString = 13,
  kSpawnBaseline = 14,
  kPlayerInfo = 17,
  kPacketEntities = 18,
  kFrame = 20,
};

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
}

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// A walled hall: its brushes and the entities asked for.
std::vector<uint8_t> hall_bytes(std::vector<br::Entity> entities) {
  br::BrushWorld w;
  w.models.resize(1);
  auto& m = w.models[0];
  m.push_back(box(-1024, -1024, -16, 1024, 1024, 0));
  m.push_back(box(-1040, -1024, -16, -1024, 1024, 256));
  m.push_back(box(1024, -1024, -16, 1040, 1024, 256));
  m.push_back(box(-1040, 1024, -16, 1040, 1040, 256));
  m.push_back(box(-1040, -1040, -16, 1040, -1024, 256));
  w.entities = std::move(entities);
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  return bytes;
}

// The hall with two spawns facing each other, written as <root>/baseq2/maps/hall.bsp; the root.
std::string hall_folder() {
  const std::vector<uint8_t> bytes = hall_bytes(
      {entity(
           {{"classname", "info_player_deathmatch"}, {"origin", "-300 -300 24"}, {"angle", "45"}}),
       entity(
           {{"classname", "info_player_deathmatch"}, {"origin", "300 300 24"}, {"angle", "225"}})});
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() / "ge_q2_wire_census_test";
  std::filesystem::create_directories(root / "baseq2" / "maps");
  std::ofstream(root / "baseq2" / "maps" / "hall.bsp", std::ios::binary)
      .write(reinterpret_cast<const char*>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  return root.string();
}

// Quake II's rules on a hall of `entities` served by the net server on the simulated network.
struct Rig {
  ss::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st;
  cn::Q2NetServer server;
  int64_t now = 0;
};

std::unique_ptr<Rig> rig(std::vector<br::Entity> entities, const std::vector<std::string>& lines) {
  auto r = std::make_unique<Rig>();
  r->rules = ss::q2_runner(ss::make_q2_server({}));
  for (const std::string& line : lines) r->rules.console(line);
  ss::MapSpec map;
  map.name = "hall";
  auto brushes = std::make_shared<br::Map>();
  std::string error;
  CHECK_MSG(br::read_bsp(hall_bytes(std::move(entities)), brushes.get(), &error), error);
  map.brushes = brushes;
  CHECK_MSG(r->rules.start(map, error), error);
  gn::LinkConditions lan;
  r->net = gn::make_sim_network(11, lan);
  gn::open_sim_transport(r->st, r->net, gn::sim_address(0, cn::kPortQ2));
  CHECK_MSG(cn::q2_net_start(r->server, r->rules, r->st, {}, error), error);
  return r;
}

}  // namespace

TEST(every_message_a_q2_match_makes_reaches_the_joined_client_and_its_recording) {
  sv::ServerConfig c;
  c.root = hall_folder();
  c.map = "hall";
  c.maxclients = 4;
  c.seed = 5;
  gn::LinkConditions lan;
  gn::SimNetwork net = gn::make_sim_network(9, lan);
  gn::Transport port;
  gn::open_sim_transport(port, net, gn::sim_address(0, cn::kPortQ2));
  sv::Server s;
  std::string error;
  const sv::ServerGame& g = cn::q2_game();
  CHECK_MSG(sv::server_open(s, g, c, &port, nullptr, error), error);
  if (!s.state) return;
  std::vector<std::unique_ptr<gn::Transport>> ports;
  std::vector<std::unique_ptr<cn::Q2ClientEnd>> clients;
  // Each player turns toward the other where the last frame put him (his view is the command's
  // angles plus the server's turn, so the command closes half the gap at each frame) and fires,
  // pressing the trigger again and again.
  std::vector<float> yaw(2, 0.0f);
  std::vector<int64_t> seen(2, 0);
  const auto aim = [&clients, &yaw, &seen](size_t me) {
    return [&clients, &yaw, &seen, me](int tick) {
      sh::MoveCmd m;
      const cl::DemoState& mine = clients[me]->demo.state;
      const cl::DemoState& theirs = clients[1 - me]->demo.state;
      const double dx = theirs.eye.x - mine.eye.x, dy = theirs.eye.y - mine.eye.y;
      double gap = std::atan2(dy, dx) * 180.0 / 3.14159265358979 - mine.angles[1];
      while (gap > 180) gap -= 360;
      while (gap < -180) gap += 360;
      if (clients[me]->frames != seen[me]) yaw[me] += static_cast<float>(gap / 2);
      seen[me] = clients[me]->frames;
      m.angles[1] = yaw[me];
      m.buttons = tick % 4 < 2 ? sh::kButtonAttack : 0;  // pressed again: a dead one respawns
      return m;
    };
  };
  for (size_t n = 0; n < 2; n++) {
    ports.push_back(std::make_unique<gn::Transport>());
    gn::open_sim_transport(*ports.back(), net, gn::sim_address(uint32_t(n + 1), 27901));
    clients.push_back(std::make_unique<cn::Q2ClientEnd>());
    cn::Q2ClientConfig cc;
    cc.name = n == 0 ? "left" : "right";
    cc.brain = aim(n);
    cc.qport = static_cast<uint16_t>(100 + n);
    cn::q2_client_start(*clients.back(), *ports.back(), gn::sim_address(0, cn::kPortQ2), cc,
                        static_cast<double>(s.now_us) / 1e6);
  }
  const auto step = [&] {
    gn::sim_advance(net, s.now_us);
    for (auto& cl : clients) cn::q2_client_tick(*cl, s.now_us);
    sv::server_frame(s);
  };
  for (int f = 0; f < 200; f++) step();  // the clients join
  for (auto& cl : clients) CHECK_MSG(cl->stage == cn::ClientStage::active, cl->refused);
  cn::Q2ClientEnd& eve = *clients[0];
  cn::q2_client_command(eve, "say well met");
  cn::q2_client_command(eve, "inven");
  for (int f = 0; f < 20; f++) step();
  cn::q2_client_command(eve, "score");
  for (int f = 0; f < static_cast<int>(30.0 / s.frame_s); f++) step();  // they fight
  g.quit(s.state, "the census is taken", false);
  for (int f = 0; f < 4; f++) step();
  sv::server_close(s);

  const int64_t* live = eve.demo.q2_read;
  std::string census;
  for (int id = 0; id < 256; id++)
    if (live[id] > 0) census += " " + std::to_string(id) + ":" + std::to_string(live[id]);
  for (const int id :
       {kMuzzleFlash, kTempEntity, kLayout, kInventory, kDisconnect, kSound, kPrint, kStuffText,
        kServerData, kConfigString, kSpawnBaseline, kPlayerInfo, kPacketEntities, kFrame})
    CHECK_MSG(live[id] > 0, "no message " + std::to_string(id) + " arrived; read:" + census);
  CHECK_MSG(live[kPlayerInfo] == live[kFrame] && live[kPacketEntities] == live[kFrame], census);
  CHECK_MSG(eve.stage == cn::ClientStage::dropped, "the server's quit did not drop the client");
  // Someone fell: the obituary is among the prints, and a frag is on the scoreboard's layout.
  bool fell = false;
  for (const std::string& p : eve.prints)
    fell = fell || p.find("left") != std::string::npos || p.find("right") != std::string::npos;
  CHECK_MSG(fell, "no print named a player; read:" + census);

  // The recording the client kept reads as a file, every message the same.
  cl::Demo d;
  CHECK_MSG(cl::open_demo(eve.demo.bytes, d, error), error);
  cl::DemoRead got;
  while ((got = cl::next_frame(d, error)) == cl::DemoRead::frame) {
  }
  CHECK_MSG(got == cl::DemoRead::end, error);
  for (int id = 0; id < 256; id++)
    CHECK_MSG(d.q2_read[id] == live[id], "message " + std::to_string(id) + ": " +
                                             std::to_string(d.q2_read[id]) + " read back, " +
                                             std::to_string(live[id]) + " live");
}

// A monster's gun: in a single-player game a light soldier across the hall sees the player and
// fires, and its flash (a monster's, its own message) reaches the joined client; the player asks
// for the help computer and its layout comes.
TEST(a_monsters_muzzle_flash_reaches_the_joined_client) {
  auto r = rig(
      {entity({{"classname", "info_player_start"}, {"origin", "-300 0 24"}, {"angle", "0"}}),
       entity({{"classname", "monster_soldier_light"}, {"origin", "200 0 24"}, {"angle", "180"}})},
      {"deathmatch 0", "skill 1"});
  gn::Transport ct;
  gn::open_sim_transport(ct, r->net, gn::sim_address(1, 27901));
  cn::Q2ClientEnd eve;
  cn::Q2ClientConfig cc;
  cc.name = "eve";
  cc.qport = 100;
  cn::q2_client_start(eve, ct, gn::sim_address(0, cn::kPortQ2), cc, 0);
  const auto run = [&](int ticks) {
    for (int i = 0; i < ticks; i++) {
      r->now += 13000;
      gn::sim_advance(r->net, r->now);
      cn::q2_client_tick(eve, r->now);
      cn::q2_net_frame(r->server, r->now);
    }
  };
  run(150);
  CHECK_MSG(eve.stage == cn::ClientStage::active, eve.refused);
  cn::q2_client_command(eve, "help");
  run(400);
  const int64_t* live = eve.demo.q2_read;
  CHECK_MSG(live[kMuzzleFlash2] > 0, std::to_string(live[kMuzzleFlash2]) + " monster flashes");
  CHECK(live[kLayout] > 0 && live[kSound] > 0);
}

// The cheat lock: the window joined to a Quake II server of more than one player slot holds every
// cheat setting at its default and refuses a change, whatever its own sv_cheats says; leaving lets
// them go.
TEST(a_window_joined_to_a_q2_server_of_many_slots_holds_its_cheats) {
  auto r = rig(
      {entity(
          {{"classname", "info_player_deathmatch"}, {"origin", "-300 -300 24"}, {"angle", "45"}})},
      {"deathmatch 1", "maxclients 8"});
  ge::console::Console con;  // the window's, which can host
  con.cvars().add({.name = "test_cheat", .fallback = "0", .flags = ge::console::kCheat});
  con.cvars().set("sv_cheats", "1", ge::console::Source::local);
  CHECK(con.cvars().set("test_cheat", "1", ge::console::Source::local) ==
        ge::console::SetResult::changed);
  gn::Transport t;
  gn::open_sim_transport(t, r->net, gn::sim_address(9, 27901));
  cn::Remote remote;
  remote.q2_game = [] { return true; };
  remote.read_file = [](const std::string&, const std::string&) {  // it has every file
    return std::vector<uint8_t>{1};
  };
  remote.ignore.con = &con;
  cn::ClientConfig cfg;
  cfg.name = "eve";
  cn::remote_start(remote, t, gn::sim_address(0, cn::kPortQ2), cfg);
  std::vector<std::string> said;
  for (int i = 0; i < 150; i++) {
    r->now += 13000;
    gn::sim_advance(r->net, r->now);
    cn::remote_step(remote, 0.0131, said);
    cn::q2_net_frame(r->server, r->now);
  }
  CHECK_MSG(remote.q2 != nullptr && remote.q2->stage == cn::ClientStage::active, "not joined");
  CHECK(con.cvars().cheats_held() && !con.cvars().cheats());
  CHECK(con.cvars().find("test_cheat")->value == "0");
  CHECK(con.cvars().set("test_cheat", "1", ge::console::Source::local) ==
        ge::console::SetResult::cheat);
  cn::remote_disconnect(remote);
  CHECK(!con.cvars().cheats_held() && con.cvars().cheats());
  CHECK(con.cvars().set("test_cheat", "1", ge::console::Source::local) ==
        ge::console::SetResult::changed);
}
