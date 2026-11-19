// games/shooters_edge/net/sequel_net*.hpp, sequel_client.hpp, sequel_net_server.hpp: the sequel's
// live protocol. A move and its checksum come back as written; the scripted client joins the
// sequel's net server on the simulated network through the whole signon, its frames are deltas,
// its recording walks whole, and a client that walks forward is seen to move; refusals read as a
// player's words; a lossy link still plays; a map with no sky gets the default one and a
// download is refused; a client fetches the files it lacks and ours keeps the packed ones in a
// partial of their pak until it is whole; the dedicated server game names itself and refuses a
// missing folder; the window joined to a server of the sequel walks, turns by the server's turn,
// sends a weapon key as `use` and leaves; each client is sent only the entities it may see.
//
// ge:summary Test: the sequel's live protocol, our client joined to our server on a simulated net
// ge:keywords test sequel protocol-34 live client server signon move checksum delta refusal loss
// ge:skill net shooter testing
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "forward/forward.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/net/classic_remote.hpp"
#include "shooters_edge/net/classic_wire.hpp"
#include "shooters_edge/net/sequel_client.hpp"
#include "shooters_edge/net/sequel_game.hpp"
#include "shooters_edge/net/sequel_net.hpp"
#include "shooters_edge/net/sequel_net_server.hpp"
#include "shooters_edge/server/sequel_wire.hpp"
#include "vfs/pak.hpp"
#include "vfs/partial.hpp"
#include "vfs/search.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace gn = ge::net;
namespace sv = shooters_edge::server;
namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;
namespace fw = ge::forward;

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
}

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

