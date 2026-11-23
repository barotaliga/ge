// games/shooters_edge/net/q2_mvd_server.hpp: Quake II's multiview recorder on a live server. A
// player plays while `mvdrecord` writes a .mvd2 our reader opens and follows; a relay over TCP
// opens with the magic and a hello, starts the stream and gets the game state and a block a frame,
// plain or as one zlib stream; a wrong password, a banned address and a stream that is not one are
// refused; the access lists and `status` answer in the server's words.
//
// ge:summary Test: Quake II's multiview recorder: mvdrecord's file, the relay stream over TCP
// ge:keywords test mvd2 mvdrecord gtv relay-stream tcp hello deflate addgtvhost addgtvban
// ge:skill net shooter testing
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "core/inflate.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "net/tcp_socket.hpp"
#include "shooters_edge/client/demo_mvd2.hpp"
#include "shooters_edge/net/q2_admin.hpp"
#include "shooters_edge/net/q2_client.hpp"
#include "shooters_edge/net/q2_mvd_server.hpp"
#include "shooters_edge/net/q2_net_server.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace gn = ge::net;
namespace sv = shooters_edge::server;
namespace cn = shooters_edge::net;

std::shared_ptr<const br::Map> hall() {
  br::BrushWorld w;
  w.models.resize(1);
  auto box = [](float x0, float y0, float z0, float x1, float y1, float z1) {
    const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
    return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
  };
  auto& m = w.models[0];
  m.push_back(box(-1024, -1024, -16, 1024, 1024, 0));
  m.push_back(box(-1040, -1024, -16, -1024, 1024, 256));
  m.push_back(box(1024, -1024, -16, 1040, 1024, 256));
  m.push_back(box(-1040, 1024, -16, 1040, 1040, 256));
  m.push_back(box(-1040, -1040, -16, 1040, -1024, 256));
  br::Entity spot;
  spot.pairs = {
      {"classname", "info_player_deathmatch"}, {"origin", "-400 -400 24"}, {"angle", "0"}};
  w.entities.push_back(spot);
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

struct Rig {
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st, ct;
  cn::Q2NetServer server;
  cn::Q2ClientEnd player;
  std::vector<std::string> log;
  int64_t now = 0;
};

std::unique_ptr<Rig> rig(const cn::Q2MvdConfig& mvd) {
  auto r = std::make_unique<Rig>();
  r->rules = sv::q2_runner(sv::make_q2_server({}));
  r->rules.console("deathmatch 1");
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall();
  std::string error;
  CHECK_MSG(r->rules.start(map, error), error);
  gn::LinkConditions link;
  CHECK(gn::conditions_preset("lan", link));
  r->net = gn::make_sim_network(7, link);
  gn::open_sim_transport(r->st, r->net, gn::sim_address(0, cn::kPortQ2));
  cn::Q2NetConfig cfg;
  cfg.mvd = mvd;
  CHECK_MSG(cn::q2_net_start(r->server, r->rules, r->st, cfg, error), error);
  Rig* raw = r.get();
  r->server.mvd.log = [raw](const std::string& line) { raw->log.push_back(line); };
  gn::open_sim_transport(r->ct, r->net, gn::sim_address(1, 27901));
  cn::Q2ClientConfig cc;
  cc.qport = 101;
  cn::q2_client_start(r->player, r->ct, gn::sim_address(0, cn::kPortQ2), cc, 0);
  return r;
}

void run(Rig& r, int ticks, gn::TcpStream* relay = nullptr) {
  for (int i = 0; i < ticks; i++) {
    r.now += 13000;
    gn::sim_advance(r.net, r.now);
    cn::q2_client_tick(r.player, r.now);
    cn::q2_net_frame(r.server, r.now);
    if (relay != nullptr) gn::tcp_pump(*relay);
  }
}

std::string admin(Rig& r, const std::string& line) {
  std::string out;
  CHECK_MSG(cn::q2_admin(r.server, line, out), line);
  return out;
}

void put16(std::vector<uint8_t>& b, uint32_t v) {
  b.push_back(static_cast<uint8_t>(v & 0xFF));
  b.push_back(static_cast<uint8_t>(v >> 8));
}

// A relay's message: its length (counting the id), its id, its payload.
std::vector<uint8_t> msg(uint8_t id, const std::vector<uint8_t>& payload = {}) {
  std::vector<uint8_t> b;
  put16(b, uint32_t(payload.size() + 1));
  b.push_back(id);
  b.insert(b.end(), payload.begin(), payload.end());
  return b;
}

std::vector<uint8_t> hello(uint32_t flags, const std::string& password) {
  std::vector<uint8_t> p;
  put16(p, cn::kGtvProtocol);
  for (int k = 0; k < 4; k++) p.push_back(static_cast<uint8_t>((flags >> (8 * k)) & 0xFF));
  for (int k = 0; k < 4; k++) p.push_back(0);
  for (const std::string& s : {std::string("relay"), password, std::string("Test relay 1.0")}) {
    p.insert(p.end(), s.begin(), s.end());
    p.push_back(0);
  }
  return msg(cn::kGtvHello, p);
}

struct Message {
  uint8_t id = 0;
  std::vector<uint8_t> payload;
};

// The server's messages in `bytes` from `at` on.
std::vector<Message> messages(const std::vector<uint8_t>& bytes, size_t at = 0) {
  std::vector<Message> out;
  while (bytes.size() - at >= 3) {
    const size_t len = size_t(bytes[at]) | size_t(bytes[at + 1]) << 8;
    if (len == 0 || bytes.size() - at - 2 < len) break;
    Message m;
    m.id = bytes[at + 2];
    m.payload.assign(bytes.begin() + static_cast<std::ptrdiff_t>(at + 3),
                     bytes.begin() + static_cast<std::ptrdiff_t>(at + 2 + len));
    out.push_back(std::move(m));
    at += 2 + len;
  }
  return out;
}

// A relay connected to the recorder's listener.
gn::TcpStream dial(Rig& r) {
  gn::TcpStream s;
  std::string error;
  CHECK_MSG(gn::tcp_connect(s, gn::loopback_v4(r.server.mvd.listener.local.port), &error), error);
  return s;
}

cn::Q2MvdConfig streaming() {
  cn::Q2MvdConfig m;
  m.enable = 2;
  m.suspend_min = 0;
  return m;
}

}  // namespace

