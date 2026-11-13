// games/shooters_edge/server/sequel_wire.hpp: the sequel's runner on a test hall recorded as the
// sequel's own client recording. The signon opens with protocol 34's server data and names the
// map where the game looks for it; every frame deltas from the one before; the recording is one
// the sequel's reader walks whole, block by block, and it comes back byte for byte from the replay
// file.
//
// ge:summary Test: the sequel's runner recorded through the server-side wire as a .dm2 and a .ged
// ge:keywords test sequel wire record dm2 ged signon frame delta configstring walk round-trip
// ge:skill shooter replay testing
#include "shooters_edge/server/sequel_wire.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooter/locs.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/sequel_demo.hpp"
#include "shooters_edge/server/sequel_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace cl = shooters_edge::client;

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
  w.models.resize(2);
  auto& m = w.models[0];
  m.push_back(box(-512, -512, -16, 512, 512, 0));
  m.push_back(box(-528, -512, -16, -512, 512, 256));
  m.push_back(box(512, -512, -16, 528, 512, 256));
  m.push_back(box(-528, 512, -16, 528, 528, 256));
  m.push_back(box(-528, -528, -16, 528, -512, 256));
  w.models[1].push_back(box(256, 256, 0, 320, 320, 8));
  w.entities.push_back(entity({{"classname", "func_plat"}, {"model", "*1"}}));
  w.entities.push_back(entity(
      {{"classname", "info_player_deathmatch"}, {"origin", "-400 -400 24"}, {"angle", "45"}}));
  w.entities.push_back(entity(
      {{"classname", "info_player_deathmatch"}, {"origin", "400 400 24"}, {"angle", "225"}}));
  w.entities.push_back(entity({{"classname", "info_player_start"}, {"origin", "0 0 24"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  if (!map->entities.empty()) map->entities[0].pairs.emplace_back("message", "The Hall");
  return map;
}

sv::RulesRunner start() {
  sv::RulesRunner r = sv::sequel_runner(sv::make_sequel_server({}));
  r.console("deathmatch 1");
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall();
  std::string error;
  CHECK_MSG(r.start(map, error), error);
  return r;
}

sh::MoveCmd cmd(int16_t forward, float yaw) {
  sh::MoveCmd c;
  c.msec = 13;
  c.forward = forward;
  c.angles = {0, yaw, 0};
  return c;
}

// A match of two players recorded from the first's view for `ticks` host ticks of 13 ms.
std::vector<uint8_t> record(int ticks, sv::SequelRecorder* out = nullptr,
                            std::vector<std::string> stuffed = {}) {
  sv::RulesRunner r = start();
  sv::SequelServer& s = *sv::sequel_of(r);
  r.connect(0, "eve", false, false);
  r.connect(1, "bob", false, false);
  sv::SequelWireOptions opt;
  opt.map_checksum = 12345;
  opt.stuffed = std::move(stuffed);
  sv::SequelRecorder rec;
  sv::sequel_record_start(rec, *r, 0, opt);
  uint32_t seq = 0;
  for (int t = 0; t < ticks; t++) {
    std::vector<sh::Packet> packets;
    for (int slot = 0; slot < 2; slot++) {
      sh::Packet p;
      p.client = slot;
      p.sequence = ++seq;
      p.cmds[0] = p.cmds[1] = p.cmds[2] = cmd(400, static_cast<float>(t % 360 + 90 * slot));
      packets.push_back(p);
    }
    r.frame(packets.data(), static_cast<int>(packets.size()));
    sv::sequel_record_frame(rec, *r, static_cast<int32_t>(s.frame_number), &s.players[0].move);
  }
  sv::sequel_record_stop(rec);
  if (out) *out = rec;
  return rec.bytes;
}

int32_t long_at(std::span<const uint8_t> b, size_t at) {
  uint32_t v = 0;
  for (int k = 0; k < 4; k++) v |= static_cast<uint32_t>(b[at + size_t(k)]) << (8 * k);
  return static_cast<int32_t>(v);
}

bool has(std::span<const uint8_t> b, const std::string& text) {
  return std::search(b.begin(), b.end(), text.begin(), text.end()) != b.end();
}

}  // namespace

TEST(the_signon_opens_with_the_server_data_and_names_the_map) {
  const std::vector<uint8_t> rec = record(1);
  const std::vector<std::span<const uint8_t>> blocks = cl::sequel_blocks(rec);
  CHECK(!blocks.empty());
  CHECK(blocks[0][0] == 12 && long_at(blocks[0], 1) == sv::kSequelProtocol);
  CHECK(long_at(blocks[0], 5) == 0x10000 + 1);  // a client's recording: 0x10000 + the count
  CHECK(blocks[0][9] == 1);                     // attract loop: a recording
  bool map = false, checksum = false, title = false, precache = false;
  for (const auto& b : blocks) {
    map = map || has(b, "maps/hall.bsp");
    checksum = checksum || has(b, "12345");
    title = title || has(b, "The Hall");
    precache = precache || has(b, "precache\n");
    CHECK(b.size() <= size_t(sv::kSequelMaxBlock));
  }
  CHECK(map && checksum && title && precache);
}

TEST(every_frame_deltas_from_the_one_before) {
  const std::vector<uint8_t> rec = record(80);  // 1.04 s: ten rules frames
  const std::vector<std::span<const uint8_t>> blocks = cl::sequel_blocks(rec);
  int32_t last = -1;
  int frames = 0;
  for (const auto& b : blocks) {
    if (b[0] != 20) continue;  // the signon's blocks
    const int32_t number = long_at(b, 1), delta = long_at(b, 5);
    CHECK(delta == last);
    CHECK(number > last);
    last = number;
    frames++;
  }
  CHECK_MSG(frames >= 9, std::to_string(frames));
}

TEST(the_recording_walks_whole_and_comes_back_from_the_replay_file) {
  const std::vector<uint8_t> rec = record(400);  // 5.2 s of two players running round
  CHECK(cl::is_sequel_recording(rec));
  std::vector<uint8_t> file, back;
  std::string error;
  cl::DemoPackReport rep;
  CHECK_MSG(cl::pack_demo(rec, file, error, {}, &rep), error);
  CHECK(rep.format == "sequel");
  CHECK_MSG(rep.walked == rep.records && rep.kept == 0,
            std::to_string(rep.walked) + " of " + std::to_string(rep.records) + " walked");
  CHECK_MSG(cl::unpack_demo(file, back, error) && back == rec, error);
  // Nothing but the match decides the bytes.
  CHECK(record(400) == rec);
}

TEST(the_signon_carries_the_place_names_and_a_reader_takes_them) {
  sh::Locs places;
  sh::add_loc(places, {64, 32, 24}, "red armor");
  const std::vector<uint8_t> rec = record(20, nullptr, sh::recorded_loc_lines(places));
  bool line = false;
  for (const auto& b : cl::sequel_blocks(rec))
    line = line || has(b, "//loc  512  256  192 red armor\n");
  CHECK(line);
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(rec, d, error), error);
  CHECK(cl::next_frame(d, error) == cl::DemoRead::frame);
  CHECK(d.state.places.points.size() == 1 && d.state.places.points[0].name == "red armor");
}

TEST(the_map_checksum_is_md4s_words_xored) {
  // RFC 1320's "abc", whose digest is a448017a af21d852 5fc10ae8 7aa6729d.
  const std::string abc = "abc";
  const std::span<const uint8_t> bytes(reinterpret_cast<const uint8_t*>(abc.data()), abc.size());
  CHECK(sv::sequel_map_checksum(bytes) == 1570836014);
}
