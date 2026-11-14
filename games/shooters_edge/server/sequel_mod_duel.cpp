// The sequel duel a mod stacks on (sequel_mod_duel.hpp).
//
// ge:summary The scripted sequel duel for mods: the hall, the two players, the frames
// ge:keywords sequel mod duel scripted-match hall fixture
// ge:skill mod shooter
// ge:test tests/shooters_edge_rules_mod_test.cpp
#include "shooters_edge/server/sequel_mod_duel.hpp"

#include <memory>
#include <utility>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "shooter/sequel_move.hpp"
#include "shooters_edge/server/sequel_server.hpp"
#include "shooters_edge/sim/mod_host.hpp"

namespace shooters_edge::server {

namespace {

namespace br = ge::brush;

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

}  // namespace

MapSpec sequel_duel_hall() {
  br::BrushWorld w;
  w.models.resize(1);
  const float floor_lo[3] = {-1024, -256, -16}, floor_hi[3] = {1024, 256, 0};
  const float east_lo[3] = {1024, -256, 0}, east_hi[3] = {1040, 256, 256};
  const float west_lo[3] = {-1040, -256, 0}, west_hi[3] = {-1024, 256, 256};
  w.models[0].push_back(br::box_brush(floor_lo, floor_hi, sh::kSqSolid, "world/floor"));
  w.models[0].push_back(br::box_brush(east_lo, east_hi, sh::kSqSolid, "world/wall"));
  w.models[0].push_back(br::box_brush(west_lo, west_hi, sh::kSqSolid, "world/wall"));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-300 0 24"}, {"angle", "0"}}));
  MapSpec spec;
  spec.name = "hall";
  std::vector<uint8_t> bytes;
  std::string error;
  auto map = std::make_shared<br::Map>();
  if (br::write_brush_world(w, &bytes, &error) && br::read_bsp(bytes, map.get(), &error))
    spec.brushes = map;
  return spec;
}

SequelDuel run_sequel_duel(const ge_mod_v1* table, int frames) {
  SequelDuel d;
  const RulesRunner r = sequel_runner(make_sequel_server({}));
  r.console("deathmatch 1");
  if (table != nullptr) {
    r->mods.tables.push_back(table);
    r->mods.names.push_back(table->name);
  }
  d.started = r.start(sequel_duel_hall(), d.error);
  if (!d.started) return d;
  r.connect(0, "Ana", false, false);
  r.connect(1, "Ben", false, false);
  SequelServer& s = *sequel_of(r);
  uint32_t sequence = 0;
  for (int t = 0; t < frames; t++) {
    if (t == 80) {  // both settled: Ana at -300, Ben at -100, still
      const float xs[2] = {-300, -100};
      for (int i = 0; i < 2; i++) {
        s.players[i].move.origin[0] = static_cast<int16_t>(xs[i] * 8);
        s.players[i].move.origin[1] = 0;
        for (int16_t& v : s.players[i].move.velocity) v = 0;
      }
    }
    sh::Packet packets[2];
    for (int c = 0; c < 2; c++) {
      packets[c].client = c;
      packets[c].sequence = ++sequence;
      sh::MoveCmd cmd;
      cmd.msec = 13;
      cmd.buttons = c == 0 && t >= 100 ? 1 : 0;
      packets[c].cmds[0] = packets[c].cmds[1] = packets[c].cmds[2] = cmd;
    }
    r.frame(packets, 2);
    d.hashes.push_back(rules_hash(*r));
    d.health.push_back(s.players[0].health);
  }
  d.obituaries = s.obituaries;
  for (const std::string& line : r->console)
    if (line.rfind("mod ", 0) == 0) d.mod_lines.push_back(line);
  if (!r->mods.hosts.empty()) {
    d.hook_calls = r->mods.hosts[0]->run.hook_calls;
    d.refusals = r->mods.hosts[0]->run.refusals;
  }
  return d;
}

}  // namespace shooters_edge::server