TEST(mvdrecord_writes_a_recording_our_reader_follows) {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "ge_q2_mvd_server_test";
  std::filesystem::remove_all(dir);
  cn::Q2MvdConfig cfg;
  cfg.enable = 1;
  cfg.demos = dir.string();
  auto r = rig(cfg);
  CHECK(admin(*r, "mvdrecord") ==
        "Missing filename argument.\nTry 'mvdrecord --help' for more information.\n");
  CHECK(admin(*r, "mvdrecord -h").rfind("Usage: mvdrecord [-hz] <filename>\n", 0) == 0);
  CHECK(admin(*r, "mvdstop") == "Not recording a local MVD.\n");
  run(*r, 100);  // the player joins
  CHECK(r->server.clients[0].stage == cn::Q2Slot::active);
  const std::string started = admin(*r, "mvdrecord duel");
  CHECK_MSG(started == "Recording local MVD to " + (dir / "duel.mvd2").string() + "\n", started);
  CHECK(admin(*r, "mvdrecord again") == "Already recording a local MVD.\n");
  run(*r, 150);
  CHECK(admin(*r, "mvdstop") == "Stopped local MVD recording.\n");
  CHECK(admin(*r, "mvdstuff wait 10").empty());  // the recorder's spectator takes it
  std::ifstream f(dir / "duel.mvd2", std::ios::binary);
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), {});
  CHECK(sv::is_mvd2(bytes));
  CHECK_MSG(bytes.size() > 400, std::to_string(bytes.size()));
  std::vector<uint8_t> view;
  std::string error;
  CHECK_MSG(shooters_edge::client::mvd2_view(bytes, -1, view, error), error);
  CHECK(view.size() > 100);
  // Compressed: the same, gzipped.
  CHECK(admin(*r, "mvdrecord -z duel2") ==
        "Recording local MVD to " + (dir / "duel2.mvd2.gz").string() + "\n");
  run(*r, 60);
  admin(*r, "mvdstop");
  std::ifstream g(dir / "duel2.mvd2.gz", std::ios::binary);
  const std::vector<uint8_t> packed((std::istreambuf_iterator<char>(g)), {});
  const auto plain = ge::core::inflate_gzip(packed);
  CHECK(plain.has_value() && sv::is_mvd2(*plain));
  std::filesystem::remove_all(dir);
}

