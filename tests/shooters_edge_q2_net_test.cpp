// games/shooters_edge/net/q2_net*.hpp, q2_client.hpp, q2_net_server.hpp: Quake II's
// live protocol. A move and its checksum come back as written; the scripted client joins the
// Quake II's net server on the simulated network through the whole signon, its frames are deltas,
// its recording walks whole, and a client that walks forward is seen to move; refusals read as a
// player's words; a lossy link still plays; a map with no sky gets the default one and a
// download is refused; a client fetches the files it lacks and ours keeps the packed ones in a
// partial of their pak until it is whole; the dedicated server game names itself and refuses a
// missing folder; the window joined to a server of Quake II walks, turns by the server's turn,
// sends a weapon key as `use` and leaves; each client is sent only the entities it may see.
//
// ge:summary Test: Quake II's live protocol, our client joined to our server on a simulated net
// ge:keywords test q2 protocol-34 live client server signon move checksum delta refusal loss
// ge:skill net shooter testing
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classic/content/downloads.hpp"
#include "console/cvar.hpp"
#include "forward/forward.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/listen_record.hpp"
#include "shooters_edge/net/q2_admin.hpp"
#include "shooters_edge/net/q2_client.hpp"
#include "shooters_edge/net/q2_game.hpp"
#include "shooters_edge/net/q2_net.hpp"
#include "shooters_edge/net/q2_net_server.hpp"
#include "shooters_edge/net/q2_remote.hpp"
#include "shooters_edge/net/qw_match_record.hpp"
#include "shooters_edge/net/qw_remote.hpp"
#include "shooters_edge/net/qw_wire.hpp"
#include "shooters_edge/server/q2_server.hpp"
#include "shooters_edge/server/q2_wire.hpp"
#include "shooters_edge_q2_rig.hpp"
#include "vfs/pak.hpp"
#include "vfs/partial.hpp"
#include "vfs/search.hpp"
#include "vfs/unpack.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace gn = ge::net;
namespace sv = shooters_edge::server;
namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;
namespace fw = ge::forward;
using q2_rig::box;
using q2_rig::entity;
using q2_rig::hall;
using q2_rig::join;
using q2_rig::Rig;
using q2_rig::rig;
using q2_rig::run;

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
bool holds_near(const cn::Q2ClientEnd& c, float x, float y) {
  for (const auto& e : c.demo.state.entities)
    if (e.present && e.model != 0 &&
        (e.origin.x - x) * (e.origin.x - x) + (e.origin.y - y) * (e.origin.y - y) < 16.0f * 16.0f)
      return true;
  return false;
}

sh::MoveCmd forward(int /*tick*/) {
  sh::MoveCmd m;
  m.forward = 300;
  return m;
}

}  // namespace

TEST(a_move_and_its_checksum_come_back_as_written) {
  cn::Q2Cmd cmds[3];
  cmds[1].forward = 400;
  cmds[1].angles[1] = 8192;
  cmds[2] = cmds[1];
  cmds[2].buttons = 1;
  cmds[2].msec = 16;
  uint8_t buf[64];
  gn::Writer w = gn::make_writer(buf, sizeof buf);
  cn::write_q2_move(w, 42, cmds, 1234);
  CHECK(!w.overflow);
  CHECK(buf[0] == cn::kClcMove);
  gn::Reader r = gn::make_reader(buf + 1, w.size - 1);
  cn::Q2Move m;
  CHECK(cn::read_q2_move(r, 1234, m));
  CHECK(m.checksum_ok);
  CHECK(m.frame == 42);
  for (int k = 0; k < 3; k++) CHECK(m.cmds[k] == cmds[k]);
  gn::Reader again = gn::make_reader(buf + 1, w.size - 1);
  CHECK(cn::read_q2_move(again, 1235, m));
  CHECK(!m.checksum_ok);  // another packet's number: another byte
}

TEST(the_checksum_is_the_byte_a_live_server_expected) {
  // Moves a live server of Quake II refused, with the byte it said it expected (the body after
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
    const uint8_t got = cn::q2_move_checksum(reinterpret_cast<const uint8_t*>(body.data()),
                                             body.size(), v.sequence);
    CHECK_MSG(got == v.expected, "sequence " + std::to_string(v.sequence) + ": " +
                                     std::to_string(got) + ", the server expected " +
                                     std::to_string(v.expected));
  }
  uint64_t sum = 0, weighted = 0;
  for (int i = 0; i < cn::kQ2ChecksumTableSize; i++) {
    sum += cn::kQ2ChecksumTable[i];
    weighted += uint64_t(i + 1) * cn::kQ2ChecksumTable[i];
  }
  CHECK(sum == 124780);
  CHECK(weighted == 59989697);
  for (int i = 960; i < cn::kQ2ChecksumTableSize; i++) CHECK(cn::kQ2ChecksumTable[i] == 0);
}

TEST(cl_nodelta_has_every_frame_sent_whole) {
  // A client that acknowledges no frame (`nodelta`, the window's cl_nodelta) is sent each one
  // whole, and still plays.
  auto r = rig({});
  cn::Q2ClientConfig cfg;
  cfg.name = "eve";
  cfg.nodelta = true;
  // cl_shownet 1: each message the server sent printed as its size.
  cfg.shownet = 1;
  ge::classic::protocol::ChannelTrace trace;
  int sizes = 0;
  bool numbers = true;
  trace.print = [&](const std::string& line) {
    sizes++;
    numbers = numbers && !line.empty() && line.find_first_not_of("0123456789") == std::string::npos;
  };
  cfg.trace = &trace;
  cn::Q2ClientEnd& c = join(*r, cfg);
  run(*r, 200);
  CHECK_MSG(sizes >= c.frames && numbers, std::to_string(sizes) + " sizes printed");
  CHECK_MSG(c.stage == cn::ClientStage::active, c.refused);
  CHECK(c.frames >= 10);
  const cn::Q2NetClient& sc = r->server.clients[0];
  CHECK_MSG(sc.full_frames >= c.frames - 1,
            std::to_string(sc.full_frames) + " whole of " + std::to_string(c.frames));
  // scr_showturtle's NODELTA: the frame read came whole.
  CHECK(c.demo.state.q2_trouble == shooters_edge::client::kQ2FrameNoDelta);
}

TEST(out_of_band_words_keep_a_quoted_word_whole) {
  const auto w = cn::q2_words("connect 34 7 99 \"\\name\\a b\\skin\\male/grunt\"\n");
  CHECK(w.size() == 5);
  CHECK(w[4] == "\\name\\a b\\skin\\male/grunt");
}

