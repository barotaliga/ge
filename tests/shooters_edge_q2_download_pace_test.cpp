// Tests: Quake II's server downloads paced (net/q2_net_download.hpp): pieces fill the packet
// (1386 bytes on a 1390-byte one), one for each `nextdl`, the file comes down whole, the rate
// spaces them, and a resume at the very end is one empty piece at 100.
//
// ge:summary Test: Quake II's download pieces filling the packet, one a nextdl, paced by the rate
// ge:keywords test q2-download download-pace nextdl piece-size rate-pause
// ge:skill net shooter testing
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classic/protocol/channel.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "shooters_edge/net/q2_client.hpp"
#include "shooters_edge/net/q2_net.hpp"
#include "shooters_edge/net/q2_net_download.hpp"
#include "shooters_edge/net/q2_net_server.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace gn = ge::net;
namespace sv = shooters_edge::server;
namespace cn = shooters_edge::net;

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
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
  br::Entity spot;
  spot.pairs = {{"classname", "info_player_deathmatch"}, {"origin", "0 0 24"}, {"angle", "0"}};
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
  gn::Transport st;
  std::vector<std::unique_ptr<gn::Transport>> ct;
  cn::Q2NetServer server;
  std::vector<std::unique_ptr<cn::Q2ClientEnd>> clients;
  int64_t now = 0;
};

std::unique_ptr<Rig> rig(const cn::Q2NetConfig& cfg) {
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
  CHECK_MSG(cn::q2_net_start(r->server, r->rules, r->st, cfg, error), error);
  return r;
}

cn::Q2ClientEnd& join(Rig& r, cn::Q2ClientConfig cfg) {
  const int n = static_cast<int>(r.clients.size());
  r.ct.push_back(std::make_unique<gn::Transport>());
  gn::open_sim_transport(*r.ct.back(), r.net,
                         gn::sim_address(uint32_t(n + 1), uint16_t(27901 + n)));
  r.clients.push_back(std::make_unique<cn::Q2ClientEnd>());
  cfg.qport = static_cast<uint16_t>(100 + n);
  cn::q2_client_start(*r.clients.back(), *r.ct.back(), gn::sim_address(0, cn::kPortQ2), cfg,
                      static_cast<double>(r.now) / 1e6);
  return *r.clients.back();
}

void run(Rig& r, int ticks) {
  for (int i = 0; i < ticks; i++) {
    r.now += 13000;
    gn::sim_advance(r.net, r.now);
    for (auto& c : r.clients) cn::q2_client_tick(*c, r.now);
    cn::q2_net_frame(r.server, r.now);
  }
}

std::vector<uint8_t> skin(size_t n) {
  std::vector<uint8_t> b(n);
  for (size_t i = 0; i < n; i++) b[i] = static_cast<uint8_t>(i * 7 + 3);
  return b;
}

cn::Q2NetConfig serving(const std::vector<uint8_t>& file) {
  cn::Q2NetConfig cfg;
  cfg.read_file = [file](const std::string& path, std::vector<uint8_t>* out) {
    if (path != "players/male/red.pcx") return false;
    *out = file;
    return true;
  };
  return cfg;
}

cn::Q2ClientConfig fetcher() {
  cn::Q2ClientConfig cc;
  cc.fetch = true;
  cc.want = {"players/male/red.pcx"};
  cc.read_file = [](const std::string& path) {
    return path.starts_with("maps/") ? std::vector<uint8_t>(1, 1) : std::vector<uint8_t>{};
  };
  return cc;
}

}  // namespace

TEST(pieces_fill_the_packet_and_the_file_comes_down_whole) {
  const std::vector<uint8_t> red = skin(5000);
  auto r = rig(serving(red));
  cn::Q2ClientEnd& c = join(*r, fetcher());
  run(*r, 400);
  CHECK_MSG(c.stage == cn::ClientStage::active, c.refused);
  CHECK(c.downloaded["players/male/red.pcx"] == red);
  // 5000 bytes: three pieces of 1386 and one of 842, where 1024-byte pieces took five.
  CHECK_MSG(r->server.clients[0].download_pieces == 4,
            std::to_string(r->server.clients[0].download_pieces) + " pieces");
  CHECK(r->server.clients[0].downloads == 1);
}

TEST(one_piece_is_owed_at_a_time_and_the_rate_spaces_them) {
  namespace cp = ge::classic::protocol;
  const std::vector<uint8_t> red = skin(20000);
  cn::Q2NetServer s;
  s.cfg = serving(red);
  cn::Q2NetClient c;
  c.name = "x";
  c.address = gn::sim_address(1, 27901);
  cp::channel_setup(c.ch, false, 100, 0);
  c.msglen = 1390;
  c.rate = 15000;
  CHECK(cn::q2_start_download(s, c, "players/male/red.pcx", 0).empty());
  // The first piece goes at once, filling the packet: 1386 bytes after its 4.
  CHECK(cn::q2_download_piece(s, c));
  CHECK(cp::channel_reliable_backlog(c.ch) == 1390 && c.download_at == 1386);
  // Nothing more without a `nextdl`; with one, not while the last is in flight.
  CHECK(!cn::q2_download_piece(s, c));
  c.download_owed = true;
  CHECK(!cn::q2_download_piece(s, c));
  // Acknowledged, it still waits out the pause: 1400 bytes at 15000 a second is 93 ms.
  cp::channel_setup(c.ch, false, 100, 0);
  CHECK(c.download_wait == 0.093);
  CHECK(!cn::q2_download_piece(s, c));
  s.now = 0.093;
  CHECK(cn::q2_download_piece(s, c));
  CHECK(c.download_pieces == 2 && !c.download_owed);
  // A longer packet a later client asked for, on the newer channel: a longer piece.
  cp::channel_setup(c.ch, false, 100, 0);
  c.ch.fragments = true;
  c.msglen = 4086;
  c.download_owed = true;
  s.now = 1;
  CHECK(cn::q2_download_piece(s, c));
  CHECK(c.download_at == 1386 * 2 + 4082);
}

TEST(a_resume_at_the_very_end_is_one_empty_piece_at_100) {
  namespace cp = ge::classic::protocol;
  const std::vector<uint8_t> red = skin(3000);
  cn::Q2NetServer s;
  s.cfg = serving(red);
  cn::Q2NetClient c;
  c.address = gn::sim_address(1, 27901);
  cp::channel_setup(c.ch, false, 100, 0);
  c.msglen = 1390;
  CHECK(cn::q2_start_download(s, c, "players/male/red.pcx", 3000).empty());
  CHECK(cn::q2_download_piece(s, c));
  CHECK(cp::channel_reliable_backlog(c.ch) == 4);
  CHECK(c.ch.pending[0].data[0] == 16 && c.ch.pending[0].data[1] == 0 &&
        c.ch.pending[0].data[3] == 100);
  CHECK(!c.downloading && c.downloads == 1);
}
