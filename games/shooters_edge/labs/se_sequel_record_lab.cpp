// LAB `se-sequel-record`: A MATCH ON THE SEQUEL'S RULES RECORDED AS THE SEQUEL'S OWN DEMO
// (games/shooters_edge/server/sequel_wire.hpp). Two players run and fire round a map on the
// sequel's rules (the demo pack's map with `--pak` and `--map`, else a test hall), recorded from
// the first's view through the server-side wire. The recording is attached twice: packed as the
// replay file (match.ged) and as the sequel's own client demo (match.dm2), the file a stock client
// of that game plays. The match is also broadcast as it is recorded (client/sequel_tv.hpp) through
// a TV node held 2 s, to a viewer there from the start and one who joins half way. It answers how
// many frames and bytes the match took, whether the sequel's reader walks every block, whether the
// replay file unpacks to the same bytes, and whether both viewers get the whole match.
//
// ge:summary Lab: a match on the sequel's rules recorded as .ged and .dm2, and watched through TV
// ge:keywords lab sequel record dm2 ged demo-pack wire stock-client signon frames
// ge:keywords tv-node broadcast late-viewer live-match
// ge:skill replay shooter labs
// ge:test tests/shooters_edge_sequel_wire_test.cpp
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/sequel_demo.hpp"
#include "shooters_edge/client/sequel_tv.hpp"
#include "shooters_edge/server/sequel_server.hpp"
#include "shooters_edge/server/sequel_wire.hpp"
#include "tv/node.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace cl = shooters_edge::client;
namespace tv = ge::tv;

// The recording's whole blocks from `at` on, each handed to the broadcast without its length; `at`
// moves past them. The end marker (length -1) is the broadcast's end, not a block.
void broadcast_new_blocks(const std::vector<uint8_t>& rec, size_t& at, cl::SequelBroadcast& b) {
  while (at + 4 <= rec.size()) {
    const uint32_t n = static_cast<uint32_t>(rec[at]) | static_cast<uint32_t>(rec[at + 1]) << 8 |
                       static_cast<uint32_t>(rec[at + 2]) << 16 |
                       static_cast<uint32_t>(rec[at + 3]) << 24;
    if (n == 0xffffffffu) {
      at += 4;
      return;
    }
    if (at + 4 + n > rec.size()) return;
    cl::sequel_broadcast_block(b, {rec.data() + at + 4, n});
    at += 4 + n;
  }
}

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
}

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// A walled hall with two deathmatch spots, as the sequel's map file.
std::vector<uint8_t> hall_file(std::string& error) {
  br::BrushWorld w;
  w.models.resize(1);
  auto& m = w.models[0];
  m.push_back(box(-512, -512, -16, 512, 512, 0));
  m.push_back(box(-528, -512, -16, -512, 512, 256));
  m.push_back(box(512, -512, -16, 528, 512, 256));
  m.push_back(box(-528, 512, -16, 528, 528, 256));
  m.push_back(box(-528, -528, -16, 528, -512, 256));
  w.entities.push_back(entity(
      {{"classname", "info_player_deathmatch"}, {"origin", "-400 -400 24"}, {"angle", "45"}}));
  w.entities.push_back(entity(
      {{"classname", "info_player_deathmatch"}, {"origin", "400 400 24"}, {"angle", "225"}}));
  std::vector<uint8_t> bytes;
  if (!br::write_brush_world(w, &bytes, &error)) return {};
  return bytes;
}