TEST(a_disabled_recorder_refuses_to_record) {
  auto r = rig({});
  run(*r, 5);
  CHECK(admin(*r, "mvdrecord x") == "MVD recording is disabled on this server.\n");
  CHECK(admin(*r, "mvdstuff help") == "Can't 'mvdstuff', dummy MVD client is not active\n");
}

// The recorder's spectator: made when the recorder switches on, in a slot past the seats, a
// spectator to the rules, sent nothing and listed nowhere; its begin commands open the
// scoreboard, `mvdstuff` runs its words; the stream names it the observer with the "messages
// left out" flag, and a view of no one follows the player, not it. Off, it leaves.
TEST(the_recorders_spectator_holds_the_scoreboard_and_takes_mvdstuff) {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "ge_q2_mvd_dummy_test";
  std::filesystem::remove_all(dir);
  cn::Q2MvdConfig cfg;
  cfg.enable = 1;
  cfg.demos = dir.string();
  cfg.begincmd = "wait 2; help;";
  cfg.disconnect_min = 1;
  auto r = rig(cfg);
  run(*r, 100);
  CHECK(r->server.mvd.dummy < 0);
  CHECK(admin(*r, "mvdrecord spec").rfind("Recording local MVD to ", 0) == 0);
  run(*r, 10);
  const int d = r->server.mvd.dummy;
  CHECK_MSG(d >= cn::q2_net_slots(r->server), std::to_string(d));
  CHECK(r->server.clients[d].stage == cn::Q2Slot::dummy);
  CHECK(r->server.clients[d].name == "[MVDSPEC]");
  CHECK((*r->rules).clients[d].active && (*r->rules).clients[d].spectator);
  CHECK(cn::q2_net_clients(r->server) == 1);  // the browser counts the player alone
  CHECK(admin(*r, "status").find("[MVDSPEC]") == std::string::npos);
  run(*r, 60);  // `wait 2` over, `help`: the scoreboard up
  CHECK_MSG(sv::q2_of(r->rules)->players[d].screen != 0, "no scoreboard");
  CHECK(admin(*r, "mvdstuff \"putaway\"").empty());
  run(*r, 20);
  CHECK(sv::q2_of(r->rules)->players[d].screen == 0);
  CHECK(admin(*r, "mvdstop") == "Stopped local MVD recording.\n");
  std::ifstream f(dir / "spec.mvd2", std::ios::binary);
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), {});
  CHECK(bytes.size() > 22);
  // The magic, the block's length, the server data with the flag in its top bits, 37, 2010, the
  // count, the game folder, the observer.
  CHECK(bytes[6] == (sv::kMv2ServerData | (sv::kMvfNoMsgs << 5)));
  const size_t at = 6 + 1 + 4 + 2 + 4 + 1;
  CHECK(int(int16_t(bytes[at] | bytes[at + 1] << 8)) == d);
  std::vector<uint8_t> view;
  std::string error;
  CHECK_MSG(shooters_edge::client::mvd2_view(bytes, -1, view, error), error);
  // Nothing records or streams: after `sv_mvd_disconnect_time` the recorder stops and it leaves.
  run(*r, 4800);
  CHECK(r->server.mvd.dummy < 0);
  CHECK(r->server.clients[d].stage == cn::Q2Slot::free);
  CHECK(!(*r->rules).clients[d].active);
  std::filesystem::remove_all(dir);
}