std::shared_ptr<const br::Map> hall() {
  br::BrushWorld w;
  w.models.resize(1);
  auto& m = w.models[0];
  m.push_back(box(-1024, -1024, -16, 1024, 1024, 0));
  m.push_back(box(-1040, -1024, -16, -1024, 1024, 256));
  m.push_back(box(1024, -1024, -16, 1040, 1024, 256));
  m.push_back(box(-1040, 1024, -16, 1040, 1040, 256));
  m.push_back(box(-1040, -1040, -16, 1040, -1024, 256));
  w.entities.push_back(entity(
      {{"classname", "info_player_deathmatch"}, {"origin", "-400 -400 24"}, {"angle", "0"}}));
  w.entities.push_back(entity(
      {{"classname", "info_player_deathmatch"}, {"origin", "400 400 24"}, {"angle", "180"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

// The hall split in two by a wall at x 0, with visibility put in by hand: every open leaf west of
// the wall cluster 0, east of it cluster 1, and neither sees the other. A spawn and an item in
// each half.
std::shared_ptr<const br::Map> halls() {
  br::BrushWorld w;
  w.models.resize(1);
  auto& m = w.models[0];
  m.push_back(box(-1024, -1024, -16, 1024, 1024, 0));
  m.push_back(box(-1040, -1024, -16, -1024, 1024, 256));
  m.push_back(box(1024, -1024, -16, 1040, 1024, 256));
  m.push_back(box(-1040, 1024, -16, 1040, 1040, 256));
  m.push_back(box(-1040, -1040, -16, 1040, -1024, 256));
  m.push_back(box(-8, -1024, 0, 8, 1024, 256));
  m.push_back(box(-1040, -1040, 256, 1040, 1040, 272));
  w.entities.push_back(entity(
      {{"classname", "info_player_deathmatch"}, {"origin", "-400 -400 24"}, {"angle", "0"}}));
  w.entities.push_back(entity(
      {{"classname", "info_player_deathmatch"}, {"origin", "400 400 24"}, {"angle", "180"}}));
  w.entities.push_back(entity({{"classname", "item_armor_body"}, {"origin", "-300 300 16"}}));
  w.entities.push_back(entity({{"classname", "item_armor_body"}, {"origin", "300 -300 16"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  for (br::Leaf& l : map->leaves) {
    const float mid = (l.mins[0] + l.maxs[0]) / 2;
    l.cluster = l.solid || (l.mins[0] < 0 && l.maxs[0] > 0) ? -1 : mid < 0 ? 0 : 1;
  }
  map->clusters = 2;
  map->row_bytes = 1;
  map->pvs = {1, 2};  // each half sees itself alone
  map->phs.clear();
  br::build_phs(map.get());
  return map;
}

// Whether a client's last frame held an entity within 16 units of (x, y) on the plan.
bool holds_near(const cn::SequelClientEnd& c, float x, float y) {
  for (const auto& e : c.demo.state.entities)
    if (e.present && e.model != 0 &&
        (e.origin.x - x) * (e.origin.x - x) + (e.origin.y - y) * (e.origin.y - y) < 16.0f * 16.0f)
      return true;
  return false;
}

// Our server on the simulated network, and as many clients as asked.
struct Rig {
  std::shared_ptr<const br::Map> map;  // the level's brushes
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st;
  std::vector<std::unique_ptr<gn::Transport>> ct;
  cn::SequelNetServer server;
  std::vector<std::unique_ptr<cn::SequelClientEnd>> clients;
  int64_t now = 0;
};

std::unique_ptr<Rig> rig(const cn::SequelNetConfig& cfg, const char* conditions = "lan",
                         const std::shared_ptr<const br::Map>& brushes = nullptr) {
  auto r = std::make_unique<Rig>();
  r->rules = sv::sequel_runner(sv::make_sequel_server({}));
  r->rules.console("deathmatch 1");
  sv::MapSpec map;
  map.name = "hall";
  r->map = brushes != nullptr ? brushes : hall();
  map.brushes = r->map;
  std::string error;
  CHECK_MSG(r->rules.start(map, error), error);
  gn::LinkConditions link;
  CHECK(gn::conditions_preset(conditions, link));
  r->net = gn::make_sim_network(7, link);
  gn::open_sim_transport(r->st, r->net, gn::sim_address(0, cn::kPortSequel));
  CHECK_MSG(cn::sequel_net_start(r->server, r->rules, r->st, cfg, error), error);
  return r;
}

// A client from host `host` (0: a host of its own).
cn::SequelClientEnd& join(Rig& r, cn::SequelClientConfig cfg, uint32_t host = 0) {
  const int n = static_cast<int>(r.clients.size());
  r.ct.push_back(std::make_unique<gn::Transport>());
  gn::open_sim_transport(*r.ct.back(), r.net,
                         gn::sim_address(host != 0 ? host : uint32_t(n + 1), uint16_t(27901 + n)));
  r.clients.push_back(std::make_unique<cn::SequelClientEnd>());
  cfg.qport = static_cast<uint16_t>(100 + n);
  cn::sequel_client_start(*r.clients.back(), *r.ct.back(), gn::sim_address(0, cn::kPortSequel), cfg,
                          static_cast<double>(r.now) / 1e6);
  return *r.clients.back();
}

void run(Rig& r, int ticks) {
  for (int i = 0; i < ticks; i++) {
    r.now += 13000;
    gn::sim_advance(r.net, r.now);
    for (auto& c : r.clients) cn::sequel_client_tick(*c, r.now);
    cn::sequel_net_frame(r.server, r.now);
  }
}

sh::MoveCmd forward(int /*tick*/) {
  sh::MoveCmd m;
  m.forward = 300;
  return m;
}

}  // namespace

TEST(a_move_and_its_checksum_come_back_as_written) {
  cn::SequelCmd cmds[3];
  cmds[1].forward = 400;
  cmds[1].angles[1] = 8192;
  cmds[2] = cmds[1];
  cmds[2].buttons = 1;
  cmds[2].msec = 16;
  uint8_t buf[64];
  gn::Writer w = gn::make_writer(buf, sizeof buf);
  cn::write_sequel_move(w, 42, cmds, 1234);
  CHECK(!w.overflow);
  CHECK(buf[0] == cn::kClcMove);
  gn::Reader r = gn::make_reader(buf + 1, w.size - 1);
  cn::SequelMove m;
  CHECK(cn::read_sequel_move(r, 1234, m));
  CHECK(m.checksum_ok);
  CHECK(m.frame == 42);
  for (int k = 0; k < 3; k++) CHECK(m.cmds[k] == cmds[k]);
  gn::Reader again = gn::make_reader(buf + 1, w.size - 1);
  CHECK(cn::read_sequel_move(again, 1235, m));
  CHECK(!m.checksum_ok);  // another packet's number: another byte
}

TEST(the_checksum_is_the_byte_a_live_server_expected) {
  // Moves a live server of the sequel refused, with the byte it said it expected (the body after
  // the checksum byte, the packet's sequence): what the measured key table must reproduce.
  struct Vector {
    const char* body;
    uint32_t sequence;
    int expected;
  };
  const Vector vectors[] = {
      {"ab020000082c010d00000d00000d00", 37, 58},
      {"b80200000a00802c010d00000d00000d00", 612, 19},
      {"c10200000a00402c010d00000d00000d00", 959, 7},
      {"cc0200000a00c02c010d00000d00000d00", 1500, 96},
  };
  for (const Vector& v : vectors) {
    std::string body;
    CHECK(cn::from_hex(v.body, body));
    const uint8_t got = cn::sequel_move_checksum(reinterpret_cast<const uint8_t*>(body.data()),
                                                 body.size(), v.sequence);
    CHECK_MSG(got == v.expected, "sequence " + std::to_string(v.sequence) + ": " +
                                     std::to_string(got) + ", the server expected " +
                                     std::to_string(v.expected));
  }
  uint64_t sum = 0, weighted = 0;
  for (int i = 0; i < cn::kSequelChecksumTableSize; i++) {
    sum += cn::kSequelChecksumTable[i];
    weighted += uint64_t(i + 1) * cn::kSequelChecksumTable[i];
  }
  CHECK(sum == 124780);
  CHECK(weighted == 59989697);
  for (int i = 960; i < cn::kSequelChecksumTableSize; i++) CHECK(cn::kSequelChecksumTable[i] == 0);
}

TEST(out_of_band_words_keep_a_quoted_word_whole) {
  const auto w = cn::sequel_words("connect 34 7 99 \"\\name\\a b\\skin\\male/grunt\"\n");
  CHECK(w.size() == 5);
  CHECK(w[4] == "\\name\\a b\\skin\\male/grunt");
}

TEST(a_client_joins_through_the_signon_and_plays_on_deltas) {
  auto r = rig({});
  cn::SequelClientConfig cfg;
  cfg.name = "eve";
  cfg.brain = forward;
  cn::SequelClientEnd& c = join(*r, cfg);
  run(*r, 120);
  CHECK_MSG(c.stage == cn::ClientStage::active, c.refused);
  CHECK(c.signon_steps >= 3);  // configstrings, baselines, begin at least
  CHECK(c.levels == 1);
  CHECK(c.demo.state.map == "hall");
  const float x0 = c.demo.state.eye.x, y0 = c.demo.state.eye.y;
  run(*r, 200);
  CHECK(c.frames >= 20);
  const cn::SequelNetClient& sc = r->server.clients[0];
  CHECK(sc.moves > 100);
  CHECK(sc.bad_checksums == 0);
  CHECK_MSG(sc.full_frames <= 2, std::to_string(sc.full_frames) + " whole frames");
  const float dx = c.demo.state.eye.x - x0, dy = c.demo.state.eye.y - y0;
  CHECK_MSG(dx * dx + dy * dy > 100.0f * 100.0f,
            "the player walked " + std::to_string(dx) + "," + std::to_string(dy));
  cn::sequel_client_stop(c);
  run(*r, 10);
  CHECK(cn::sequel_net_clients(r->server) == 0);
  // The recording it kept walks whole.
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(c.demo.bytes, d, error), error);
  int frames = 0;
  cl::DemoRead got;
  while ((got = cl::next_frame(d, error)) == cl::DemoRead::frame) frames++;
  CHECK_MSG(got == cl::DemoRead::end, error);
  CHECK(frames == c.frames);
}

TEST(a_map_with_no_sky_gets_the_default_and_a_download_is_refused) {
  // A client that finds no sky asks for one to download; it is refused and plays on.
  cn::SequelNetConfig cfg;
  sv::sequel_sky(*hall(), cfg.wire);
  CHECK(cfg.wire.sky == "unit1_");
  CHECK(cfg.wire.sky_rotate == "0.000000");
  auto r = rig(cfg);
  cn::SequelClientConfig cc;
  cc.brain = forward;
  cn::SequelClientEnd& c = join(*r, cc);
  run(*r, 120);
  CHECK_MSG(c.stage == cn::ClientStage::active, c.refused);
  cn::sequel_client_command(c, "download env/rt.tga");
  const auto frames = c.frames;
  run(*r, 120);
  CHECK_MSG(c.error.empty(), c.error);
  CHECK(c.stage == cn::ClientStage::active);
  CHECK(c.frames > frames + 5);
  bool noted = false;
  for (const std::string& line : r->server.log)
    noted = noted || line.find("asked for env/rt.tga") != std::string::npos;
  CHECK(noted);
}

TEST(a_level_change_keeps_every_connection_and_seats_each_client_again) {
  auto r = rig({});
  cn::SequelClientConfig cfg;
  cfg.brain = forward;
  cfg.name = "eve";
  cn::SequelClientEnd& a = join(*r, cfg);
  cfg.name = "bob";
  cn::SequelClientEnd& b = join(*r, cfg);
  run(*r, 160);
  CHECK_MSG(a.stage == cn::ClientStage::active, a.refused);
  CHECK_MSG(b.stage == cn::ClientStage::active, b.refused);
  const int32_t count = r->server.count;
  // What the dedicated game does when the rules ask for a map: start them on it and tell the
  // clients (sequel_game.cpp).
  sv::MapSpec next;
  next.name = "hall2";
  next.brushes = hall();
  std::string error;
  r->rules.carry();
  CHECK_MSG(r->rules.start(next, error), error);
  sv::SequelWireOptions wire = r->server.cfg.wire;
  wire.map_checksum = 1234;
  cn::sequel_net_level(r->server, wire);
  CHECK(r->server.count == count + 1);
  // The server data carries the new count: a client keeps its loading picture up until it does.
  CHECK(r->server.cfg.wire.server_count == count + 1 - 0x10000);
  run(*r, 1);
  cn::sequel_client_stop(b);  // gone while it signs on again: its slot in the rules is freed
  const auto frames = a.frames;
  run(*r, 200);
  CHECK_MSG(a.error.empty(), a.error);
  CHECK(a.stage == cn::ClientStage::active);
  CHECK(a.levels == 2);
  CHECK(a.demo.state.map == "hall2");
  CHECK(a.frames > frames + 10);
  CHECK(cn::sequel_net_clients(r->server) == 1);
  CHECK(r->server.clients[0].stage == cn::SequelSlot::active);
  CHECK((*r->rules).clients[0].active);
  CHECK(!(*r->rules).clients[1].active);
  bool noted = false;
  for (const std::string& line : r->server.log)
    noted = noted || line.find("the level changed to hall2") != std::string::npos;
  CHECK(noted);
}

TEST(the_sequel_server_game_names_itself_and_refuses_a_missing_folder) {
  const ge::server::ServerGame& g = cn::sequel_game();
  CHECK(g.default_port == cn::kPortSequel);
  CHECK(g.binary == std::string("sesv"));
  ge::server::ServerConfig c;
  c.root = "no-such-folder-here";
  std::string error;
  CHECK(g.create(c, error) == nullptr);
  CHECK_MSG(!error.empty(), "a refusal says why");
}

TEST(refusals_read_as_a_players_words) {
  cn::SequelNetConfig cfg;
  cfg.password = "secret";
  cfg.max_clients = 1;
  auto r = rig(cfg);
  cn::SequelClientConfig wrong;
  wrong.name = "mallory";
  cn::SequelClientEnd& a = join(*r, wrong);
  run(*r, 40);
  CHECK(a.stage == cn::ClientStage::dropped);
  CHECK(a.refused == "Password required or incorrect.\nConnection refused.\n");
  cn::SequelClientConfig right;
  right.password = "secret";
  cn::SequelClientEnd& b = join(*r, right);
  run(*r, 80);
  CHECK_MSG(b.stage == cn::ClientStage::active, b.refused);
  cn::SequelClientEnd& full = join(*r, right);
  run(*r, 40);
  CHECK(full.stage == cn::ClientStage::dropped);
  CHECK(full.refused == "Server is full.\n");
}

// sv_iplimit (3 by default here): a fourth client from one host is refused, another host's taken.
TEST(a_fourth_client_from_one_address_is_refused) {
  cn::SequelNetConfig cfg;
  cfg.max_clients = 8;
  auto r = rig(cfg);
  for (int i = 0; i < 3; i++) join(*r, {}, 50);
  run(*r, 80);
  for (int i = 0; i < 3; i++)
    CHECK_MSG(r->clients[size_t(i)]->stage == cn::ClientStage::active,
              r->clients[size_t(i)]->refused);
  cn::SequelClientEnd& fourth = join(*r, {}, 50);
  cn::SequelClientEnd& other = join(*r, {});
  run(*r, 80);
  CHECK(fourth.stage == cn::ClientStage::dropped);
  CHECK(fourth.refused == "Too many connections from your IP address.\nConnection refused.\n");
  CHECK_MSG(other.stage == cn::ClientStage::active, other.refused);
}

TEST(a_lossy_link_still_plays) {
  auto r = rig({}, "bad");
  cn::SequelClientConfig cfg;
  cfg.brain = forward;
  cn::SequelClientEnd& c = join(*r, cfg);
  run(*r, 800);
  CHECK_MSG(c.stage == cn::ClientStage::active, c.refused);
  CHECK(c.frames > 30);
  CHECK(r->server.clients[0].moves > 300);
}

TEST(a_client_plays_through_two_forwarders) {
  auto r = rig({});
  // Forwarders on nodes 20 and 21 (their upstream sockets from port 40000), the server on node 0.
  const gn::Address a = gn::sim_address(20, 30000), b = gn::sim_address(21, 30000);
  gn::Transport a_port, b_port;
  gn::open_sim_transport(a_port, r->net, a);
  gn::open_sim_transport(b_port, r->net, b);
  uint16_t next_up = 40000;
  auto opener = [&r, &next_up](uint32_t node) {
    return [&r, &next_up, node](gn::Transport& t) {
      return gn::open_sim_transport(t, r->net, gn::sim_address(node, next_up++));
    };
  };
  fw::Forward fa, fb;
  fw::ForwardConfig fc;
  fc.name = "first";
  fc.allow = {b};
  fc.seed = 1;
  fw::forward_init(fa, fc, a_port, opener(20));
  fc.name = "second";
  fc.allow = {gn::sim_address(0, cn::kPortSequel)};
  fc.seed = 2;
  fw::forward_init(fb, fc, b_port, opener(21));
  cn::SequelClientConfig cfg;
  cfg.brain = forward;
  cfg.via = {a, b};
  cn::SequelClientEnd& c = join(*r, cfg);
  for (int i = 0; i < 300; i++) {
    r->now += 13000;
    gn::sim_advance(r->net, r->now);
    fw::forward_update(fa, r->now);
    fw::forward_update(fb, r->now);
    cn::sequel_client_tick(c, r->now);
    cn::sequel_net_frame(r->server, r->now);
  }
  CHECK_MSG(c.stage == cn::ClientStage::active, c.refused);
  CHECK(c.dial.state == fw::DialState::routed);
  CHECK(c.frames > 20);
  CHECK(r->server.clients[0].moves > 200);
  CHECK(r->server.clients[0].bad_checksums == 0);
  // The server sees the second forwarder, not the client.
  CHECK(gn::sim_node(r->server.clients[0].address) == 21);
  fw::forward_close(fa);
  fw::forward_close(fb);
}

TEST(the_window_joins_a_server_of_the_sequel_and_plays) {
  auto r = rig({});
  gn::Transport t;
  gn::open_sim_transport(t, r->net, gn::sim_address(9, 27901));
  cn::Remote remote;
  remote.sequel_game = [] { return true; };
  remote.read_file = [](const std::string&, const std::string&) {  // it has every file
    return std::vector<uint8_t>{1};
  };
  cn::ClientConfig cfg;
  cfg.name = "eve";
  cn::remote_start(remote, t, gn::sim_address(0, cn::kPortSequel), cfg);
  CHECK(remote.sequel != nullptr);
  std::vector<std::string> said;
  std::vector<std::string> heard;
  int frames = 0, levels = 0;
  const auto steps = [&](int n) {
    for (int i = 0; i < n; i++) {
      r->now += 13000;
      gn::sim_advance(r->net, r->now);
      const cn::RemoteStep got = cn::remote_step(remote, 0.0131, said);
      frames += got == cn::RemoteStep::frame ? 1 : 0;
      levels += got == cn::RemoteStep::new_level ? 1 : 0;
      for (const cl::DemoPrint& p : remote.demo.state.prints) heard.push_back(p.text);
      cn::sequel_net_frame(r->server, r->now);
    }
  };
  steps(150);
  CHECK_MSG(remote.sequel->stage == cn::ClientStage::active, remote.sequel->refused);
  CHECK(levels == 1);
  CHECK(frames > 5);
  CHECK(remote.demo.state.map == "hall");
  CHECK(cn::remote_status(remote) == "playing on " + remote.server_name);
  CHECK(!said.empty() && said.back() == "playing on " + remote.server_name);
  // The drawn view is the hands' plus the server's turn: the spawn faced 0 or 180, the hands 0.
  const float yaw = remote.demo.state.angles.y;
  CHECK_MSG(yaw == 0 || std::abs(std::abs(yaw) - 180) < 0.1f, std::to_string(yaw));
  // The hands walk the player; frames come in between, drawn between the last two.
  const sh::Vec3f eye0 = remote.demo.state.eye;
  remote.hands.forward = 300;
  steps(150);
  const sh::Vec3f moved = remote.demo.state.eye - eye0;
  CHECK_MSG(sh::length(moved) > 100, "walked " + std::to_string(sh::length(moved)));
  CHECK_MSG(remote.lerped > 0, "nothing drawn between frames");
  // A weapon key goes as the game's own `use`, and an unknown word goes to the server.
  remote.hands.forward = 0;
  remote.hands.impulse = 2;
  steps(40);
  CHECK(remote.hands.impulse == 0);
  CHECK(cn::remote_rules_word(remote, "use Railgun"));
  steps(40);
  const auto heard_line = [&](const std::string& line) {
    return std::find(heard.begin(), heard.end(), line) != heard.end();
  };
  CHECK(heard_line("You have no Shotgun.\n"));
  CHECK(heard_line("You have no Railgun.\n"));
  // Leaving tells the server.
  cn::remote_disconnect(remote);
  CHECK(!remote.active && remote.sequel == nullptr);
  steps(10);
  CHECK(cn::sequel_net_clients(r->server) == 0);
}

TEST(a_client_fetches_what_it_lacks_and_ours_keeps_it_in_its_archive) {
  // The server's pak1.pak holds the map and a skin; another skin is loose in its folder. Our
  // client lacks all three: it is told the packed files' archive, fetches each (the skin in
  // several pieces), keeps them in pak1.partial.pak and ends with the server's pak1.pak byte for
  // byte, the loose skin told no archive. A client that does not ask for archives (as a stock
  // client) is refused the packed map and gets the skins loose; both play on.
  std::string error;
  std::vector<uint8_t> pak;
  const std::vector<uint8_t> red(2500, 7), blue(300, 9);
  CHECK_MSG(ge::vfs::write_pak(
                {{"maps/hall.bsp", std::vector<uint8_t>(5000, 1)}, {"players/male/red.pcx", red}},
                &pak, &error),
            error);
  ge::vfs::SearchPath files;
  CHECK_MSG(ge::vfs::add_archive(&files, "pak1.pak", pak, "baseq2", &error), error);
  cn::SequelNetConfig cfg;
  cfg.read_file = [&files, &blue](const std::string& path, std::vector<uint8_t>* out) {
    if (path == "players/male/blue.pcx") {
      *out = blue;
      return true;
    }
    const ge::vfs::FileRef* f = ge::vfs::find(files, path);
    return f != nullptr && ge::vfs::read(files, *f, out, nullptr);
  };
  cfg.file_origin = [&files](const std::string& path, ge::vfs::ArchiveOrigin* out) {
    const ge::vfs::FileRef* f = ge::vfs::find(files, path);
    return f != nullptr && ge::vfs::archive_origin(files, *f, out);
  };
  auto r = rig(cfg);
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "ge_sequel_fetch";
  std::filesystem::remove_all(dir);
  int told = 0, untold = 0;
  cn::SequelClientConfig ours;
  ours.name = "ours";
  ours.fetch = true;
  ours.want = {"players/male/red.pcx", "players/male/blue.pcx"};
  ours.keep_file = [&](const std::string& path, const std::vector<uint8_t>& bytes,
                       const ge::vfs::ArchiveOrigin* origin) {
    if (origin == nullptr) {
      untold++;
      return;
    }
    told++;
    ge::vfs::Kept kept;
    std::string why;
    CHECK_MSG(ge::vfs::keep_in_partial(dir.string(), *origin, path, bytes, &kept, &why), why);
  };
  cn::SequelClientConfig stock = ours;
  stock.name = "stock";
  stock.ask_origins = false;
  int stock_told = 0;
  stock.keep_file = [&stock_told](const std::string&, const std::vector<uint8_t>&,
                                  const ge::vfs::ArchiveOrigin* origin) {
    stock_told += origin != nullptr ? 1 : 0;
  };
  cn::SequelClientEnd& a = join(*r, ours);
  cn::SequelClientEnd& b = join(*r, stock);
  run(*r, 300);
  CHECK_MSG(a.stage == cn::ClientStage::active, a.refused);
  CHECK_MSG(b.stage == cn::ClientStage::active, b.refused);
  CHECK(a.downloaded.size() == 3 && a.downloads_refused == 0);
  CHECK(a.downloaded["players/male/red.pcx"] == red);
  CHECK_MSG(told == 2 && untold == 1, std::to_string(told) + " told, " + std::to_string(untold));
  std::ifstream in(dir / "pak1.pak", std::ios::binary);
  CHECK(std::vector<uint8_t>(std::istreambuf_iterator<char>(in), {}) == pak);
  CHECK(!std::filesystem::exists(dir / "pak1.partial.pak"));
  CHECK(b.downloaded.size() == 2 && b.downloads_refused == 1 && stock_told == 0);
  CHECK(b.downloaded.count("maps/hall.bsp") == 0);
  std::filesystem::remove_all(dir);
}

TEST(each_client_is_sent_only_what_it_may_see) {
  for (const bool cut : {true, false}) {
    cn::SequelNetConfig cfg;
    cfg.cut_to_view = cut;
    auto r = rig(cfg, "lan", halls());
    cn::SequelClientConfig cc;
    cc.name = "eve";
    cn::SequelClientEnd& a = join(*r, cc);
    cc.name = "bob";
    cn::SequelClientEnd& b = join(*r, cc);
    run(*r, 200);
    for (const cn::SequelClientEnd* c : {&a, &b}) {
      CHECK_MSG(c->stage == cn::ClientStage::active, c->refused);
      const bool in_west = c->demo.state.eye.x < 0;
      const bool in_east = c->demo.state.eye.x > 0;
      CHECK(in_west != in_east);
      const bool west_item = holds_near(*c, -300, 300), east_item = holds_near(*c, 300, -300);
      if (!cut) {
        CHECK(west_item && east_item);
      } else {
        CHECK_MSG(west_item == in_west,
                  "the west armour, seen from the " + std::string(in_west ? "west" : "east"));
        CHECK_MSG(east_item == in_east,
                  "the east armour, seen from the " + std::string(in_west ? "west" : "east"));
      }
    }
    int64_t kept_out = 0;
    for (const cn::SequelNetClient& sc : r->server.clients) kept_out += sc.wire.cut;
    CHECK_MSG((kept_out > 0) == cut, std::to_string(kept_out) + " entities kept out");
  }
}

TEST(the_window_predicts_its_moves_on_a_server_of_the_sequel) {
  auto r = rig({}, "broadband");
  gn::Transport t;
  gn::open_sim_transport(t, r->net, gn::sim_address(9, 27901));
  cn::Remote remote;
  remote.sequel_game = [] { return true; };
  remote.read_file = [](const std::string&, const std::string&) { return std::vector<uint8_t>{1}; };
  cn::ClientConfig cfg;
  cfg.name = "eve";
  cn::remote_start(remote, t, gn::sim_address(0, cn::kPortSequel), cfg);
  std::vector<std::string> said;
  float most_correction = 0, highest = -1e9f;
  int predicted = 0, most_replayed = 0;
  const auto steps = [&](int n) {
    for (int i = 0; i < n; i++) {
      r->now += 13000;
      gn::sim_advance(r->net, r->now);
      if (cn::remote_step(remote, 0.0131, said) == cn::RemoteStep::new_level)
        cn::remote_world(remote, nullptr, r->map.get());  // as the window does, the level loaded
      cn::sequel_net_frame(r->server, r->now);
      const cn::SequelPredict& p = remote.sequel_ahead;
      if (!p.predicted) continue;
      predicted++;
      most_replayed = std::max(most_replayed, p.replayed);
      most_correction = std::max(most_correction, sh::length(p.smooth.correction));
      highest = std::max(highest, p.origin.z);
    }
  };
  steps(200);
  CHECK_MSG(remote.sequel->stage == cn::ClientStage::active, remote.sequel->refused);
  CHECK(predicted > 0);
  // Walking and jumping: the drawn place leads the server's by the moves in flight, and the
  // server's frames prove every prediction right (the same move, bit for bit): no correction.
  const sh::Vec3f start = remote.sequel_ahead.origin;
  remote.hands.forward = 300;
  remote.hands.buttons = sh::kButtonJump;
  steps(150);
  remote.hands = {};
  steps(100);
  CHECK_MSG(most_replayed >= 2, std::to_string(most_replayed) + " moves ahead at most");
  CHECK_MSG(most_correction == 0, std::to_string(most_correction) + " units corrected");
  const sh::Vec3f moved = remote.sequel_ahead.origin - start;
  CHECK_MSG(sh::length(moved) > 100, "walked " + std::to_string(sh::length(moved)));
  CHECK_MSG(highest > start.z + 20, "rose " + std::to_string(highest - start.z));  // it jumped
  // Standing still, the prediction is the server's place exactly.
  const cl::DemoState& st = remote.demo.state;
  const cl::SequelView& v = remote.demo.views[static_cast<size_t>(remote.demo.last_frame & 15)];
  CHECK(remote.sequel_ahead.origin.x == static_cast<float>(v.origin[0]) * 0.125f);
  CHECK(remote.sequel_ahead.origin.y == static_cast<float>(v.origin[1]) * 0.125f);
  CHECK(remote.sequel_ahead.origin.z == static_cast<float>(v.origin[2]) * 0.125f);
  CHECK(st.eye.z > remote.sequel_ahead.origin.z);
  // cl_predict_crouch 2: a duck lowers the eye the frame it goes down, ahead of the server's
  // frames, which catch up; 0: the eye stays the server's.
  const auto crouch = [&](int mode) {
    remote.sequel_ahead.crouch = mode;
    remote.hands.up = -400;
    float lowest = 0;
    for (int i = 0; i < 40; i++) {
      steps(1);
      lowest = std::min(lowest, remote.sequel_ahead.eye_shift);
    }
    const float last = remote.sequel_ahead.eye_shift;
    remote.hands = {};
    steps(40);
    return std::pair{lowest, last};
  };
  const auto [ducked, caught_up] = crouch(2);
  CHECK_MSG(ducked == ge::classic::kSqViewDucked - ge::classic::kSqViewStand,
            "the eye moved " + std::to_string(ducked));
  CHECK_MSG(caught_up == 0, std::to_string(caught_up) + " once the server ducked too");
  CHECK(crouch(0).first == 0);
  // cl_nopred: the server's place drawn.
  remote.nopred = true;
  steps(5);
  CHECK(!remote.sequel_ahead.predicted && !remote.predicted);
}

TEST(the_window_runs_the_other_players_forward_on_our_server_of_the_sequel) {
  auto r = rig({}, "broadband");
  bool go = false;
  cn::SequelClientConfig bob;
  bob.name = "bob";
  bob.brain = [&go](int) {
    sh::MoveCmd m;
    m.forward = go ? 300 : 0;
    return m;
  };
  join(*r, bob);
  gn::Transport t;
  gn::open_sim_transport(t, r->net, gn::sim_address(9, 27901));
  cn::Remote remote;
  remote.sequel_game = [] { return true; };
  remote.read_file = [](const std::string&, const std::string&) { return std::vector<uint8_t>{1}; };
  cn::ClientConfig cfg;
  cfg.name = "eve";
  cn::remote_start(remote, t, gn::sim_address(0, cn::kPortSequel), cfg);
  std::vector<std::string> said;
  const sv::SequelServer* game = sv::sequel_of(r->rules);
  // Each step: how far the drawn bob is from where the server has him now.
  double off = 0;
  int drawn = 0, most_others = 0;
  const auto steps = [&](int n) {
    for (int i = 0; i < n; i++) {
      r->now += 13000;
      gn::sim_advance(r->net, r->now);
      for (auto& c : r->clients) cn::sequel_client_tick(*c, r->now);
      if (cn::remote_step(remote, 0.013, said) == cn::RemoteStep::new_level)
        cn::remote_world(remote, nullptr, r->map.get());
      cn::sequel_net_frame(r->server, r->now);
      most_others = std::max(most_others, remote.others);
      const cl::DemoState& st = remote.demo.state;
      for (int k = 0; k < sh::kMaxClients; k++) {
        if (r->server.clients[k].name != "bob" || static_cast<size_t>(k) >= st.players.size() ||
            !st.players[static_cast<size_t>(k)].present)
          continue;
        const ge::classic::SequelState& m = game->players[k].move;
        const sh::Vec3f at{static_cast<float>(m.origin[0]) * 0.125f,
                           static_cast<float>(m.origin[1]) * 0.125f,
                           static_cast<float>(m.origin[2]) * 0.125f};
        off += sh::length(st.players[static_cast<size_t>(k)].origin - at);
        drawn++;
      }
    }
  };
  steps(200);
  CHECK_MSG(remote.sequel->stage == cn::ClientStage::active, remote.sequel->refused);
  CHECK(remote.sequel->moves_read > 0);  // our server sends the moves to a client that asks
  go = true;
  steps(30);  // up to speed
  off = 0;
  drawn = 0;
  steps(70);
  const double ahead = off / std::max(drawn, 1);
  CHECK_MSG(most_others >= 1, "no other player run forward");
  // cl_predict_players 0: drawn between the frames, a frame and a round trip behind.
  remote.predict_players = false;
  off = 0;
  drawn = 0;
  steps(70);
  const double behind = off / std::max(drawn, 1);
  CHECK(remote.others == 0);
  CHECK_MSG(drawn > 0 && ahead * 2 < behind, "run forward " + std::to_string(ahead) +
                                                 " units off, between frames " +
                                                 std::to_string(behind));
}

// How far, on average, the window draws a runner circling at full speed from where the server
// has him at each step, with the moves between the frames (`between`) or at the frames only.
double circling_off(bool between, int* moves_read) {
  cn::SequelNetConfig sc;
  sc.moves_between = between;
  auto r = rig(sc, "broadband");
  bool go = false;
  cn::SequelClientConfig bob;
  bob.name = "bob";
  bob.brain = [&go](int tick) {
    sh::MoveCmd m;
    m.forward = go ? 300 : 0;
    m.angles.y = static_cast<float>((tick * 3) % 360);  // a full turn in about 1.6 s
    return m;
  };
  join(*r, bob);
  gn::Transport t;
  gn::open_sim_transport(t, r->net, gn::sim_address(9, 27901));
  cn::Remote remote;
  remote.sequel_game = [] { return true; };
  remote.read_file = [](const std::string&, const std::string&) { return std::vector<uint8_t>{1}; };
  cn::ClientConfig cfg;
  cfg.name = "eve";
  cn::remote_start(remote, t, gn::sim_address(0, cn::kPortSequel), cfg);
  std::vector<std::string> said;
  const sv::SequelServer* game = sv::sequel_of(r->rules);
  double off = 0;
  int drawn = 0;
  const auto steps = [&](int n, bool count) {
    for (int i = 0; i < n; i++) {
      r->now += 13000;
      gn::sim_advance(r->net, r->now);
      for (auto& c : r->clients) cn::sequel_client_tick(*c, r->now);
      if (cn::remote_step(remote, 0.013, said) == cn::RemoteStep::new_level)
        cn::remote_world(remote, nullptr, r->map.get());
      cn::sequel_net_frame(r->server, r->now);
      const cl::DemoState& st = remote.demo.state;
      for (int k = 0; count && k < sh::kMaxClients; k++) {
        if (r->server.clients[k].name != "bob" || static_cast<size_t>(k) >= st.players.size() ||
            !st.players[static_cast<size_t>(k)].present)
          continue;
        const ge::classic::SequelState& m = game->players[k].move;
        const sh::Vec3f at{static_cast<float>(m.origin[0]) * 0.125f,
                           static_cast<float>(m.origin[1]) * 0.125f,
                           static_cast<float>(m.origin[2]) * 0.125f};
        off += sh::length(st.players[static_cast<size_t>(k)].origin - at);
        drawn++;
      }
    }
  };
  steps(200, false);
  go = true;
  steps(30, false);
  steps(150, true);
  *moves_read = static_cast<int>(remote.sequel->moves_read);
  CHECK(drawn > 0);
  return off / std::max(drawn, 1);
}

TEST(the_moves_between_frames_keep_a_turning_runner_closer) {
  int at_frames = 0, between = 0;
  const double frames_off = circling_off(false, &at_frames);
  const double between_off = circling_off(true, &between);
  CHECK_MSG(between > at_frames * 2,
            std::to_string(between) + " moves messages against " + std::to_string(at_frames));
  CHECK_MSG(between_off < frames_off, "between frames " + std::to_string(between_off) +
                                          " units off, at frames only " +
                                          std::to_string(frames_off));
}

TEST(a_stock_server_s_players_run_on_from_its_frames) {
  auto r = rig({}, "broadband");
  bool go = false;
  cn::SequelClientConfig bob;
  bob.name = "bob";
  bob.brain = [&go](int) {
    sh::MoveCmd m;
    m.forward = go ? 300 : 0;
    return m;
  };
  join(*r, bob);
  gn::Transport t;
  gn::open_sim_transport(t, r->net, gn::sim_address(9, 27901));
  cn::Remote remote;
  remote.sequel_game = [] { return true; };
  remote.read_file = [](const std::string&, const std::string&) { return std::vector<uint8_t>{1}; };
  cn::ClientConfig cfg;
  cfg.name = "eve";
  cn::remote_start(remote, t, gn::sim_address(0, cn::kPortSequel), cfg);
  std::vector<std::string> said;
  const sv::SequelServer* game = sv::sequel_of(r->rules);
  double off = 0;
  int drawn = 0, most_others = 0;
  const auto steps = [&](int n) {
    for (int i = 0; i < n; i++) {
      r->now += 13000;
      gn::sim_advance(r->net, r->now);
      for (auto& c : r->clients) cn::sequel_client_tick(*c, r->now);
      if (cn::remote_step(remote, 0.013, said) == cn::RemoteStep::new_level)
        cn::remote_world(remote, nullptr, r->map.get());
      for (cn::SequelNetClient& c : r->server.clients)
        if (c.name == "eve") c.told_moves = false;  // as a stock server: no moves sent
      cn::sequel_net_frame(r->server, r->now);
      most_others = std::max(most_others, remote.others);
      const cl::DemoState& st = remote.demo.state;
      for (int k = 0; k < sh::kMaxClients; k++) {
        if (r->server.clients[k].name != "bob" || static_cast<size_t>(k) >= st.players.size() ||
            !st.players[static_cast<size_t>(k)].present)
          continue;
        const ge::classic::SequelState& m = game->players[k].move;
        const sh::Vec3f at{static_cast<float>(m.origin[0]) * 0.125f,
                           static_cast<float>(m.origin[1]) * 0.125f,
                           static_cast<float>(m.origin[2]) * 0.125f};
        off += sh::length(st.players[static_cast<size_t>(k)].origin - at);
        drawn++;
      }
    }
  };
  steps(200);
  CHECK_MSG(remote.sequel->stage == cn::ClientStage::active, remote.sequel->refused);
  go = true;
  steps(30);
  off = 0;
  drawn = 0;
  steps(70);
  const double run_on = off / std::max(drawn, 1);
  CHECK(remote.sequel->moves_read == 0);
  CHECK_MSG(most_others >= 1, "no other player run on");
  remote.predict_players = false;
  off = 0;
  drawn = 0;
  steps(70);
  const double between = off / std::max(drawn, 1);
  CHECK_MSG(
      drawn > 0 && run_on * 2 < between,
      "run on " + std::to_string(run_on) + " units off, between frames " + std::to_string(between));
}

TEST(a_stock_client_is_not_sent_the_other_players_moves) {
  auto r = rig({});
  cn::SequelClientConfig a;
  a.name = "a";
  a.brain = forward;
  cn::SequelClientConfig b = a;
  b.name = "b";
  b.ask_moves = true;
  cn::SequelClientEnd& ca = join(*r, a);
  cn::SequelClientEnd& cb = join(*r, b);
  run(*r, 200);
  CHECK(ca.stage == cn::ClientStage::active && cb.stage == cn::ClientStage::active);
  CHECK(ca.moves_read == 0 && ca.demo.other_moves.empty());
  CHECK(cb.moves_read > 0);
  CHECK(cb.demo.other_moves.size() == 1);
}

// The easy jump on a server of the sequel: the user info says `cl_speedjump 1`, our server tells
// the share (its pm_speedjump), and the hands strafe for forward and jump held, past the run's 300.
namespace {

struct JumpRun {
  float told = -2, fastest = 0;
  bool said = false;
};

JumpRun easy_jump_run(const char* share, bool aid) {
  auto r = rig({}, "broadband");
  if (share != nullptr) r->rules.console(std::string("pm_speedjump ") + share);
  gn::Transport t;
  gn::open_sim_transport(t, r->net, gn::sim_address(9, 27901));
  cn::Remote remote;
  remote.sequel_game = [] { return true; };
  remote.read_file = [](const std::string&, const std::string&) { return std::vector<uint8_t>{1}; };
  cn::ClientConfig cfg;
  cfg.name = "eve";
  cfg.speedjump = aid;
  cn::remote_start(remote, t, gn::sim_address(0, cn::kPortSequel), cfg);
  std::vector<std::string> said;
  JumpRun out;
  const auto steps = [&](int n) {
    for (int i = 0; i < n; i++) {
      r->now += 13000;
      gn::sim_advance(r->net, r->now);
      if (cn::remote_step(remote, 0.0131, said) == cn::RemoteStep::new_level)
        cn::remote_world(remote, nullptr, r->map.get());
      cn::sequel_net_frame(r->server, r->now);
      const sh::Vec3f v = remote.sequel_ahead.velocity;
      if (remote.sequel_ahead.predicted)
        out.fastest = std::max(out.fastest, std::sqrt(v.x * v.x + v.y * v.y));
    }
  };
  steps(200);
  CHECK_MSG(remote.sequel->stage == cn::ClientStage::active, remote.sequel->refused);
  out.fastest = 0;
  remote.hands.forward = 300;
  remote.hands.buttons = sh::kButtonJump;
  steps(260);
  out.told = remote.demo.speedjump_share;
  for (const cn::SequelNetClient& c : r->server.clients)
    if (c.stage != cn::SequelSlot::free)
      out.said = c.userinfo.find("cl_speedjump\\1") != std::string::npos;
  return out;
}

}  // namespace

TEST(the_easy_jump_strafes_on_our_server_of_the_sequel_at_its_share) {
  const JumpRun plain = easy_jump_run(nullptr, false);
  const JumpRun eased = easy_jump_run("0.8", true);
  const JumpRun off = easy_jump_run("0", true);
  CHECK(!plain.said && plain.told == -1);  // not asked: nothing said, nothing told
  CHECK(eased.said && eased.told == 0.8f);
  CHECK(off.said && off.told == 0);
  CHECK_MSG(plain.fastest <= 320, "held forward and jump: " + std::to_string(plain.fastest));
  CHECK_MSG(eased.fastest > plain.fastest + 60,
            "eased " + std::to_string(eased.fastest) + " against " + std::to_string(plain.fastest));
  CHECK_MSG(off.fastest <= plain.fastest + 20, "share 0: " + std::to_string(off.fastest));
}

TEST(the_rate_holds_back_frames_past_a_clients_bytes_a_second) {
  auto r = rig({});
  cn::SequelClientConfig fast, slow;
  fast.name = "eve";
  fast.brain = forward;
  slow = fast;
  slow.name = "bob";
  slow.rate = 100;  // the least the sequel takes: a frame now and then
  cn::SequelClientEnd& f = join(*r, fast);
  cn::SequelClientEnd& s = join(*r, slow);
  run(*r, 120);
  CHECK_MSG(f.stage == cn::ClientStage::active && s.stage == cn::ClientStage::active,
            f.refused + s.refused);
  const int64_t f0 = f.frames, s0 = s.frames;
  run(*r, 231);  // three seconds
  const cn::SequelNetClient &sf = r->server.clients[0], &ss = r->server.clients[1];
  CHECK(sf.rate == 15000 && ss.rate == 100);
  CHECK_MSG(f.frames - f0 >= 28 && sf.held == 0, std::to_string(f.frames - f0) + " frames");
  CHECK_MSG(s.frames - s0 >= 1 && s.frames - s0 <= 5 && ss.held >= 20,
            std::to_string(s.frames - s0) + " frames, " + std::to_string(ss.held) + " held");
  CHECK(ss.wire.suppress <= 10);  // the count the next frame carries, a frame's worth at most
  CHECK(ss.full_frames <= 2);     // a held frame is no lost frame: the deltas go on
  CHECK(s.stage == cn::ClientStage::active);
}

TEST(a_frame_carries_the_count_held_back_before_it) {
  auto r = rig({});
  run(*r, 20);
  sv::SequelWire w;
  sv::SequelSignon signon;
  sv::sequel_signon_parts(w, *r->rules, 0, {}, signon);
  w.suppress = 6;
  std::vector<uint8_t> block, reliable;
  sv::sequel_frame(w, *r->rules, 3, nullptr, block, &reliable);
  CHECK(block.size() > 10 && block[0] == 20);  // the frame first: id, number, delta's, suppress
  CHECK(block[9] == 6);
  CHECK(w.suppress == 0);
  sv::sequel_frame(w, *r->rules, 4, nullptr, block, &reliable);
  CHECK(block[9] == 0);
}

TEST(a_rate_out_of_the_servers_bounds_is_set_back_and_the_player_told) {
  cn::SequelNetConfig cfg;
  cfg.max_rate = 3000;
  cfg.min_rate = 2000;
  auto r = rig(cfg);
  cn::SequelClientConfig high, low;
  high.name = "eve";
  low.name = "bob";
  low.rate = 500;
  cn::SequelClientEnd& h = join(*r, high);
  cn::SequelClientEnd& l = join(*r, low);
  run(*r, 120);
  CHECK(r->server.clients[0].rate == 3000 && r->server.clients[1].rate == 2000);
  auto told = [](const cn::SequelClientEnd& c, const std::string& said, const std::string& line) {
    bool p = false, st = false;
    for (const std::string& t : c.prints) p = p || t.find(said) != std::string::npos;
    for (const std::string& t : c.stuffed) st = st || t == line;
    return p && st;
  };
  CHECK(told(h, "Your rate setting is too high for this server.", "rate 3000"));
  CHECK(told(l, "Your rate setting is too low for this server.", "rate 2000"));
  bool listed = false;
  const ge::server::ServerGame& g = cn::sequel_game();
  for (int i = 0; i < g.rule_count; i++)
    listed = listed || g.rules[i].name == std::string("sv_maxrate");
  CHECK(listed);
}

// Our server tells a joined window the feel it runs every move on (svc 122): with the classic
// game's ground and air (`sv_physics classic`: 320 a second, friction 4, no landing pause) the
// window predicts that move, so walking and jumping are never corrected. On a phone's link (the
// moves in flight a round trip of 90 ms) the stock move predicted instead is corrected by 2.9
// units.
TEST(the_window_predicts_on_the_feel_our_server_of_the_sequel_tells) {
  auto r = rig({}, "mobile");
  sv::sequel_of(r->rules)->classic_physics = true;
  gn::Transport t;
  gn::open_sim_transport(t, r->net, gn::sim_address(9, 27901));
  cn::Remote remote;
  remote.sequel_game = [] { return true; };
  remote.read_file = [](const std::string&, const std::string&) { return std::vector<uint8_t>{1}; };
  cn::ClientConfig cfg;
  cfg.name = "eve";
  cn::remote_start(remote, t, gn::sim_address(0, cn::kPortSequel), cfg);
  std::vector<std::string> said;
  float most_correction = 0;
  const auto steps = [&](int n) {
    for (int i = 0; i < n; i++) {
      r->now += 13000;
      gn::sim_advance(r->net, r->now);
      if (cn::remote_step(remote, 0.0131, said) == cn::RemoteStep::new_level)
        cn::remote_world(remote, nullptr, r->map.get());
      cn::sequel_net_frame(r->server, r->now);
      if (remote.sequel_ahead.predicted)
        most_correction =
            std::max(most_correction, sh::length(remote.sequel_ahead.smooth.correction));
    }
  };
  steps(200);
  CHECK_MSG(remote.sequel->stage == cn::ClientStage::active, remote.sequel->refused);
  CHECK(remote.demo.feel_told);
  CHECK(remote.demo.feel.max_speed == 320 && remote.demo.feel.friction == 4);
  CHECK(!remote.demo.feel.land_pause && !remote.demo.feel.air_step);
  const sh::Vec3f start = remote.sequel_ahead.origin;
  remote.hands.forward = 400;
  remote.hands.buttons = sh::kButtonJump;
  steps(150);
  remote.hands = {};
  steps(100);
  CHECK_MSG(most_correction == 0, std::to_string(most_correction) + " units corrected");
  const sh::Vec3f moved = remote.sequel_ahead.origin - start;
  CHECK_MSG(sh::length(moved) > 100, "walked " + std::to_string(sh::length(moved)));
}

// Voice on our server of the sequel, by the first game's rules (net/classic_voice_route.hpp): the
// server says it takes a client's voice, the client unmutes and talks, and the voice reaches the
// talker's team (the sequel's teams by skin), everyone once he targets all, nobody who sent
// `muteall`, and never a stock client, which is never told and never dropped for it.
namespace {
bool always_talks(int /*tick*/, int /*seed*/) { return true; }
int slot_named(const Rig& r, const char* name) {
  for (int i = 0; i < sh::kMaxClients; i++)
    if (cn::sequel_held(r.server.clients[i]) && r.server.clients[i].name == name) return i;
  return -1;
}
}  // namespace

TEST(voice_on_our_server_of_the_sequel_reaches_whom_the_first_games_rules_say) {
  cn::SequelNetConfig cfg;
  cfg.voice = true;  // sv_voip 1
  auto r = rig(cfg);
  r->rules.console("dmflags 64");  // teams by skin
  cn::SequelClientConfig a;
  a.name = "a";
  a.skin = "male/red";
  a.voice = true;
  a.talk = always_talks;
  cn::SequelClientConfig b;
  b.name = "b";
  b.skin = "female/red";
  b.voice = true;
  cn::SequelClientConfig c = b;
  c.name = "c";
  c.skin = "male/blue";
  cn::SequelClientConfig d = b;
  d.name = "d";
  d.skin = "male/red";
  d.voice = false;  // a client that never asks
  cn::SequelClientEnd& ca = join(*r, a);
  cn::SequelClientEnd& cb = join(*r, b);
  cn::SequelClientEnd& cc = join(*r, c);
  cn::SequelClientEnd& cd = join(*r, d);
  run(*r, 300);
  CHECK(ca.stage == cn::ClientStage::active && cd.stage == cn::ClientStage::active);
  CHECK(ca.voice_on && cb.voice_on && cc.voice_on && !cd.voice_on);
  const int sa = slot_named(*r, "a");
  CHECK(sa >= 0);
  if (sa < 0) return;
  CHECK_MSG(ca.voice_sent > 0, "the talker sent nothing");
  CHECK_MSG(cb.voice_heard[sa] > 0, "his teammate heard nothing");
  CHECK_MSG(cc.voice_heard[sa] == 0, "the other team heard his team talk");
  CHECK(cd.voice_heard[sa] == 0 && ca.voice_heard[sa] == 0);  // a stock client; never his own
  cn::sequel_voice_target(ca, "all");
  run(*r, 60);
  CHECK_MSG(cc.voice_heard[sa] > 0, "voicetarg all did not reach the other team");
  CHECK(cd.voice_heard[sa] == 0 && cd.stage == cn::ClientStage::active);
  cn::sequel_client_command(cc, "muteall");
  run(*r, 30);
  const int64_t muted = cc.voice_heard[sa];
  run(*r, 60);
  CHECK_MSG(cc.voice_heard[sa] == muted, "muteall still heard voice");
  CHECK(cb.voice_heard[sa] > 0);
  // A server with voice off (sv_voip 0, the default): never said, so nobody talks.
  auto off = rig({});
  cn::SequelClientEnd& oa = join(*off, a);
  run(*off, 200);
  CHECK(oa.stage == cn::ClientStage::active && !oa.voice_on && oa.voice_sent == 0);
}

// sv_landjump and sv_waterjump (both 1 by default) reach a client told the feel, whose prediction
// then hops with no landing pause and no correction; for a client that is not told, the server
// keeps the stock landing pause and surfacing lift, unless a setting is 2.
TEST(the_landing_and_surfacing_settings_reach_a_client_told_the_feel) {
  auto r = rig({}, "mobile");
  gn::Transport t;
  gn::open_sim_transport(t, r->net, gn::sim_address(9, 27901));
  cn::Remote remote;
  remote.sequel_game = [] { return true; };
  remote.read_file = [](const std::string&, const std::string&) { return std::vector<uint8_t>{1}; };
  cn::ClientConfig cfg;
  cfg.name = "eve";
  cn::remote_start(remote, t, gn::sim_address(0, cn::kPortSequel), cfg);
  std::vector<std::string> said;
  float most_correction = 0;
  const auto steps = [&](int n) {
    for (int i = 0; i < n; i++) {
      r->now += 13000;
      gn::sim_advance(r->net, r->now);
      if (cn::remote_step(remote, 0.0131, said) == cn::RemoteStep::new_level)
        cn::remote_world(remote, nullptr, r->map.get());
      cn::sequel_net_frame(r->server, r->now);
      if (remote.sequel_ahead.predicted)
        most_correction =
            std::max(most_correction, sh::length(remote.sequel_ahead.smooth.correction));
    }
  };
  steps(200);
  CHECK_MSG(remote.sequel->stage == cn::ClientStage::active, remote.sequel->refused);
  CHECK(remote.demo.feel_told);
  CHECK(!remote.demo.feel.land_pause && !remote.demo.feel.surface_lift);
  sv::SequelServer& game = *sv::sequel_of(r->rules);
  const auto told = sv::sequel_move_feel(game, 0);
  CHECK(!told.land_pause && !told.surface_lift);
  remote.hands.forward = 400;
  remote.hands.buttons = sh::kButtonJump;
  steps(200);
  remote.hands = {};
  steps(100);
  CHECK_MSG(most_correction == 0, std::to_string(most_correction) + " units corrected");
  // The same player as a client that never asked for the feel.
  auto& info = game.clients[0].userinfo;
  std::erase_if(info, [](const auto& kv) { return kv.first == "*ge_ext"; });
  const auto untold = sv::sequel_move_feel(game, 0);
  CHECK(untold.land_pause && untold.surface_lift);
  game.cvars.set("sv_landjump", "2", ge::console::Source::code);
  CHECK(!sv::sequel_move_feel(game, 0).land_pause);
}
