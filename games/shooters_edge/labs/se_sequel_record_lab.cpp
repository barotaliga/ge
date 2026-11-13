// LAB `se-sequel-record`: A MATCH ON THE SEQUEL'S RULES RECORDED AS THE SEQUEL'S OWN DEMO
// (games/shooters_edge/server/sequel_wire.hpp). Two players run and fire round a map on the
// sequel's rules (the demo pack's map with `--pak` and `--map`, else a test hall), recorded from
// the first's view through the server-side wire. The recording is attached twice: packed as the
// replay file (match.ged) and as the sequel's own client demo (match.dm2), the file a stock client
// of that game plays. It answers how many frames and bytes the match took, whether the sequel's
// reader walks every block, and whether the replay file unpacks to the same bytes.
//
// ge:summary Lab: a match on the sequel's rules recorded as a .ged and as the sequel's own .dm2
// ge:keywords lab sequel record dm2 ged demo-pack wire stock-client signon frames
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
#include "shooters_edge/server/sequel_server.hpp"
#include "shooters_edge/server/sequel_wire.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
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
  }
  sv::sequel_record_stop(rec);

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
  lab.table(t);
  lab.metric("record.frames", static_cast<double>(rec.frames), "frames");
  lab.metric("record.dm2_bytes", static_cast<double>(rec.bytes.size()), "bytes");
  lab.metric("record.ged_bytes", static_cast<double>(packed.size()), "bytes");
  lab.metric("record.walked", rep.walked == rep.records && rep.kept == 0 ? 1 : 0, "bool");
  lab.metric("record.round_trip", round ? 1 : 0, "bool");
  lab.attach("match.dm2", rec.bytes);
  lab.attach("match.ged", packed);
  if (rep.walked != rep.records || rep.kept != 0)
    lab.fail("the sequel's reader did not walk every block of the recording",
             "next: run tests/shooters_edge_sequel_wire_test.cpp; skill: replay.");
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
    "bytes, does the sequel's reader walk it whole, and does the .ged give the .dm2 back?",
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