TEST(a_relay_gets_the_game_state_and_a_block_a_frame) {
  auto r = rig(streaming());
  CHECK(cn::q2_mvd_listen(r->server.mvd, gn::loopback_v4(0)));
  gn::TcpStream relay = dial(*r);
  const uint8_t magic[4] = {'M', 'V', 'D', '2'};
  CHECK(gn::tcp_send_bytes(relay, magic, 4));
  const std::vector<uint8_t> h = hello(0, "");
  CHECK(gn::tcp_send_bytes(relay, h.data(), h.size()));
  run(*r, 60, &relay);
  const std::vector<uint8_t> start = msg(cn::kGtvStreamStart, {10, 0});
  CHECK(gn::tcp_send_bytes(relay, start.data(), start.size()));
  run(*r, 150, &relay);
  std::vector<uint8_t> got;
  gn::tcp_take_bytes(relay, got);
  CHECK(got.size() > 4 && std::equal(got.begin(), got.begin() + 4, magic));
  const std::vector<Message> ms = messages(got, 4);
  CHECK(ms.size() > 5);
  CHECK(ms[0].id == cn::kGtsHello && ms[0].payload.size() == 4 && ms[0].payload[0] == 0);
  CHECK(ms[1].id == cn::kGtsStreamStart);
  // The recorder was off: it answers suspended (an empty block), then resumes with the game state.
  CHECK(ms[2].id == cn::kGtsStreamData && ms[2].payload.empty());
  CHECK(ms[3].id == cn::kGtsStreamData);
  CHECK(!ms[3].payload.empty() && (ms[3].payload[0] & 0x1F) == sv::kMv2ServerData);
  int frames = 0;
  for (size_t i = 4; i < ms.size(); i++)
    if (ms[i].id == cn::kGtsStreamData) frames++;
  CHECK_MSG(frames > 10, std::to_string(frames));
  CHECK(r->log.size() >= 1 && r->log[0].rfind("Accepted MVD client relay[", 0) == 0);
  // The game state and the blocks read back as a recording.
  std::vector<uint8_t> file(magic, magic + 4);
  for (size_t i = 2; i < ms.size(); i++) {
    if (ms[i].id != cn::kGtsStreamData || ms[i].payload.empty()) continue;
    put16(file, uint32_t(ms[i].payload.size()));
    file.insert(file.end(), ms[i].payload.begin(), ms[i].payload.end());
  }
  put16(file, 0);
  std::vector<uint8_t> view;
  std::string error;
  CHECK_MSG(shooters_edge::client::mvd2_view(file, -1, view, error), error);
  const std::string status = admin(*r, "status");
  CHECK_MSG(status.find(" SEND") != std::string::npos, status);
  // Stopped, then the server shuts down: the stop's ack, then the disconnect message.
  const std::vector<uint8_t> stop = msg(cn::kGtvStreamStop);
  CHECK(gn::tcp_send_bytes(relay, stop.data(), stop.size()));
  run(*r, 5, &relay);
  cn::q2_net_shutdown(r->server, "Server quit\n", false);
  for (int k = 0; k < 100; k++) gn::tcp_pump(relay);
  got.clear();
  gn::tcp_take_bytes(relay, got);
  const std::vector<Message> tail = messages(got);
  bool acked = false, told = false;
  for (const Message& m : tail) {
    acked = acked || m.id == cn::kGtsStreamStop;
    told = told || m.id == cn::kGtsDisconnect;
  }
  CHECK(acked && told);
  gn::tcp_close(relay);
}

TEST(a_relay_asking_for_deflate_gets_one_zlib_stream) {
  auto r = rig(streaming());
  CHECK(cn::q2_mvd_listen(r->server.mvd, gn::loopback_v4(0)));
  gn::TcpStream relay = dial(*r);
  const uint8_t magic[4] = {'M', 'V', 'D', '2'};
  gn::tcp_send_bytes(relay, magic, 4);
  const std::vector<uint8_t> h = hello(cn::kGtfDeflate | cn::kGtfStringCmds, "");
  gn::tcp_send_bytes(relay, h.data(), h.size());
  run(*r, 30, &relay);
  const std::vector<uint8_t> start = msg(cn::kGtvStreamStart, {10, 0});
  gn::tcp_send_bytes(relay, start.data(), start.size());
  run(*r, 80, &relay);
  cn::q2_net_shutdown(r->server, "Server quit\n", true);
  for (int k = 0; k < 100; k++) gn::tcp_pump(relay);
  std::vector<uint8_t> got;
  gn::tcp_take_bytes(relay, got);
  const std::vector<Message> plain = messages(got, 4);
  CHECK(!plain.empty() && plain[0].id == cn::kGtsHello);
  // Deflate taken; string commands never.
  CHECK(plain[0].payload[0] == cn::kGtfDeflate);
  const size_t z = 4 + 2 + 1 + 4;
  const std::vector<uint8_t> packed(got.begin() + static_cast<std::ptrdiff_t>(z), got.end());
  const auto inflated = ge::core::inflate_zlib(packed);
  CHECK(inflated.has_value());
  if (!inflated) return;
  const std::vector<Message> ms = messages(*inflated);
  CHECK(ms.size() > 5);
  CHECK(ms[0].id == cn::kGtsStreamStart && ms[1].id == cn::kGtsStreamData);
  CHECK(ms[2].id == cn::kGtsStreamData && (ms[2].payload[0] & 0x1F) == sv::kMv2ServerData);
  CHECK(ms.back().id == cn::kGtsReconnect);  // a restart tells the relay to come back
  gn::tcp_close(relay);
}