// The map's file: `maps/<map>.bsp` out of the pack, else the hall.
std::vector<uint8_t> map_file(gl::Lab& lab, const std::string& name, std::string& error) {
  const std::string& pak_path = lab.arg("pak");
  if (pak_path.empty()) return hall_file(error);
  std::ifstream in(pak_path, std::ios::binary);
  const std::vector<uint8_t> pak((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());
  ge::vfs::Pak p;
  const ge::vfs::PakEntry* e = nullptr;
  if (ge::vfs::read_pak(pak, &p, &error)) e = ge::vfs::pak_find(p, "maps/" + name + ".bsp");
  if (e == nullptr) {
    error = "no maps/" + name + ".bsp in " + pak_path;
    return {};
  }
  const std::span<const uint8_t> b = ge::vfs::pak_file(pak, *e);
  return {b.begin(), b.end()};
}

void run(gl::Lab& lab) {
  const std::string name = lab.arg("pak").empty() ? std::string("hall") : lab.arg("map");
  std::string error;
  const std::vector<uint8_t> file = map_file(lab, name, error);
  std::unique_ptr<cl::LoadedMap> map;
  if (!file.empty()) map = cl::map_from_bytes(file, name, error);
  if (!map || map->spec.brushes == nullptr)
    return lab.fail("the map " + name + " did not load on the sequel's rules: " + error,
                    "next: pass --pak=<the sequel demo's baseq2/pak0.pak> --map=demo1; skill: "
                    "brush.");
  sv::RulesRunner r = sv::sequel_runner(sv::make_sequel_server({}));
  r.console("deathmatch 1");
  if (!r.start(map->spec, error))
    return lab.fail("the sequel's rules did not start on " + name + ": " + error,
                    "next: run tests/shooters_edge_sequel_server_test.cpp; skill: shooter.");
  sv::SequelServer& s = *sv::sequel_of(r);
  r.connect(0, "eve", false, false);
  r.connect(1, "bob", false, false);
  sv::SequelWireOptions opt;
  opt.map_checksum = sv::sequel_map_checksum(map->file);
  sv::sequel_sky(*map->spec.brushes, opt);
  sv::SequelRecorder rec;
  sv::sequel_record_start(rec, *r, 0, opt);
  // The match broadcast as it is recorded, through a TV node held 2 s, to a viewer there from the
  // start and one who joins half way.
  cl::SequelBroadcast cast;
  cl::sequel_broadcast_begin(cast, 0);
  size_t cast_at = 0;
  tv::Node node;
  node.config.delay_ms = 2000;
  tv::Watch early, late;
  tv::Watch* watch[2] = {&early, &late};
  const tv::Send send = [&](int link, std::span<const uint8_t> msg) {
    return tv::watch_receive(*watch[link], msg);
  };
  bool fed = true;
  int64_t late_catch_up = -1;
  const int ticks = static_cast<int>(lab.arg_num("seconds") * 1000.0 / 13.0);
  uint32_t seq = 0;
  for (int t = 0; t < ticks; t++) {
    std::vector<sh::Packet> packets;
    for (int slot = 0; slot < 2; slot++) {
      sh::Packet p;
      p.client = slot;
      p.sequence = ++seq;
      sh::MoveCmd c;
      c.msec = 13;
      c.forward = 400;
      c.side = static_cast<int16_t>((t / 150) % 2 == 0 ? 200 : -200);
      c.angles = {0, static_cast<float>((t / 2) % 360) + 180.0f * static_cast<float>(slot), 0};
      if (t % 60 < 10) c.buttons = sh::kButtonAttack;
      p.cmds[0] = p.cmds[1] = p.cmds[2] = c;
      packets.push_back(p);
    }
    r.frame(packets.data(), static_cast<int>(packets.size()));
    sv::sequel_record_frame(rec, *r, static_cast<int32_t>(s.frame_number), &s.players[0].move);
    broadcast_new_blocks(rec.bytes, cast_at, cast);
    const int64_t now_ms = static_cast<int64_t>(t) * 13;
    const std::span<const uint8_t> fresh = cl::sequel_broadcast_fresh(cast);
    if (!fresh.empty()) fed = tv::tv_feed(node, fresh, now_ms) && fed;
    if (t == 0) {
      tv::tv_join(node, 0);
      tv::tv_receive(node, 0, tv::tv_hello("early", false));
    }
    if (t == ticks / 2) {
      late_catch_up = tv::tv_catch_up_blocks(node);
      tv::tv_join(node, 1);
      tv::tv_receive(node, 1, tv::tv_hello("late", false));
    }
    tv::tv_update(node, now_ms, send);
  }
  sv::sequel_record_stop(rec);
  broadcast_new_blocks(rec.bytes, cast_at, cast);
  cl::sequel_broadcast_end(cast);
  int64_t end_ms = static_cast<int64_t>(ticks) * 13;
  fed = tv::tv_feed(node, cl::sequel_broadcast_fresh(cast), end_ms) && fed;
  tv::tv_end(node);
  for (int k = 0; k < 400 && !(early.ended && late.ended); k++) {
    end_ms += 13;
    tv::tv_update(node, end_ms, send);
  }

  std::vector<uint8_t> packed, back;
  cl::DemoPackReport rep;
  if (!cl::pack_demo(rec.bytes, packed, error, {}, &rep))
    return lab.fail("the recording did not pack: " + error,
                    "next: run tests/shooters_edge_sequel_wire_test.cpp; skill: replay.");
  const bool round = cl::unpack_demo(packed, back, error) && back == rec.bytes;
  gl::LabTable t{"the match recorded", {"what", "value"}, {}};
  t.rows.push_back({"map", "maps/" + name + ".bsp"});
  t.rows.push_back({"map checksum", std::to_string(opt.map_checksum)});
  t.rows.push_back({"frames", std::to_string(rec.frames)});
  t.rows.push_back({"blocks walked / records",
                    std::to_string(rep.walked) + " / " + std::to_string(rep.records)});
  t.rows.push_back({".dm2 bytes", std::to_string(rec.bytes.size())});
  t.rows.push_back({".ged bytes", std::to_string(packed.size())});
  t.rows.push_back({".ged back to .dm2 exactly", round ? "yes" : "no"});
  int whole = 0;
  for (const tv::Watch* w : watch) {
    std::vector<uint8_t> got;
    std::string why;
    if (w->ended && cl::unpack_demo(w->stream, got, why) && got == rec.bytes) whole++;
  }
  t.rows.push_back({"broadcast: the TV node fed / its file's bytes",
                    std::string(fed ? "yes" : "no") + " / " + std::to_string(cast.out.size())});
  t.rows.push_back(
      {"the late viewer's catch-up at half time (blocks)", std::to_string(late_catch_up)});
  t.rows.push_back({"viewers given the whole match (of 2)", std::to_string(whole)});
  lab.table(t);
  lab.metric("record.frames", static_cast<double>(rec.frames), "frames");
  lab.metric("record.dm2_bytes", static_cast<double>(rec.bytes.size()), "bytes");
  lab.metric("record.ged_bytes", static_cast<double>(packed.size()), "bytes");
  lab.metric("record.walked", rep.walked == rep.records && rep.kept == 0 ? 1 : 0, "bool");
  lab.metric("record.round_trip", round ? 1 : 0, "bool");
  lab.metric("tv.viewers_whole", whole, "viewers");
  lab.attach("match.dm2", rec.bytes);
  lab.attach("match.ged", packed);
  if (rep.walked != rep.records || rep.kept != 0)
    lab.fail("the sequel's reader did not walk every block of the recording",
             "next: run tests/shooters_edge_sequel_wire_test.cpp; skill: replay.");
  if (whole != 2)
    lab.fail("a viewer of the match through the TV node did not get the whole recording",
             "next: run tests/shooters_edge_sequel_tv_test.cpp; skill: replay.");
  if (!round)
    lab.fail("the replay file did not unpack to the recording: " + error,
             "next: run tests/shooters_edge_sequel_wire_test.cpp; skill: replay.");
}

const gl::LabSpec kSpec{
    "se-sequel-record",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/server/sequel_wire: a match on the sequel's rules recorded as its demo",
    "replay",
    "Two players on the sequel's rules recorded through the server-side wire: how many frames and "
    "bytes, does the sequel's reader walk it whole, does the .ged give the .dm2 back, and do two "
    "viewers watching it live through a TV node get the whole match?",
    {
        {"pak", "", "the sequel demo's baseq2/pak0.pak (empty: a test hall)"},
        {"map", "demo1", "the map in the pack, without maps/ and .bsp"},
        {"seconds", "30", "the match's length"},
    },
    "--seconds=5",
    run,
};
GE_LAB(kSpec);

}  // namespace