TEST(a_client_joins_through_the_signon_and_plays_on_deltas) {
  auto r = rig({});
  cn::Q2ClientConfig cfg;
  cfg.name = "eve";
  cfg.brain = forward;
  cn::Q2ClientEnd& c = join(*r, cfg);
  run(*r, 120);
  CHECK_MSG(c.stage == cn::ClientStage::active, c.refused);
  CHECK(c.signon_steps >= 3);  // configstrings, baselines, begin at least
  CHECK(c.levels == 1);
  CHECK(c.demo.state.map == "hall");
  const float x0 = c.demo.state.eye.x, y0 = c.demo.state.eye.y;
  run(*r, 200);
  CHECK(c.frames >= 20);
  const cn::Q2NetClient& sc = r->server.clients[0];
  CHECK(sc.moves > 100);
  CHECK(sc.bad_checksums == 0);
  CHECK_MSG(sc.full_frames <= 2, std::to_string(sc.full_frames) + " whole frames");
  const float dx = c.demo.state.eye.x - x0, dy = c.demo.state.eye.y - y0;
  CHECK_MSG(dx * dx + dy * dy > 100.0f * 100.0f,
            "the player walked " + std::to_string(dx) + "," + std::to_string(dy));
  cn::q2_client_stop(c);
  run(*r, 10);
  CHECK(cn::q2_net_clients(r->server) == 0);
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

// Protocols 35 and 36 (net/q2_proto_in.hpp): each joins, walks on deltas and keeps a recording
// of protocol 34 that reads whole; 36 on either channel, its moves batched or one a packet.
TEST(a_client_of_each_later_protocol_joins_and_plays) {
  struct Case {
    int protocol, chantype;
    bool batch;
  };
  for (const Case k : {Case{35, 0, false}, Case{36, 1, true}, Case{36, 0, false},
                       Case{36, 1, false}, Case{0, 1, true}}) {
    const std::string name =
        std::to_string(k.protocol) + "/" + std::to_string(k.chantype) + (k.batch ? " batched" : "");
    auto r = rig({});
    cn::Q2ClientConfig cfg;
    cfg.name = "eve";
    cfg.brain = forward;
    cfg.protocol = k.protocol;
    cfg.chantype = k.chantype;
    cfg.batch = k.batch;
    cn::Q2ClientEnd& c = join(*r, cfg);
    run(*r, 120);
    CHECK_MSG(c.stage == cn::ClientStage::active, name + ": " + c.refused);
    CHECK_MSG(c.protocol == (k.protocol == 0 ? 36 : k.protocol), name);
    CHECK(c.demo.state.map == "hall");
    const float x0 = c.demo.state.eye.x, y0 = c.demo.state.eye.y;
    run(*r, 200);
    CHECK_MSG(c.frames >= 20, name);
    const cn::Q2NetClient& sc = r->server.clients[0];
    CHECK_MSG(sc.moves > 30, name + ": " + std::to_string(sc.moves) + " moves");
    CHECK_MSG(sc.full_frames <= 3, name + ": " + std::to_string(sc.full_frames) + " whole frames");
    const float dx = c.demo.state.eye.x - x0, dy = c.demo.state.eye.y - y0;
    CHECK_MSG(dx * dx + dy * dy > 100.0f * 100.0f, name + ": walked " + std::to_string(dx));
    cn::q2_client_stop(c);
    run(*r, 10);
    cl::Demo d;
    std::string error;
    CHECK_MSG(cl::open_demo(c.demo.bytes, d, error), name + ": " += error);
    CHECK(d.state.protocol == 34 || d.state.protocol == 0);
    int frames = 0;
    cl::DemoRead got;
    while ((got = cl::next_frame(d, error)) == cl::DemoRead::frame) frames++;
    CHECK_MSG(got == cl::DemoRead::end, name + ": " += error);
    CHECK_MSG(frames == c.frames, name);
  }
}

TEST(a_map_with_no_sky_gets_the_default_and_a_download_is_refused) {
  // A client that finds no sky asks for one to download; it is refused and plays on.
  cn::Q2NetConfig cfg;
  sv::q2_sky(*hall(), cfg.wire);
  CHECK(cfg.wire.sky == "unit1_");
  CHECK(cfg.wire.sky_rotate == "0.000000");
  auto r = rig(cfg);
  cn::Q2ClientConfig cc;
  cc.brain = forward;
  cn::Q2ClientEnd& c = join(*r, cc);
  run(*r, 120);
  CHECK_MSG(c.stage == cn::ClientStage::active, c.refused);
  cn::q2_client_command(c, "download env/rt.tga");
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

// Which files are offered: each kind by its own setting (textures/ and env/, pics/, anything else
// off by default), none past sv_max_download_size, a resume past the end told the size differs;
// the connect answer names the web address and the map.
TEST(downloads_are_offered_by_kind_and_size_and_a_bad_resume_is_told) {
  cn::Q2NetConfig cfg;
  cfg.allow_pics = false;
  cfg.max_download_size = 50;
  cfg.download_server = "http://files.example/q2/";
  cfg.read_file = [](const std::string& path, std::vector<uint8_t>* out) {
    out->assign(path.rfind("models/big", 0) == 0 ? 100 : 10, uint8_t{7});
    return true;
  };
  auto r = rig(cfg);
  cn::Q2ClientConfig cc;
  cn::Q2ClientEnd& c = join(*r, cc);
  run(*r, 120);
  CHECK_MSG(c.stage == cn::ClientStage::active, c.refused);
  const auto why = [&](const std::string& asked) -> std::string {
    const size_t before = r->server.log.size();
    cn::q2_client_command(c, "download " + asked);
    run(*r, 10);
    for (size_t i = before; i < r->server.log.size(); i++) {
      const std::string& l = r->server.log[i];
      if (const size_t at = l.find(": refused, "); at != std::string::npos)
        return l.substr(at + 11);
    }
    return "sent";
  };
  CHECK(why("env/unit1_up.tga") == "sent");
  CHECK(why("sprites/s_bfg1.sp2") == "sent");
  CHECK(why("pics/colormap.pcx") == "that kind of file is not offered");
  CHECK(why("docs/readme.txt") == "that kind of file is not offered");
  CHECK(why("models/big/tris.md2") == "larger than sv_max_download_size");
  CHECK(why("/models/./a/../b/tris.md2") == "sent");  // made plain first
  CHECK(why("../models/x.md2") == "not a file the server offers");
  CHECK(why("models/x.md2.") == "not a file the server offers");
  CHECK(why("models/x.md2 20") == "a resume past its end");
  CHECK(std::find(c.prints.begin(), c.prints.end(),
                  "File size differs from server.\nPlease delete the corresponding .tmp file "
                  "from your system.\n") != c.prints.end());
}

TEST(a_level_change_keeps_every_connection_and_seats_each_client_again) {
  auto r = rig({});
  cn::Q2ClientConfig cfg;
  cfg.brain = forward;
  cfg.name = "eve";
  cn::Q2ClientEnd& a = join(*r, cfg);
  cfg.name = "bob";
  cn::Q2ClientEnd& b = join(*r, cfg);
  run(*r, 160);
  CHECK_MSG(a.stage == cn::ClientStage::active, a.refused);
  CHECK_MSG(b.stage == cn::ClientStage::active, b.refused);
  const int32_t count = r->server.count;
  // What the dedicated game does when the rules ask for a map: start them on it and tell the
  // clients (q2_game.cpp).
  sv::MapSpec next;
  next.name = "hall2";
  next.brushes = hall();
  std::string error;
  r->rules.carry();
  CHECK_MSG(r->rules.start(next, error), error);
  sv::Q2WireOptions wire = r->server.cfg.wire;
  wire.map_checksum = 1234;
  cn::q2_net_level(r->server, wire);
  CHECK(r->server.count == count + 1);
  // The server data carries the new count: a client keeps its loading picture up until it does.
  CHECK(r->server.cfg.wire.server_count == count + 1 - 0x10000);
  run(*r, 1);
  cn::q2_client_stop(b);  // gone while it signs on again: its slot in the rules is freed
  const auto frames = a.frames;
  run(*r, 200);
  CHECK_MSG(a.error.empty(), a.error);
  CHECK(a.stage == cn::ClientStage::active);
  CHECK(a.levels == 2);
  CHECK(a.demo.state.map == "hall2");
  CHECK(a.frames > frames + 10);
  CHECK(cn::q2_net_clients(r->server) == 1);
  CHECK(r->server.clients[0].stage == cn::Q2Slot::active);
  CHECK((*r->rules).clients[0].active);
  CHECK(!(*r->rules).clients[1].active);
  bool noted = false;
  for (const std::string& line : r->server.log)
    noted = noted || line.find("the level changed to hall2") != std::string::npos;
  CHECK(noted);
}

TEST(q2_server_game_names_itself_and_refuses_a_missing_folder) {
  const ge::server::ServerGame& g = cn::q2_game();
  CHECK(g.default_port == cn::kPortQ2);
  CHECK(g.binary == std::string("sesv"));
  ge::server::ServerConfig c;
  c.root = "no-such-folder-here";
  std::string error;
  CHECK(g.create(c, error) == nullptr);
  CHECK_MSG(!error.empty(), "a refusal says why");
}

TEST(sv_idlekick_is_the_idle_drop_not_the_lobbys_unready_kick) {
  ge::console::Cvars cvars;
  ge::server::ServerConfig c;
  ge::server::add_server_cvars(cvars, c, cn::q2_game());
  CHECK(cvars.set("sv_idlekick", "30", ge::console::Source::code) ==
        ge::console::SetResult::changed);
  CHECK(c.rules["sv_idletime"] == "30");
  CHECK(c.idle_kick_s == 0);  // the core's setting of that name is set aside
  const ge::console::Cvar* v = cvars.find("sv_idlekick");
  CHECK(v != nullptr && v->name == "sv_idletime");
}

// A server that insists on anticheat (` ac=2` in its connect answer): this game has none, so the
// player is told so and the game is joined without it; ` ac=1` (wanted, not needed) says nothing.
TEST(a_server_insisting_on_anticheat_is_told_and_joined_without_it) {
  for (int ask : {1, 2}) {
    gn::SimNetwork net = gn::make_sim_network(3, {});
    gn::Transport server, client;
    gn::open_sim_transport(server, net, gn::sim_address(0, cn::kPortQ2));
    gn::open_sim_transport(client, net, gn::sim_address(1, 27901));
    cn::Q2ClientEnd c;
    cn::Q2ClientConfig cfg;
    cfg.qport = 7;
    cn::q2_client_start(c, client, gn::sim_address(0, cn::kPortQ2), cfg, 0);
    int64_t now = 0;
    bool answered = false;
    for (int i = 0; i < 40 && !answered; i++) {
      now += 13000;
      gn::sim_advance(net, now);
      cn::q2_client_tick(c, now);
      uint8_t buf[gn::kMaxDatagram];
      gn::Address from;
      for (int n; (n = gn::transport_receive(server, buf, sizeof buf, from, now)) > 0;) {
        const std::vector<std::string> w =
            cn::q2_words(cn::q2_oob_text(buf, static_cast<size_t>(n)));
        std::string reply;
        if (!w.empty() && w[0] == "getchallenge") reply = "challenge 4242 p=34";
        if (!w.empty() && w[0] == "connect") {
          reply = "client_connect ac=" + std::to_string(ask);
          answered = true;
        }
        if (reply.empty()) continue;
        const std::vector<uint8_t> b = cn::q2_oob(reply);
        gn::transport_send(server, from, b.data(), b.size(), now);
      }
    }
    for (int i = 0; i < 4; i++) {
      now += 13000;
      gn::sim_advance(net, now);
      cn::q2_client_tick(c, now);
    }
    CHECK(answered && c.refused.empty());
    bool told = false;
    for (const std::string& line : c.prints)
      told = told || line.find("asks for anticheat") != std::string::npos;
    CHECK_MSG(told == (ask == 2), "ac=" + std::to_string(ask));
  }
}

TEST(refusals_read_as_a_players_words) {
  cn::Q2NetConfig cfg;
  cfg.password = "secret";
  cfg.max_clients = 1;
  auto r = rig(cfg);
  cn::Q2ClientConfig wrong;
  wrong.name = "mallory";
  cn::Q2ClientEnd& a = join(*r, wrong);
  run(*r, 40);
  CHECK(a.stage == cn::ClientStage::dropped);
  CHECK(a.refused == "Password required or incorrect.\nConnection refused.\n");
  cn::Q2ClientConfig right;
  right.password = "secret";
  cn::Q2ClientEnd& b = join(*r, right);
  run(*r, 80);
  CHECK_MSG(b.stage == cn::ClientStage::active, b.refused);
  cn::Q2ClientEnd& full = join(*r, right);
  run(*r, 40);
  CHECK(full.stage == cn::ClientStage::dropped);
  CHECK(full.refused == "Server is full.\n");
}

// sv_iplimit (3 by default here): a fourth client from one host is refused, another host's taken.
TEST(a_fourth_client_from_one_address_is_refused) {
  cn::Q2NetConfig cfg;
  cfg.max_clients = 8;
  auto r = rig(cfg);
  for (int i = 0; i < 3; i++) join(*r, {}, 50);
  run(*r, 80);
  for (int i = 0; i < 3; i++)
    CHECK_MSG(r->clients[size_t(i)]->stage == cn::ClientStage::active,
              r->clients[size_t(i)]->refused);
  cn::Q2ClientEnd& fourth = join(*r, {}, 50);
  cn::Q2ClientEnd& other = join(*r, {});
  run(*r, 80);
  CHECK(fourth.stage == cn::ClientStage::dropped);
  CHECK(fourth.refused == "Too many connections from your IP address.\n");
  CHECK_MSG(other.stage == cn::ClientStage::active, other.refused);
}

// sv_redirect: a newcomer to a full server is told so and sent on to another, where he plays; with
// a leading "!" every newcomer is sent on, full or not.
TEST(a_full_server_sends_a_newcomer_on_to_its_redirect) {
  for (const bool everyone : {false, true}) {
    cn::Q2NetConfig cfg;
    cfg.max_clients = 1;
    auto r = rig(cfg);
    // The second server, on the same network.
    sv::RulesRunner rules = sv::q2_runner(sv::make_q2_server({}));
    rules.console("deathmatch 1");
    sv::MapSpec map;
    map.name = "hall";
    map.brushes = r->map;
    std::string error;
    CHECK_MSG(rules.start(map, error), error);
    gn::Transport bt;
    const gn::Address b_at = gn::sim_address(0, uint16_t(cn::kPortQ2 + 1));
    gn::open_sim_transport(bt, r->net, b_at);
    cn::Q2NetServer b;
    CHECK_MSG(cn::q2_net_start(b, rules, bt, {}, error), error);
    r->server.cfg.redirect = (everyone ? "!" : "") + gn::to_string(b_at);
    cn::Q2ClientEnd& first = join(*r, {});
    for (int i = 0; i < 80; i++) {
      run(*r, 1);
      cn::q2_net_frame(b, r->now);
    }
    cn::Q2ClientEnd& second = join(*r, {});
    for (int i = 0; i < 160; i++) {
      run(*r, 1);
      cn::q2_net_frame(b, r->now);
    }
    CHECK_MSG(second.stage == cn::ClientStage::active, second.refused);
    CHECK(second.server == b_at && second.redirects == 1);
    CHECK(r->server.redirected == (everyone ? 2 : 1));
    CHECK(q2_net_clients(b) == (everyone ? 2 : 1));
    CHECK(first.stage == cn::ClientStage::active);
    CHECK((first.server == b_at) == everyone);
  }
}

// sv_vipslots 1 of two: the second player needs the VIP password (asked no other) or the server's.
TEST(the_last_slot_is_kept_for_a_vip_or_the_servers_password) {
  for (const char* given : {"door", "gold", "wrong"}) {
    cn::Q2NetConfig cfg;
    cfg.max_clients = 2;
    cfg.vip_slots = 1;
    cfg.vip_passwords = {"gold"};
    cfg.password = "door";
    auto r = rig(cfg);
    cn::Q2ClientConfig first;
    first.password = "door";
    first.name = "first";
    cn::Q2ClientEnd& a = join(*r, first);
    run(*r, 80);
    CHECK_MSG(a.stage == cn::ClientStage::active, a.refused);
    cn::Q2ClientConfig second;
    second.password = given;
    second.name = "second";
    cn::Q2ClientEnd& b = join(*r, second);
    run(*r, 80);
    if (std::string(given) == "wrong") {
      CHECK(b.refused == "Password required or incorrect.\nConnection refused.\n");
    } else {
      CHECK_MSG(b.stage == cn::ClientStage::active, b.refused);
    }
  }
  cn::Q2NetConfig cfg;  // without the server's password, only the VIP password passes
  cfg.max_clients = 2;
  cfg.vip_slots = 1;
  cfg.vip_passwords = {"gold"};
  auto r = rig(cfg);
  join(*r, {});
  run(*r, 80);
  cn::Q2ClientEnd& b = join(*r, {});
  run(*r, 80);
  CHECK(b.stage == cn::ClientStage::dropped);
  CHECK(b.refused == "Server and reserved slots are full.\n");
}

// sv_idletime 2: a player who presses nothing is dropped and told so; one who keeps moving stays.
TEST(an_idle_player_is_dropped) {
  cn::Q2NetConfig cfg;
  cfg.idle_time = 2;
  auto r = rig(cfg);
  cn::Q2ClientConfig idle;
  idle.name = "idler";
  cn::Q2ClientEnd& a = join(*r, idle);
  cn::Q2ClientConfig busy;
  busy.name = "busy";
  busy.brain = [](int tick) {  // a step one way, then the other, every half second
    sh::MoveCmd m = forward(tick);
    if ((tick / 40) % 2 == 1) m.forward = static_cast<int16_t>(-m.forward);
    return m;
  };
  cn::Q2ClientEnd& b = join(*r, busy);
  run(*r, 400);  // 5.2 s
  CHECK(a.stage == cn::ClientStage::dropped);
  CHECK_MSG(b.stage == cn::ClientStage::active, b.refused);
  bool told = false;
  for (const std::string& l : r->server.log) told = told || l == "idler dropped: idling";
  CHECK(told);
}

// sv_locked, set by the admin while the server runs: a newcomer is refused until it is unlocked.
TEST(a_locked_server_refuses_newcomers) {
  auto r = rig({});
  cn::Q2ClientEnd& a = join(*r, {});
  run(*r, 80);
  CHECK_MSG(a.stage == cn::ClientStage::active, a.refused);
  std::string out;
  CHECK(cn::q2_admin(r->server, "sv_locked 1", out) && r->server.cfg.locked);
  cn::Q2ClientEnd& b = join(*r, {});
  run(*r, 80);
  CHECK(b.refused == "Server is locked.\n");
  CHECK(cn::q2_admin(r->server, "sv_locked 0", out) && out == "Server unlocked.\n");
  cn::Q2ClientEnd& c = join(*r, {});
  run(*r, 80);
  CHECK_MSG(c.stage == cn::ClientStage::active, c.refused);
}

// sv_ruleset: a player on another ruleset is refused, one on it and a spectator come in, and the
// status record names it.
TEST(a_competition_server_refuses_a_player_on_another_ruleset) {
  cn::Q2NetConfig cfg;
  cfg.ruleset = "duel";
  auto r = rig(cfg);
  cn::Q2ClientConfig plain;
  cn::Q2ClientEnd& a = join(*r, plain);
  cn::Q2ClientConfig dueller;
  dueller.ruleset = "duel";
  cn::Q2ClientEnd& b = join(*r, dueller);
  cn::Q2ClientConfig watcher;
  watcher.spectator = true;
  cn::Q2ClientEnd& c = join(*r, watcher);
  run(*r, 80);
  CHECK(a.refused ==
        "This server requires the duel ruleset. Pick it in OPTIONS or type ruleset duel, then "
        "join again.\n");
  CHECK_MSG(b.stage == cn::ClientStage::active, b.refused);
  CHECK_MSG(c.refused.empty(), c.refused);
  CHECK(cn::q2_status_text(r->server).find("\\ruleset\\duel") != std::string::npos);
}

TEST(renames_past_the_limit_are_refused_and_the_rest_told) {
  cn::Q2NetConfig cfg;
  cfg.names = {2, 60, 1};  // two renames a minute, everyone told
  auto r = rig(cfg);
  cn::Q2ClientConfig a_cfg, b_cfg;
  a_cfg.name = "renamer";
  b_cfg.name = "watcher";
  cn::Q2ClientEnd& a = join(*r, a_cfg);
  cn::Q2ClientEnd& b = join(*r, b_cfg);
  run(*r, 120);
  CHECK_MSG(a.stage == cn::ClientStage::active, a.refused);
  for (const char* name : {"one", "two", "three"}) {  // the user info as the client sends it
    std::string info;
    cn::info_set(info, "name", name);
    std::vector<uint8_t> m = {cn::kClcUserInfo};
    m.insert(m.end(), info.begin(), info.end());
    m.push_back(0);
    cn::channel_reliable(a.ch, m.data(), m.size());
    run(*r, 20);
  }
  auto heard = [](const cn::Q2ClientEnd& c, const std::string& line) {
    for (const std::string& l : c.prints)
      if (l == line) return true;
    return false;
  };
  CHECK(heard(b, "renamer changed name to one.\n"));
  CHECK(heard(b, "one changed name to two.\n"));
  CHECK(!heard(b, "two changed name to three.\n"));
  CHECK(heard(a, "You can't change your name too often.\n"));
  CHECK(r->server.clients[0].name == "two");
}

TEST(a_lossy_link_still_plays) {
  auto r = rig({}, "bad");
  cn::Q2ClientConfig cfg;
  cfg.brain = forward;
  cn::Q2ClientEnd& c = join(*r, cfg);
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
  fc.allow = {gn::sim_address(0, cn::kPortQ2)};
  fc.seed = 2;
  fw::forward_init(fb, fc, b_port, opener(21));
  cn::Q2ClientConfig cfg;
  cfg.brain = forward;
  cfg.via = {a, b};
  cn::Q2ClientEnd& c = join(*r, cfg);
  for (int i = 0; i < 300; i++) {
    r->now += 13000;
    gn::sim_advance(r->net, r->now);
    fw::forward_update(fa, r->now);
    fw::forward_update(fb, r->now);
    cn::q2_client_tick(c, r->now);
    cn::q2_net_frame(r->server, r->now);
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

TEST(the_window_joins_a_server_of_q2_and_plays) {
  auto r = rig({});
  gn::Transport t;
  gn::open_sim_transport(t, r->net, gn::sim_address(9, 27901));
  cn::Remote remote;
  remote.q2_game = [] { return true; };
  remote.read_file = [](const std::string&, const std::string&) {  // it has every file
    return std::vector<uint8_t>{1};
  };
  cn::ClientConfig cfg;
  cfg.name = "eve";
  cn::remote_start(remote, t, gn::sim_address(0, cn::kPortQ2), cfg);
  CHECK(remote.q2 != nullptr);
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
      cn::q2_net_frame(r->server, r->now);
    }
  };
  steps(150);
  CHECK_MSG(remote.q2->stage == cn::ClientStage::active, remote.q2->refused);
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
  CHECK(heard_line("Out of item: Shotgun\n"));
  CHECK(heard_line("Out of item: Railgun\n"));
  // Leaving tells the server.
  cn::remote_disconnect(remote);
  CHECK(!remote.active && remote.q2 == nullptr);
  steps(10);
  CHECK(cn::q2_net_clients(r->server) == 0);
}

TEST(a_q2_recording_s_size_is_said_in_decimal_units) {
  CHECK(cl::q2_demo_size(1) == "1 byte");
  CHECK(cl::q2_demo_size(999) == "999 bytes");
  CHECK(cl::q2_demo_size(12345) == "12 kB");
  CHECK(cl::q2_demo_size(1250000) == "1.2 MB");
  CHECK(cl::q2_demo_size(45600000) == "45 MB");
  CHECK(cl::q2_demo_size(2500000000) == "2.5 GB");
}

TEST(a_recording_of_a_q2_server_joined_late_plays_from_its_server_data) {
  // Recording begun well into the level still opens with the level's server data (the client
  // keeps the level from its start), and the file ends with Quake II's end block (length -1).
  auto r = rig({});
  gn::Transport t;
  gn::open_sim_transport(t, r->net, gn::sim_address(9, 27902));
  cn::Remote remote;
  remote.q2_game = [] { return true; };
  remote.read_file = [](const std::string&, const std::string&) { return std::vector<uint8_t>{1}; };
  cn::ClientConfig cfg;
  cfg.name = "eve";
  cn::remote_start(remote, t, gn::sim_address(0, cn::kPortQ2), cfg);
  std::vector<std::string> said;
  const auto steps = [&](int n) {
    for (int i = 0; i < n; i++) {
      r->now += 13000;
      gn::sim_advance(r->net, r->now);
      cn::remote_step(remote, 0.0131, said);
      cn::q2_net_frame(r->server, r->now);
    }
  };
  steps(150);
  CHECK_MSG(remote.q2->stage == cn::ClientStage::active, remote.q2->refused);
  const std::filesystem::path file =
      std::filesystem::temp_directory_path() / "ge_q2_joined_record.dm2";
  std::string error;
  CHECK_MSG(cn::remote_record_open(remote, file, error), error);
  steps(60);
  ge::console::Console con{false};
  const std::string line = cn::remote_record_close(remote, con, 0);
  CHECK_MSG(
      line.starts_with("Stopped demo (") && line.ends_with(").") && !line.ends_with("(0 bytes)."),
      line);
  // The same through gzip (`record -z`): the file a gzip stream of the same kind of recording.
  const std::filesystem::path packed = file.string() + ".gz";
  CHECK_MSG(cn::remote_record_open(remote, packed, error), error);
  steps(30);
  CHECK(cn::remote_record_close(remote, con, 0).starts_with("Stopped demo ("));
  {
    std::ifstream zin(packed, std::ios::binary);
    std::vector<uint8_t> zbytes((std::istreambuf_iterator<char>(zin)), {});
    zin.close();
    std::filesystem::remove(packed);
    CHECK(ge::vfs::wrapping_of(zbytes) == ge::vfs::Wrapping::gzip);
    ge::vfs::Unpacked plain;
    CHECK_MSG(ge::vfs::unpack_one(zbytes, plain, error), error);
    CHECK(plain.bytes.size() > 8 && plain.bytes.back() == 0xff);
  }
  std::ifstream in(file, std::ios::binary);
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
  in.close();
  std::filesystem::remove(file);
  CHECK(bytes.size() > 8);
  CHECK(bytes.size() > 4 && bytes[bytes.size() - 1] == 0xff && bytes[bytes.size() - 4] == 0xff);
  cl::Demo d;
  CHECK_MSG(cl::open_demo(bytes, d, error), error);
  int frames = 0;
  for (cl::DemoRead got = cl::DemoRead::frame; got == cl::DemoRead::frame && frames < 1000;) {
    got = cl::next_frame(d, error);
    frames += got == cl::DemoRead::frame ? 1 : 0;
    CHECK_MSG(got != cl::DemoRead::error, error);
  }
  CHECK(d.state.map == "hall");
  CHECK_MSG(frames > 20, std::to_string(frames));
  cn::remote_disconnect(remote);
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
  cn::Q2NetConfig cfg;
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
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "ge_q2_fetch";
  std::filesystem::remove_all(dir);
  int told = 0, untold = 0;
  cn::Q2ClientConfig ours;
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
  cn::Q2ClientConfig stock = ours;
  stock.name = "stock";
  stock.ask_origins = false;
  int stock_told = 0;
  stock.keep_file = [&stock_told](const std::string&, const std::vector<uint8_t>&,
                                  const ge::vfs::ArchiveOrigin* origin) {
    stock_told += origin != nullptr ? 1 : 0;
  };
  // allow_download_players 0: the map alone is fetched, the skins left lacking.
  cn::Q2ClientConfig picky = ours;
  picky.name = "picky";
  picky.allow.players = false;
  picky.keep_file = nullptr;
  cn::Q2ClientEnd& a = join(*r, ours);
  cn::Q2ClientEnd& b = join(*r, stock);
  cn::Q2ClientEnd& p = join(*r, picky);
  run(*r, 300);
  CHECK_MSG(p.stage == cn::ClientStage::active, p.refused);
  CHECK(p.downloaded.size() == 1 && p.downloaded.count("maps/hall.bsp") == 1);
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

TEST(a_download_cut_short_is_resumed_from_what_is_held) {
  // A skin's first 1000 bytes held from before (as `<path>.tmp`): the client asks for the rest
  // from there, so it ends as the held bytes then the server's from 1000 on, and the held file
  // goes once it is whole. A client holding nothing gets every piece added as it comes (two of
  // them: pieces fill the packet), the first one begun afresh.
  const std::vector<uint8_t> red(2500, 7);
  cn::Q2NetConfig cfg;
  cfg.read_file = [&red](const std::string& path, std::vector<uint8_t>* out) {
    if (path != "players/male/red.pcx") return false;
    *out = red;
    return true;
  };
  auto r = rig(cfg);
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "ge_q2_resume";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir / "players/male");
  {
    std::ofstream held(dir / "players/male/red.pcx.tmp", std::ios::binary);
    const std::vector<char> first(1000, 5);
    held.write(first.data(), std::streamsize(first.size()));
  }
  cn::Q2ClientConfig resumed;
  resumed.name = "resumed";
  resumed.fetch = true;
  resumed.want = {"players/male/red.pcx"};
  resumed.read_file = [](const std::string& path) {
    return path.starts_with("maps/") ? std::vector<uint8_t>(1, 1) : std::vector<uint8_t>{};
  };
  resumed.held = ge::classic::content::held_in_folder([dir] { return dir.string(); });
  cn::Q2ClientConfig fresh = resumed;
  fresh.name = "fresh";
  int adds = 0, fresh_adds = 0;
  size_t added = 0;
  fresh.held.read = nullptr;
  fresh.held.add = [&](const std::string&, std::span<const uint8_t> bytes, bool first) {
    adds++;
    fresh_adds += first ? 1 : 0;
    added += bytes.size();
  };
  fresh.held.drop = nullptr;
  cn::Q2ClientEnd& a = join(*r, resumed);
  cn::Q2ClientEnd& b = join(*r, fresh);
  run(*r, 300);
  CHECK_MSG(a.stage == cn::ClientStage::active, a.refused);
  std::vector<uint8_t> want(1000, 5);
  want.insert(want.end(), red.begin() + 1000, red.end());
  CHECK(a.downloaded["players/male/red.pcx"] == want);
  CHECK(!std::filesystem::exists(dir / "players/male/red.pcx.tmp"));
  CHECK_MSG(b.stage == cn::ClientStage::active, b.refused);
  CHECK(b.downloaded["players/male/red.pcx"] == red);
  CHECK_MSG(adds == 2 && fresh_adds == 1 && added == red.size(),
            std::to_string(adds) + " adds, " + std::to_string(fresh_adds) + " fresh");
  std::filesystem::remove_all(dir);
}

TEST(each_client_is_sent_only_what_it_may_see) {
  for (const bool cut : {true, false}) {
    cn::Q2NetConfig cfg;
    cfg.cut_to_view = cut;
    auto r = rig(cfg, "lan", halls());
    cn::Q2ClientConfig cc;
    cc.name = "eve";
    cn::Q2ClientEnd& a = join(*r, cc);
    cc.name = "bob";
    cn::Q2ClientEnd& b = join(*r, cc);
    run(*r, 200);
    for (const cn::Q2ClientEnd* c : {&a, &b}) {
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
    for (const cn::Q2NetClient& sc : r->server.clients) kept_out += sc.wire.cut;
    CHECK_MSG((kept_out > 0) == cut, std::to_string(kept_out) + " entities kept out");
  }
}

TEST(the_window_predicts_its_moves_on_a_server_of_the_q2) {
  auto r = rig({}, "broadband");
  gn::Transport t;
  gn::open_sim_transport(t, r->net, gn::sim_address(9, 27901));
  cn::Remote remote;
  remote.q2_game = [] { return true; };
  remote.read_file = [](const std::string&, const std::string&) { return std::vector<uint8_t>{1}; };
  cn::ClientConfig cfg;
  cfg.name = "eve";
  cn::remote_start(remote, t, gn::sim_address(0, cn::kPortQ2), cfg);
  std::vector<std::string> said;
  float most_correction = 0, highest = -1e9f;
  int predicted = 0, most_replayed = 0;
  const auto steps = [&](int n) {
    for (int i = 0; i < n; i++) {
      r->now += 13000;
      gn::sim_advance(r->net, r->now);
      if (cn::remote_step(remote, 0.0131, said) == cn::RemoteStep::new_level)
        cn::remote_world(remote, nullptr, r->map.get());  // as the window does, the level loaded
      cn::q2_net_frame(r->server, r->now);
      const cn::Q2Predict& p = remote.q2_ahead;
      if (!p.predicted) continue;
      predicted++;
      most_replayed = std::max(most_replayed, p.replayed);
      most_correction = std::max(most_correction, sh::length(p.smooth.correction));
      highest = std::max(highest, p.origin.z);
    }
  };
  steps(200);
  CHECK_MSG(remote.q2->stage == cn::ClientStage::active, remote.q2->refused);
  CHECK(predicted > 0);
  // Walking and jumping: the drawn place leads the server's by the moves in flight, and the
  // server's frames prove every prediction right (the same move, bit for bit): no correction.
  const sh::Vec3f start = remote.q2_ahead.origin;
  remote.hands.forward = 300;
  remote.hands.buttons = sh::kButtonJump;
  steps(150);
  remote.hands = {};
  steps(100);
  CHECK_MSG(most_replayed >= 2, std::to_string(most_replayed) + " moves ahead at most");
  CHECK_MSG(most_correction == 0, std::to_string(most_correction) + " units corrected");
  const sh::Vec3f moved = remote.q2_ahead.origin - start;
  CHECK_MSG(sh::length(moved) > 100, "walked " + std::to_string(sh::length(moved)));
  CHECK_MSG(highest > start.z + 20, "rose " + std::to_string(highest - start.z));  // it jumped
  // Standing still, the prediction is the server's place exactly.
  const cl::DemoState& st = remote.demo.state;
  const cl::Q2View& v = remote.demo.views[static_cast<size_t>(remote.demo.last_frame & 15)];
  CHECK(remote.q2_ahead.origin.x == static_cast<float>(v.origin[0]) * 0.125f);
  CHECK(remote.q2_ahead.origin.y == static_cast<float>(v.origin[1]) * 0.125f);
  CHECK(remote.q2_ahead.origin.z == static_cast<float>(v.origin[2]) * 0.125f);
  CHECK(st.eye.z > remote.q2_ahead.origin.z);
  // cl_showmiss: none so far; a base knocked 10 units aside is a miss the next frame shows and the
  // window prints, once.
  CHECK(remote.q2_ahead.misses == 0);
  std::vector<std::string> printed;
  remote.trace.print = [&printed](const std::string& line) { printed.push_back(line); };
  remote.q2_ahead.show_misses = true;
  remote.q2_ahead.base.origin[0] = static_cast<int16_t>(remote.q2_ahead.base.origin[0] + 80);
  steps(20);
  CHECK_MSG(remote.q2_ahead.misses == 1, std::to_string(remote.q2_ahead.misses) + " misses");
  CHECK_MSG(printed.size() == 1 && printed[0].rfind("prediction miss on ", 0) == 0 &&
                printed[0].substr(printed[0].size() - 4) == ": 10",
            printed.empty() ? "nothing printed" : printed[0]);
  remote.trace.print = nullptr;
  // cl_predict_crouch 2: a duck lowers the eye the frame it goes down, ahead of the server's
  // frames, which catch up; 0: the eye stays the server's.
  const auto crouch = [&](int mode) {
    remote.q2_ahead.crouch = mode;
    remote.hands.up = -400;
    float lowest = 0;
    for (int i = 0; i < 40; i++) {
      steps(1);
      lowest = std::min(lowest, remote.q2_ahead.eye_shift);
    }
    const float last = remote.q2_ahead.eye_shift;
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
  CHECK(!remote.q2_ahead.predicted && !remote.predicted);
}

TEST(the_window_runs_the_other_players_forward_on_our_server_of_the_q2) {
  auto r = rig({}, "broadband");
  bool go = false;
  cn::Q2ClientConfig bob;
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
  remote.q2_game = [] { return true; };
  remote.read_file = [](const std::string&, const std::string&) { return std::vector<uint8_t>{1}; };
  cn::ClientConfig cfg;
  cfg.name = "eve";
  cn::remote_start(remote, t, gn::sim_address(0, cn::kPortQ2), cfg);
  std::vector<std::string> said;
  const sv::Q2Server* game = sv::q2_of(r->rules);
  // Each step: how far the drawn bob is from where the server has him now.
  double off = 0;
  int drawn = 0, most_others = 0;
  const auto steps = [&](int n) {
    for (int i = 0; i < n; i++) {
      r->now += 13000;
      gn::sim_advance(r->net, r->now);
      for (auto& c : r->clients) cn::q2_client_tick(*c, r->now);
      if (cn::remote_step(remote, 0.013, said) == cn::RemoteStep::new_level)
        cn::remote_world(remote, nullptr, r->map.get());
      cn::q2_net_frame(r->server, r->now);
      most_others = std::max(most_others, remote.others);
      const cl::DemoState& st = remote.demo.state;
      for (int k = 0; k < sh::kMaxClients; k++) {
        if (r->server.clients[k].name != "bob" || static_cast<size_t>(k) >= st.players.size() ||
            !st.players[static_cast<size_t>(k)].present)
          continue;
        const ge::classic::Q2State& m = game->players[k].move;
        const sh::Vec3f at{static_cast<float>(m.origin[0]) * 0.125f,
                           static_cast<float>(m.origin[1]) * 0.125f,
                           static_cast<float>(m.origin[2]) * 0.125f};
        off += sh::length(st.players[static_cast<size_t>(k)].origin - at);
        drawn++;
      }
    }
  };
  steps(200);
  CHECK_MSG(remote.q2->stage == cn::ClientStage::active, remote.q2->refused);
  CHECK(remote.q2->moves_read > 0);  // our server sends the moves to a client that asks
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
  cn::Q2NetConfig sc;
  sc.moves_between = between;
  auto r = rig(sc, "broadband");
  bool go = false;
  cn::Q2ClientConfig bob;
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
  remote.q2_game = [] { return true; };
  remote.read_file = [](const std::string&, const std::string&) { return std::vector<uint8_t>{1}; };
  cn::ClientConfig cfg;
  cfg.name = "eve";
  cn::remote_start(remote, t, gn::sim_address(0, cn::kPortQ2), cfg);
  std::vector<std::string> said;
  const sv::Q2Server* game = sv::q2_of(r->rules);
  double off = 0;
  int drawn = 0;
  const auto steps = [&](int n, bool count) {
    for (int i = 0; i < n; i++) {
      r->now += 13000;
      gn::sim_advance(r->net, r->now);
      for (auto& c : r->clients) cn::q2_client_tick(*c, r->now);
      if (cn::remote_step(remote, 0.013, said) == cn::RemoteStep::new_level)
        cn::remote_world(remote, nullptr, r->map.get());
      cn::q2_net_frame(r->server, r->now);
      const cl::DemoState& st = remote.demo.state;
      for (int k = 0; count && k < sh::kMaxClients; k++) {
        if (r->server.clients[k].name != "bob" || static_cast<size_t>(k) >= st.players.size() ||
            !st.players[static_cast<size_t>(k)].present)
          continue;
        const ge::classic::Q2State& m = game->players[k].move;
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
  *moves_read = static_cast<int>(remote.q2->moves_read);
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
  cn::Q2ClientConfig bob;
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
  remote.q2_game = [] { return true; };
  remote.read_file = [](const std::string&, const std::string&) { return std::vector<uint8_t>{1}; };
  cn::ClientConfig cfg;
  cfg.name = "eve";
  cn::remote_start(remote, t, gn::sim_address(0, cn::kPortQ2), cfg);
  std::vector<std::string> said;
  const sv::Q2Server* game = sv::q2_of(r->rules);
  double off = 0;
  int drawn = 0, most_others = 0;
  const auto steps = [&](int n) {
    for (int i = 0; i < n; i++) {
      r->now += 13000;
      gn::sim_advance(r->net, r->now);
      for (auto& c : r->clients) cn::q2_client_tick(*c, r->now);
      if (cn::remote_step(remote, 0.013, said) == cn::RemoteStep::new_level)
        cn::remote_world(remote, nullptr, r->map.get());
      for (cn::Q2NetClient& c : r->server.clients)
        if (c.name == "eve") c.told_moves = false;  // as a stock server: no moves sent
      cn::q2_net_frame(r->server, r->now);
      most_others = std::max(most_others, remote.others);
      const cl::DemoState& st = remote.demo.state;
      for (int k = 0; k < sh::kMaxClients; k++) {
        if (r->server.clients[k].name != "bob" || static_cast<size_t>(k) >= st.players.size() ||
            !st.players[static_cast<size_t>(k)].present)
          continue;
        const ge::classic::Q2State& m = game->players[k].move;
        const sh::Vec3f at{static_cast<float>(m.origin[0]) * 0.125f,
                           static_cast<float>(m.origin[1]) * 0.125f,
                           static_cast<float>(m.origin[2]) * 0.125f};
        off += sh::length(st.players[static_cast<size_t>(k)].origin - at);
        drawn++;
      }
    }
  };
  steps(200);
  CHECK_MSG(remote.q2->stage == cn::ClientStage::active, remote.q2->refused);
  go = true;
  steps(30);
  off = 0;
  drawn = 0;
  steps(70);
  const double run_on = off / std::max(drawn, 1);
  CHECK(remote.q2->moves_read == 0);
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
  cn::Q2ClientConfig a;
  a.name = "a";
  a.brain = forward;
  cn::Q2ClientConfig b = a;
  b.name = "b";
  b.ask_moves = true;
  cn::Q2ClientEnd& ca = join(*r, a);
  cn::Q2ClientEnd& cb = join(*r, b);
  run(*r, 200);
  CHECK(ca.stage == cn::ClientStage::active && cb.stage == cn::ClientStage::active);
  CHECK(ca.moves_read == 0 && ca.demo.other_moves.empty());
  CHECK(cb.moves_read > 0);
  CHECK(cb.demo.other_moves.size() == 1);
}

// The easy jump on a server of Quake II: the user info says `cl_speedjump 1`, our server tells
// the share (its pm_speedjump), and the hands strafe for forward and jump held, past the run's 300.
namespace {

// An item taken on our server is told to the window with where it lies, its model and when it
// comes back, so the window draws it coming back ahead of time.
TEST(an_item_taken_reaches_the_window_with_when_it_comes_back) {
  auto map = std::make_shared<br::Map>(*hall());
  map->entities.push_back(entity({{"classname", "item_health"}, {"origin", "0 200 24"}}));
  auto r = rig({}, "lan", map);
  gn::Transport t;
  gn::open_sim_transport(t, r->net, gn::sim_address(9, 27901));
  cn::Remote remote;
  remote.q2_game = [] { return true; };
  remote.read_file = [](const std::string&, const std::string&) { return std::vector<uint8_t>{1}; };
  cn::ClientConfig cfg;
  cfg.name = "eve";
  cn::remote_start(remote, t, gn::sim_address(0, cn::kPortQ2), cfg);
  std::vector<std::string> said;
  const auto steps = [&](int n) {
    for (int i = 0; i < n; i++) {
      r->now += 13000;
      gn::sim_advance(r->net, r->now);
      cn::remote_step(remote, 0.0131, said);
      cn::q2_net_frame(r->server, r->now);
    }
  };
  steps(150);
  CHECK_MSG(remote.q2->stage == cn::ClientStage::active, remote.q2->refused);
  sv::Q2Server& s = *sv::q2_of(r->rules);
  int at = -1;
  for (size_t i = 0; i < s.entities.size(); i++)
    if (s.entities[i].item >= 0) at = static_cast<int>(i);
  CHECK(at >= 0);
  if (at < 0) return;
  s.entities[size_t(at)].hidden = true;  // as if taken: back in two seconds
  s.entities[size_t(at)].respawn_at = s.level_time + 2;
  const int number = 1 + sh::kMaxClients + at;
  const cl::DemoItemBack* told = nullptr;
  for (int k = 0; k < 40 && told == nullptr; k++) {
    steps(1);
    for (const cl::DemoItemBack& b : remote.demo.state.items_back)
      if (b.entity == number) told = &b;
  }
  CHECK(told != nullptr);
  if (told == nullptr) return;
  CHECK(told->model == s.entities[size_t(at)].modelindex);
  CHECK(std::abs(told->origin.y - 200) < 1);
  const double left = told->at - remote.demo.state.time;
  CHECK_MSG(left > 1.6 && left <= 2.0, std::to_string(left));
}

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
  remote.q2_game = [] { return true; };
  remote.read_file = [](const std::string&, const std::string&) { return std::vector<uint8_t>{1}; };
  cn::ClientConfig cfg;
  cfg.name = "eve";
  cfg.speedjump = aid;
  cn::remote_start(remote, t, gn::sim_address(0, cn::kPortQ2), cfg);
  std::vector<std::string> said;
  JumpRun out;
  const auto steps = [&](int n) {
    for (int i = 0; i < n; i++) {
      r->now += 13000;
      gn::sim_advance(r->net, r->now);
      if (cn::remote_step(remote, 0.0131, said) == cn::RemoteStep::new_level)
        cn::remote_world(remote, nullptr, r->map.get());
      cn::q2_net_frame(r->server, r->now);
      const sh::Vec3f v = remote.q2_ahead.velocity;
      if (remote.q2_ahead.predicted)
        out.fastest = std::max(out.fastest, std::sqrt(v.x * v.x + v.y * v.y));
    }
  };
  steps(200);
  CHECK_MSG(remote.q2->stage == cn::ClientStage::active, remote.q2->refused);
  out.fastest = 0;
  remote.hands.forward = 300;
  remote.hands.buttons = sh::kButtonJump;
  steps(260);
  out.told = remote.demo.speedjump_share;
  for (const cn::Q2NetClient& c : r->server.clients)
    if (c.stage != cn::Q2Slot::free)
      out.said = c.userinfo.find("cl_speedjump\\1") != std::string::npos;
  return out;
}

}  // namespace

TEST(the_easy_jump_strafes_on_our_server_of_q2_at_its_share) {
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

TEST(a_joined_server_hears_a_new_name_skin_hand_fov_and_msg_at_once) {
  // Joined to our server: a rename reaches the server's own user info for the player.
  auto r = rig({}, "broadband");
  gn::Transport t;
  gn::open_sim_transport(t, r->net, gn::sim_address(9, 27901));
  cn::Remote remote;
  remote.q2_game = [] { return true; };
  remote.read_file = [](const std::string&, const std::string&) { return std::vector<uint8_t>{1}; };
  cn::ClientConfig cfg;
  cfg.name = "eve";
  cn::remote_start(remote, t, gn::sim_address(0, cn::kPortQ2), cfg);
  std::map<std::string, std::string> live{{"name", "eve"}};
  remote.setting = [&live](const std::string& k) {
    const auto at = live.find(k);
    return at == live.end() ? std::string() : at->second;
  };
  std::vector<std::string> said;
  const auto steps = [&](int n) {
    for (int i = 0; i < n; i++) {
      r->now += 13000;
      gn::sim_advance(r->net, r->now);
      if (cn::remote_step(remote, 0.0131, said) == cn::RemoteStep::new_level)
        cn::remote_world(remote, nullptr, r->map.get());
      cn::q2_net_frame(r->server, r->now);
    }
  };
  steps(200);
  CHECK_MSG(remote.q2->stage == cn::ClientStage::active, remote.q2->refused);
  live["name"] = "zed";
  live["msg"] = "2";
  steps(20);
  bool renamed = false;
  for (const cn::Q2NetClient& c : r->server.clients)
    if (c.stage != cn::Q2Slot::free)
      renamed = c.userinfo.find("name\\zed") != std::string::npos &&
                c.userinfo.find("msg\\2") != std::string::npos;
  CHECK(renamed);
}

TEST(the_rate_holds_back_frames_past_a_clients_bytes_a_second) {
  cn::Q2NetConfig open;  // no floor or cap: the rate each client asks for
  open.min_rate = 0;
  open.max_rate = 0;
  auto r = rig(open);
  cn::Q2ClientConfig fast, slow;
  fast.name = "eve";
  fast.brain = forward;
  slow = fast;
  slow.name = "bob";
  slow.rate = 100;  // the least Quake II takes: a frame now and then
  cn::Q2ClientEnd& f = join(*r, fast);
  cn::Q2ClientEnd& s = join(*r, slow);
  run(*r, 120);
  CHECK_MSG(f.stage == cn::ClientStage::active && s.stage == cn::ClientStage::active,
            f.refused + s.refused);
  const int64_t f0 = f.frames, s0 = s.frames;
  run(*r, 231);  // three seconds
  const cn::Q2NetClient &sf = r->server.clients[0], &ss = r->server.clients[1];
  CHECK(sf.rate == 15000 && ss.rate == 100);
  CHECK_MSG(f.frames - f0 >= 28 && sf.held == 0, std::to_string(f.frames - f0) + " frames");
  CHECK_MSG(s.frames - s0 >= 1 && s.frames - s0 <= 5 && ss.held >= 20,
            std::to_string(s.frames - s0) + " frames, " + std::to_string(ss.held) + " held");
  CHECK(ss.wire.suppress <= 10);  // the count the next frame carries, a frame's worth at most
  CHECK(ss.full_frames <= 2);     // a held frame is no lost frame: the deltas go on
  CHECK(s.stage == cn::ClientStage::active);
}

// At `sv_fps` 40 a client that asks for every frame (`*ge_ext` bit 256) is sent forty a second,
// numbered by the rules' frames and played on deltas; a stock client still gets ten.
TEST(at_sv_fps_40_our_client_gets_every_frame_and_a_stock_one_the_tenths) {
  auto r = rig({}, "lan", nullptr, 40);
  cn::Q2ClientConfig ours, stock;
  ours.name = "eve";
  ours.brain = forward;
  ours.ask_frames = true;
  stock = ours;
  stock.name = "bob";
  stock.ask_frames = false;
  cn::Q2ClientEnd& o = join(*r, ours);
  cn::Q2ClientEnd& k = join(*r, stock);
  run(*r, 120);
  CHECK_MSG(o.stage == cn::ClientStage::active && k.stage == cn::ClientStage::active,
            o.refused + k.refused);
  CHECK(r->server.clients[0].told_frames && !r->server.clients[1].told_frames);
  const int64_t o0 = o.frames, k0 = k.frames;
  run(*r, 231);  // three seconds
  CHECK_MSG(o.frames - o0 >= 110 && o.frames - o0 <= 125, std::to_string(o.frames - o0));
  CHECK_MSG(k.frames - k0 >= 28 && k.frames - k0 <= 31, std::to_string(k.frames - k0));
  CHECK(r->server.clients[0].full_frames <= 2 && r->server.clients[1].full_frames <= 2);
  CHECK(o.stage == cn::ClientStage::active && k.stage == cn::ClientStage::active);
}

// Our client asks for the damage it dealt (`*ge_ext` bit 1024) and its frames carry the hit
// report; a stock client's never do.
TEST(our_client_is_sent_the_hit_report_and_a_stock_one_is_not) {
  auto r = rig({});
  cn::Q2ClientConfig ours, stock;
  ours.name = "eve";
  ours.ask_hits = true;
  stock = ours;
  stock.name = "bob";
  stock.ask_hits = false;
  cn::Q2ClientEnd& o = join(*r, ours);
  cn::Q2ClientEnd& k = join(*r, stock);
  run(*r, 60);
  CHECK_MSG(o.stage == cn::ClientStage::active && k.stage == cn::ClientStage::active,
            o.refused + k.refused);
  CHECK(r->server.clients[0].told_hits && !r->server.clients[1].told_hits);
  CHECK(r->server.clients[0].wire.opt.hit_reports && !r->server.clients[1].wire.opt.hit_reports);
}

TEST(a_frame_carries_the_count_held_back_before_it) {
  auto r = rig({});
  run(*r, 20);
  sv::Q2Wire w;
  sv::Q2Signon signon;
  sv::q2_signon_parts(w, *r->rules, 0, {}, signon);
  w.suppress = 6;
  std::vector<uint8_t> block, reliable;
  sv::q2_frame(w, *r->rules, 3, nullptr, block, &reliable);
  CHECK(block.size() > 10 && block[0] == 20);  // the frame first: id, number, delta's, suppress
  CHECK(block[9] == 6);
  CHECK(w.suppress == 0);
  sv::q2_frame(w, *r->rules, 4, nullptr, block, &reliable);
  CHECK(block[9] == 0);
}

TEST(a_rate_out_of_the_servers_bounds_is_set_back_and_the_player_told) {
  cn::Q2NetConfig cfg;
  cfg.max_rate = 3000;
  cfg.min_rate = 2000;
  auto r = rig(cfg);
  cn::Q2ClientConfig high, low;
  high.name = "eve";
  low.name = "bob";
  low.rate = 500;
  cn::Q2ClientEnd& h = join(*r, high);
  cn::Q2ClientEnd& l = join(*r, low);
  run(*r, 120);
  CHECK(r->server.clients[0].rate == 3000 && r->server.clients[1].rate == 2000);
  auto told = [](const cn::Q2ClientEnd& c, const std::string& said, const std::string& line) {
    bool p = false, st = false;
    for (const std::string& t : c.prints) p = p || t.find(said) != std::string::npos;
    for (const std::string& t : c.stuffed) st = st || t == line;
    // Not a line the client answers itself: it goes on to the player's console, run as typed.
    const bool run = std::find(c.to_run.begin(), c.to_run.end(), line) != c.to_run.end();
    return p && st && run;
  };
  CHECK(told(h, "Your rate setting is too high for this server.", "rate 3000"));
  CHECK(told(l, "Your rate setting is too low for this server.", "rate 2000"));
  bool listed = false;
  const ge::server::ServerGame& g = cn::q2_game();
  for (int i = 0; i < g.rule_count; i++)
    listed = listed || g.rules[i].name == std::string("sv_maxrate");
  CHECK(listed);
}

// Our server tells a joined window the feel it runs every move on (svc 122): with
// Quake's ground and air (`sv_physics q1`: 320 a second, friction 4, no landing pause) the
// window predicts that move, so walking and jumping are never corrected. On a phone's link (the
// moves in flight a round trip of 90 ms) the stock move predicted instead is corrected by 2.9
// units.
TEST(the_window_predicts_on_the_feel_our_server_of_q2_tells) {
  auto r = rig({}, "mobile");
  sv::q2_of(r->rules)->q1_physics = true;
  gn::Transport t;
  gn::open_sim_transport(t, r->net, gn::sim_address(9, 27901));
  cn::Remote remote;
  remote.q2_game = [] { return true; };
  remote.read_file = [](const std::string&, const std::string&) { return std::vector<uint8_t>{1}; };
  cn::ClientConfig cfg;
  cfg.name = "eve";
  cn::remote_start(remote, t, gn::sim_address(0, cn::kPortQ2), cfg);
  std::vector<std::string> said;
  float most_correction = 0;
  const auto steps = [&](int n) {
    for (int i = 0; i < n; i++) {
      r->now += 13000;
      gn::sim_advance(r->net, r->now);
      if (cn::remote_step(remote, 0.0131, said) == cn::RemoteStep::new_level)
        cn::remote_world(remote, nullptr, r->map.get());
      cn::q2_net_frame(r->server, r->now);
      if (remote.q2_ahead.predicted)
        most_correction = std::max(most_correction, sh::length(remote.q2_ahead.smooth.correction));
    }
  };
  steps(200);
  CHECK_MSG(remote.q2->stage == cn::ClientStage::active, remote.q2->refused);
  CHECK(remote.demo.feel_told);
  CHECK(remote.demo.feel.max_speed == 320 && remote.demo.feel.friction == 4);
  CHECK(!remote.demo.feel.land_pause && !remote.demo.feel.air_step);
  const sh::Vec3f start = remote.q2_ahead.origin;
  remote.hands.forward = 400;
  remote.hands.buttons = sh::kButtonJump;
  steps(150);
  remote.hands = {};
  steps(100);
  CHECK_MSG(most_correction == 0, std::to_string(most_correction) + " units corrected");
  const sh::Vec3f moved = remote.q2_ahead.origin - start;
  CHECK_MSG(sh::length(moved) > 100, "walked " + std::to_string(sh::length(moved)));
}

// Voice on our server of Quake II, by Quake's rules (net/qw_voice_route.hpp): the
// server says it takes a client's voice, the client unmutes and talks, and the voice reaches the
// talker's team (Quake II's teams by skin), everyone once he targets all, nobody who sent
// `muteall`, and never a stock client, which is never told and never dropped for it.
namespace {
bool always_talks(int /*tick*/, int /*seed*/) { return true; }
int slot_named(const Rig& r, const char* name) {
  for (int i = 0; i < sh::kMaxClients; i++)
    if (cn::q2_held(r.server.clients[i]) && r.server.clients[i].name == name) return i;
  return -1;
}
}  // namespace

TEST(voice_on_our_server_of_q2_reaches_whom_the_first_games_rules_say) {
  cn::Q2NetConfig cfg;
  cfg.voice = true;  // sv_voip 1
  auto r = rig(cfg);
  r->rules.console("dmflags 64");  // teams by skin
  cn::Q2ClientConfig a;
  a.name = "a";
  a.skin = "male/red";
  a.voice = true;
  a.talk = always_talks;
  cn::Q2ClientConfig b;
  b.name = "b";
  b.skin = "female/red";
  b.voice = true;
  cn::Q2ClientConfig c = b;
  c.name = "c";
  c.skin = "male/blue";
  cn::Q2ClientConfig d = b;
  d.name = "d";
  d.skin = "male/red";
  d.voice = false;  // a client that never asks
  cn::Q2ClientEnd& ca = join(*r, a);
  cn::Q2ClientEnd& cb = join(*r, b);
  cn::Q2ClientEnd& cc = join(*r, c);
  cn::Q2ClientEnd& cd = join(*r, d);
  std::vector<uint8_t> level;  // the level as a `record` keeps it
  ca.level_record = &level;
  run(*r, 300);
  CHECK(ca.stage == cn::ClientStage::active && cd.stage == cn::ClientStage::active);
  CHECK(ca.voice_on && cb.voice_on && cc.voice_on && !cd.voice_on);
  int own = 0;  // the talker's own voice in his recording, a block of its own each time
  for (size_t at = 0, len = 0; at + 4 < level.size(); at += 4 + len) {
    std::memcpy(&len, &level[at], 4);  // a block's length, little-endian (the hosts are)
    own += len > 0 && level[at + 4] == cn::kSvcGeVoice;
  }
  CHECK_MSG(own > 0, "the talker's recording kept none of his own voice");
  const int sa = slot_named(*r, "a");
  CHECK(sa >= 0);
  if (sa < 0) return;
  CHECK_MSG(ca.voice_sent > 0, "the talker sent nothing");
  CHECK_MSG(cb.voice_heard[sa] > 0, "his teammate heard nothing");
  CHECK_MSG(cc.voice_heard[sa] == 0, "the other team heard his team talk");
  CHECK(cd.voice_heard[sa] == 0 && ca.voice_heard[sa] == 0);  // a stock client; never his own
  cn::q2_voice_target(ca, "all");
  run(*r, 60);
  CHECK_MSG(cc.voice_heard[sa] > 0, "voicetarg all did not reach the other team");
  CHECK(cd.voice_heard[sa] == 0 && cd.stage == cn::ClientStage::active);
  cn::q2_client_command(cc, "muteall");
  run(*r, 30);
  const int64_t muted = cc.voice_heard[sa];
  run(*r, 60);
  CHECK_MSG(cc.voice_heard[sa] == muted, "muteall still heard voice");
  CHECK(cb.voice_heard[sa] > 0);
  // A server with voice off (sv_voip 0, the default): never said, so nobody talks.
  auto off = rig({});
  cn::Q2ClientEnd& oa = join(*off, a);
  run(*off, 200);
  CHECK(oa.stage == cn::ClientStage::active && !oa.voice_on && oa.voice_sent == 0);
}

// sv_antilag 1: our server keeps each player's places and boxes by the network's time and hands
// our rules the place a shooter's screen showed each other player (his newest acknowledged
// frame), unless his cl_antilag says 0; with it off (the default) the rules trace the players
// where they stand; with 2 and sv_antilag_projectiles the missiles are rewound too.
TEST(lag_compensation_on_our_server_of_q2_rewinds_to_the_shooters_frame) {
  cn::Q2NetConfig cfg;
  cfg.antilag = true;
  auto r = rig(cfg);
  cn::Q2ClientConfig a;
  a.name = "a";
  cn::Q2ClientConfig b = a;
  b.name = "b";
  join(*r, a);
  join(*r, b);
  run(*r, 300);
  const int sa = slot_named(*r, "a"), sb = slot_named(*r, "b");
  CHECK(sa >= 0 && sb >= 0);
  if (sa < 0 || sb < 0) return;
  sv::Q2Server& game = *r->server.game;
  CHECK(game.lag_place != nullptr);
  if (game.lag_place == nullptr) return;
  CHECK(r->server.clients[sa].acked > 0);
  sh::Vec3f at, lo, hi;
  CHECK(game.lag_place(game.lag_ctx, sa, sb, at, lo, hi));
  const int32_t* o = game.players[sb].move.origin;
  const sh::Vec3f now{float(o[0]) * 0.125f, float(o[1]) * 0.125f, float(o[2]) * 0.125f};
  CHECK_MSG(sh::length(at - now) < 1, "a standing player was rewound away from where he stands");
  CHECK(lo.z == game.players[sb].mins.z && hi.z == game.players[sb].maxs.z);  // his box then
  CHECK(!game.lag_place(game.lag_ctx, sa, sa, at, lo, hi));                   // never himself
  CHECK(!game.lag_place(game.lag_ctx, sa, 30, at, lo, hi));                   // nobody there
  CHECK(!game.lag_missiles);  // sv_antilag 1: hitscan only
  // A shooter whose user info says cl_antilag 0 is not rewound.
  r->server.clients[sa].userinfo += "\\cl_antilag\\0";
  CHECK(!game.lag_place(game.lag_ctx, sa, sb, at, lo, hi));
  // Off, the default: no rewinds.
  auto off = rig({});
  CHECK(off->server.game->lag_place == nullptr);
  // sv_antilag 2 with sv_antilag_projectiles: missiles in flight are rewound too.
  cn::Q2NetConfig two;
  two.antilag = 2;
  two.antilag_projectiles = true;
  auto r2 = rig(two);
  CHECK(r2->server.game->lag_place != nullptr && r2->server.game->lag_missiles);
}

// sv_landjump and sv_waterjump (both 1 by default) reach a client told the feel, whose prediction
// then hops with no landing pause and no correction; for a client that is not told, the server
// keeps the stock landing pause and surfacing lift, unless a setting is 2.
TEST(the_landing_and_surfacing_settings_reach_a_client_told_the_feel) {
  auto r = rig({}, "mobile");
  gn::Transport t;
  gn::open_sim_transport(t, r->net, gn::sim_address(9, 27901));
  cn::Remote remote;
  remote.q2_game = [] { return true; };
  remote.read_file = [](const std::string&, const std::string&) { return std::vector<uint8_t>{1}; };
  cn::ClientConfig cfg;
  cfg.name = "eve";
  cn::remote_start(remote, t, gn::sim_address(0, cn::kPortQ2), cfg);
  std::vector<std::string> said;
  float most_correction = 0;
  const auto steps = [&](int n) {
    for (int i = 0; i < n; i++) {
      r->now += 13000;
      gn::sim_advance(r->net, r->now);
      if (cn::remote_step(remote, 0.0131, said) == cn::RemoteStep::new_level)
        cn::remote_world(remote, nullptr, r->map.get());
      cn::q2_net_frame(r->server, r->now);
      if (remote.q2_ahead.predicted)
        most_correction = std::max(most_correction, sh::length(remote.q2_ahead.smooth.correction));
    }
  };
  steps(200);
  CHECK_MSG(remote.q2->stage == cn::ClientStage::active, remote.q2->refused);
  CHECK(remote.demo.feel_told);
  CHECK(!remote.demo.feel.land_pause && !remote.demo.feel.surface_lift);
  sv::Q2Server& game = *sv::q2_of(r->rules);
  const auto told = sv::q2_move_feel(game, 0);
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
  const auto untold = sv::q2_move_feel(game, 0);
  CHECK(untold.land_pause && untold.surface_lift);
  game.cvars.set("sv_landjump", "2", ge::console::Source::code);
  CHECK(!sv::q2_move_feel(game, 0).land_pause);
}

// A Quake II server beats to a hub's masters under the game's own id, as a Quake server does, so
// the hub's list (which asks for that id alone) shows both.
TEST(q2_server_is_listed_under_the_games_id) {
  const ge::server::ServerGame& g = shooters_edge::net::q2_game();
  CHECK(g.protocol != 0);
  CHECK(g.protocol == ge::net::protocol_id("shooters_edge", 1));
  CHECK(g.fill_status != nullptr);
}