TEST(the_wrong_password_a_ban_and_a_stranger_are_refused) {
  cn::Q2MvdConfig cfg = streaming();
  cfg.password = "secret";
  auto r = rig(cfg);
  CHECK(cn::q2_mvd_listen(r->server.mvd, gn::loopback_v4(0)));
  const uint8_t magic[4] = {'M', 'V', 'D', '2'};
  const auto attempt = [&](const std::string& password) {
    gn::TcpStream relay = dial(*r);
    gn::tcp_send_bytes(relay, magic, 4);
    const std::vector<uint8_t> h = hello(0, password);
    gn::tcp_send_bytes(relay, h.data(), h.size());
    run(*r, 30, &relay);
    std::vector<uint8_t> got;
    gn::tcp_take_bytes(relay, got);
    gn::tcp_close(relay);
    const std::vector<Message> ms = messages(got, 4);
    return ms.empty() ? -1 : int(ms[0].id);
  };
  CHECK(attempt("guess") == cn::kGtsNoAccess);
  CHECK(r->log.back().find("dropped: not authorized") != std::string::npos);
  CHECK(attempt("secret") == cn::kGtsHello);
  // The allow list lets an address in without the password; the ban list refuses it.
  CHECK(admin(*r, "addgtvhost 127.0.0.1 this machine").empty());
  CHECK(attempt("") == cn::kGtsHello);
  CHECK(admin(*r, "listgtvhosts").find("127.0.0.1/32       1    ") != std::string::npos);
  CHECK(admin(*r, "delgtvhost 1").empty());
  CHECK(admin(*r, "addgtvban 127.0.0.0/8").empty());
  CHECK(attempt("secret") == cn::kGtsNoAccess);
  CHECK(admin(*r, "listgtvhosts") == "Address list is empty.\n");
  // Not the stream's magic: closed with nothing said, nothing logged.
  const size_t logged = r->log.size();
  gn::TcpStream http = dial(*r);
  const std::string get = "GET / HTTP/1.0\r\n\r\n";
  gn::tcp_send_bytes(http, reinterpret_cast<const uint8_t*>(get.data()), get.size());
  run(*r, 30, &http);
  std::vector<uint8_t> got;
  gn::tcp_take_bytes(http, got);
  CHECK(got.empty());
  CHECK(r->log.size() == logged);
  gn::tcp_close(http);
}

// sv_mvd_maxsize and sv_mvd_maxtime: a recording stops on its own once past its size in kilobytes
// or its length in minutes, and says why on the console.
TEST(a_recording_stops_at_its_size_and_at_its_length) {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "ge_q2_mvd_server_limits_test";
  std::filesystem::remove_all(dir);
  for (const bool by_size : {true, false}) {
    cn::Q2MvdConfig cfg;
    cfg.enable = 1;
    cfg.demos = dir.string();
    if (by_size)
      cfg.maxsize_kb = 2;
    else
      cfg.maxtime_min = 1;
    auto r = rig(cfg);
    run(*r, 100);
    CHECK(admin(*r, "mvdrecord limit").rfind("Recording local MVD to ", 0) == 0);
    run(*r, by_size ? 400 : 5000);  // a minute is 600 frames, 4600 ticks
    const std::string why = by_size ? "Stopping MVD recording, maximum size reached."
                                    : "Stopping MVD recording, maximum duration reached.";
    CHECK_MSG(std::find(r->log.begin(), r->log.end(), why) != r->log.end(), why);
    CHECK(admin(*r, "mvdstop") == "Not recording a local MVD.\n");
  }
  std::filesystem::remove_all(dir);
}
