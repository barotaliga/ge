// games/shooters_edge/server/sequel_wire.hpp: the sequel's runner on a test hall recorded as the
// sequel's own client recording. The signon opens with protocol 34's server data and names the
// map where the game looks for it; every frame deltas from the one before; the recording is one
// the sequel's reader walks whole, block by block, and it comes back byte for byte from the replay
// file; broadcast while it is recorded, it reaches a TV node's viewers whole, a late one too.
//
// ge:summary Test: the sequel's runner recorded through the server-side wire as a .dm2 and a .ged
// ge:keywords test sequel wire record dm2 ged signon frame delta configstring walk round-trip
// ge:keywords tv-node broadcast live-match late-viewer
// ge:skill shooter replay testing
#include "shooters_edge/server/sequel_wire.hpp"

#include <algorithm>
#include <cstring>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classic/locs.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/sequel_bar.hpp"
#include "shooters_edge/client/sequel_demo.hpp"
#include "shooters_edge/client/sequel_tv.hpp"
#include "shooters_edge/server/sequel_server.hpp"
#include "tv/node.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace cl = shooters_edge::client;
namespace tv = ge::tv;

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

// A match of two players recorded from the first's view for `ticks` host ticks of 13 ms;
// `each_tick` sees the recording after every tick; `stuffed` ends the signon.
std::vector<uint8_t> record(
    int ticks, sv::SequelRecorder* out = nullptr,
    const std::function<void(const sv::SequelRecorder&, int)>& each_tick = nullptr,
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
    if (each_tick) each_tick(rec, t);
  }
  sv::sequel_record_stop(rec);
  if (each_tick) each_tick(rec, ticks);
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
  ge::classic::Locs places;
  ge::classic::add_loc(places, {64, 32, 24}, "red armor");
  const std::vector<uint8_t> rec =
      record(20, nullptr, nullptr, ge::classic::recorded_loc_lines(places));
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

TEST(a_recording_draws_the_games_own_bar_as_the_game_did_and_its_screen) {
  // A sequel recording (and a sequel server joined, read the same way) shows the sequel's own
  // status bar from its program, never the window's HUD: the same pictures and numbers as the
  // game drew for that player, the scoreboard over it while it was up.
  sv::RulesRunner r = start();
  sv::SequelServer& s = *sv::sequel_of(r);
  r.connect(0, "eve", false, false);
  r.connect(1, "bob", false, false);
  sv::SequelRecorder rec;
  sv::sequel_record_start(rec, *r, 0, {});
  uint32_t seq = 0;
  for (int t = 0; t < 60; t++) {
    if (t == 40) CHECK(r.client_command(0, "score") == sv::CommandResult::mod);
    std::vector<sh::Packet> packets(2);
    for (int slot = 0; slot < 2; slot++) {
      packets[size_t(slot)].client = slot;
      packets[size_t(slot)].sequence = ++seq;
      packets[size_t(slot)].cmds[0] = packets[size_t(slot)].cmds[1] =
          packets[size_t(slot)].cmds[2] = cmd(0, 0);
    }
    r.frame(packets.data(), 2);
    sv::sequel_record_frame(rec, *r, static_cast<int32_t>(s.frame_number), &s.players[0].move);
  }
  sv::sequel_record_stop(rec);
  cl::SequelBar live;
  cl::run_sequel_bar(live, r->world, 0, 0, 640, 480);
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(rec.bytes, d, error), error);
  while (cl::next_frame(d, error) == cl::DemoRead::frame) {
  }
  sv::RulesWorld seen;
  CHECK(cl::sequel_world_of(d.state, seen));
  cl::SequelBar played;
  cl::run_sequel_bar(played, seen, d.state.player, 0, 640, 480);
  CHECK_MSG(played.ops.size() == live.ops.size() && !live.ops.empty(),
            std::to_string(played.ops.size()) + " vs " + std::to_string(live.ops.size()));
  bool same = played.ops.size() == live.ops.size();
  for (size_t i = 0; same && i < live.ops.size(); i++)
    same = played.ops[i].x == live.ops[i].x && played.ops[i].y == live.ops[i].y &&
           played.ops[i].name == live.ops[i].name && played.ops[i].text == live.ops[i].text;
  CHECK(same);
  CHECK(std::any_of(played.ops.begin(), played.ops.end(),
                    [](const auto& op) { return op.text == "eve"; }));  // the scoreboard is up
  // A first-game recording has no such bar: the window's own HUD is drawn for it.
  cl::DemoState first;
  first.protocol = 15;
  first.status_bar = "xl 0";
  CHECK(!cl::sequel_world_of(first, seen));
}

TEST(the_map_checksum_is_md4s_words_xored) {
  // RFC 1320's "abc", whose digest is a448017a af21d852 5fc10ae8 7aa6729d.
  const std::string abc = "abc";
  const std::span<const uint8_t> bytes(reinterpret_cast<const uint8_t*>(abc.data()), abc.size());
  CHECK(sv::sequel_map_checksum(bytes) == 1570836014);
}

TEST(a_match_broadcast_as_it_is_recorded_reaches_every_viewer_whole) {
  cl::SequelBroadcast cast;
  cl::sequel_broadcast_begin(cast, 0);
  size_t at = 0;
  tv::Node node;
  node.config.delay_ms = 2000;
  tv::Watch early, late;
  tv::Watch* watch[2] = {&early, &late};
  const tv::Send send = [&](int link, std::span<const uint8_t> msg) {
    return tv::watch_receive(*watch[link], msg);
  };
  const int ticks = 800;
  int64_t late_catch_up = -1;
  bool ended = false;
  const auto each_tick = [&](const sv::SequelRecorder& rec, int t) {
    // The recording's new whole blocks, each without its length, until the end marker.
    while (!ended && at + 4 <= rec.bytes.size()) {
      const auto n = static_cast<uint32_t>(long_at(rec.bytes, at));
      at += 4;
      if (n == 0xffffffffu) {
        ended = true;
        break;
      }
      cl::sequel_broadcast_block(cast, {rec.bytes.data() + at, n});
      at += n;
    }
    if (ended) cl::sequel_broadcast_end(cast);
    const int64_t now = static_cast<int64_t>(t) * 13;
    const std::span<const uint8_t> fresh = cl::sequel_broadcast_fresh(cast);
    if (!fresh.empty()) CHECK(tv::tv_feed(node, fresh, now));
    if (t == 0) {
      tv::tv_join(node, 0);
      CHECK(tv::tv_receive(node, 0, tv::tv_hello("early", false)));
    }
    if (t == ticks / 2) {
      late_catch_up = tv::tv_catch_up_blocks(node);
      tv::tv_join(node, 1);
      CHECK(tv::tv_receive(node, 1, tv::tv_hello("late", false)));
    }
    if (ended) tv::tv_end(node);
    tv::tv_update(node, now, send);
  };
  const std::vector<uint8_t> rec = record(ticks, nullptr, each_tick);
  CHECK(ended);
  for (int64_t now = int64_t{ticks} * 13; now < int64_t{ticks} * 13 + 5000; now += 13)
    tv::tv_update(node, now, send);
  CHECK(late_catch_up > 0);  // the late viewer is sent the match from its start
  std::string error;
  for (const tv::Watch* w : watch) {
    CHECK(w->ended);
    std::vector<uint8_t> back;
    CHECK_MSG(cl::unpack_demo(w->stream, back, error), error);
    CHECK(back == rec);
  }
}

// An item that shows again carries the entity event 1 on that rules frame alone, so a viewer
// throws its glow once (the sequel's item back), and never while it waits or after.
TEST(an_item_back_carries_its_entity_event_for_one_frame) {
  auto map = std::const_pointer_cast<br::Map>(hall());
  br::Entity item;
  item.pairs = {{"classname", "item_health"}, {"origin", "0 200 24"}};
  map->entities.push_back(item);
  sv::RulesRunner r = sv::sequel_runner(sv::make_sequel_server({}));
  r.console("deathmatch 1");
  sv::MapSpec spec;
  spec.name = "hall";
  spec.brushes = map;
  std::string error;
  CHECK_MSG(r.start(spec, error), error);
  r.connect(0, "eve", false, false);
  sv::SequelServer& s = *sv::sequel_of(r);
  int at = -1;
  for (size_t i = 0; i < s.entities.size(); i++)
    if (s.entities[i].item >= 0) at = static_cast<int>(i);
  CHECK(at >= 0);
  if (at < 0) return;
  const int number = 1 + sh::kMaxClients + at;
  s.entities[size_t(at)].hidden = true;  // as if taken: back in half a second
  s.entities[size_t(at)].respawn_at = s.level_time + 0.5;
  int64_t evented = -1;
  int frames_with_event = 0;
  float was_in = 1;  // the rules' world counts its wait down to the frame it shows (back_in)
  bool counted = false;
  for (int t = 0; t < 120; t++) {
    sh::Packet p;
    p.client = 0;
    p.sequence = uint32_t(t + 1);
    p.cmds[0] = p.cmds[1] = p.cmds[2] = cmd(0, 0);
    r.frame(&p, 1);
    const sh::WorldEntity& row = r->world.entities[size_t(number)];
    if (s.entities[size_t(at)].hidden) {
      CHECK(row.back_in > 0 && row.back_in <= was_in);
      CHECK(row.back_model == s.entities[size_t(at)].modelindex && row.back_model > 0);
      was_in = row.back_in;
      counted = true;
    } else {
      CHECK(row.back_in == -1.0f && row.back_model == 0);
    }
    const sv::SequelEntityState e = sv::sequel_entity_state(*r, number);
    if (e.event == 1 && evented != s.frame_number) {
      evented = s.frame_number;
      frames_with_event++;
      CHECK(e.present);
    }
  }
  CHECK_MSG(frames_with_event == 1, std::to_string(frames_with_event));
  CHECK(!s.entities[size_t(at)].hidden);
  CHECK(counted);
}

// A teleporter's pad and its exit are drawn as the disc a player walks to: the pad with its
// specks rising (skin 1), the exit plain (skin 0), both the same model.
TEST(a_teleporters_pad_and_exit_are_drawn) {
  auto map = std::const_pointer_cast<br::Map>(hall());
  br::Entity pad, exit;
  pad.pairs = {{"classname", "misc_teleporter"}, {"origin", "0 -200 24"}, {"target", "t1"}};
  exit.pairs = {
      {"classname", "misc_teleporter_dest"}, {"origin", "0 300 24"}, {"targetname", "t1"}};
  map->entities.push_back(pad);
  map->entities.push_back(exit);
  sv::RulesRunner r = sv::sequel_runner(sv::make_sequel_server({}));
  r.console("deathmatch 1");
  sv::MapSpec spec;
  spec.name = "hall";
  spec.brushes = map;
  std::string error;
  CHECK_MSG(r.start(spec, error), error);
  r.connect(0, "eve", false, false);
  sh::Packet p;
  p.client = 0;
  p.sequence = 1;
  p.cmds[0] = p.cmds[1] = p.cmds[2] = cmd(0, 0);
  r.frame(&p, 1);
  sv::SequelServer& s = *sv::sequel_of(r);
  int pad_at = -1, exit_at = -1;
  for (size_t i = 0; i < s.entities.size(); i++) {
    if (s.entities[i].classname == "misc_teleporter") pad_at = static_cast<int>(i);
    if (s.entities[i].classname == "misc_teleporter_dest") exit_at = static_cast<int>(i);
  }
  CHECK(pad_at >= 0 && exit_at >= 0);
  if (pad_at < 0 || exit_at < 0) return;
  const sv::SequelEntityState a = sv::sequel_entity_state(*r, 1 + sh::kMaxClients + pad_at);
  const sv::SequelEntityState b = sv::sequel_entity_state(*r, 1 + sh::kMaxClients + exit_at);
  CHECK(a.present && b.present);
  CHECK(a.model[0] != 0 && a.model[0] == b.model[0]);
  CHECK(a.skin == 1 && b.skin == 0);
  CHECK(a.effects == 131072 && b.effects == 0);
}

// The rules' named effects go out as the protocol's temporary entities, so a stock client (and a
// recording) draws the blasts, the blood, the rail's line and the teleports; a name the protocol
// has none for goes nowhere.
TEST(the_rules_effects_go_out_as_temporary_entities) {
  sv::RulesRunner r = start();
  sv::SequelServer& s = *sv::sequel_of(r);
  r.connect(0, "eve", false, false);
  sv::SequelRecorder rec;
  sv::sequel_record_start(rec, *r, 0, {});
  const auto throw_effect = [&r](sv::EventKind kind, const char* name, sh::Vec3f at,
                                 sh::Vec3f end) {
    sv::NetEvent ev;
    ev.kind = kind;
    ev.to = 0xFFFFFFFFu;
    ev.origin = at;
    ev.values[0] = end.x;
    ev.values[1] = end.y;
    ev.values[2] = end.z;
    ev.text = name;
    r->out.events.push_back(ev);
    r->out.events_total++;
  };
  uint32_t seq = 0;
  for (int t = 0; t < 40; t++) {
    sh::Packet p;
    p.client = 0;
    p.sequence = ++seq;
    p.cmds[0] = p.cmds[1] = p.cmds[2] = cmd(0, 0);
    r.frame(&p, 1);
    if (t == 20) {
      throw_effect(sv::EventKind::point_effect, "te_explosion", {100, 50, 30}, {});
      throw_effect(sv::EventKind::point_effect, "te_teleport", {-64, 8, 24}, {});
      throw_effect(sv::EventKind::point_effect, "te_blood", {10, 20, 30}, {0, 0, 1});
      throw_effect(sv::EventKind::trail_effect, "te_railtrail", {0, 0, 40}, {300, 0, 40});
      throw_effect(sv::EventKind::point_effect, "te_no_such_thing", {0, 0, 0}, {});
    }
    sv::sequel_record_frame(rec, *r, static_cast<int32_t>(s.frame_number), &s.players[0].move);
  }
  sv::sequel_record_stop(rec);
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(rec.bytes, d, error), error);
  std::vector<cl::DemoEffect> seen;
  while (cl::next_frame(d, error) == cl::DemoRead::frame)
    seen.insert(seen.end(), d.state.effects.begin(), d.state.effects.end());
  CHECK_MSG(seen.size() == 4, std::to_string(seen.size()));
  if (seen.size() != 4) return;
  CHECK(seen[0].kind == cl::DemoEffectKind::explosion && seen[0].code == 7);
  CHECK(seen[0].at.x == 100 && seen[0].at.y == 50 && seen[0].at.z == 30);
  CHECK(seen[1].kind == cl::DemoEffectKind::named && seen[1].name == "sq_teleport");
  CHECK(seen[2].kind == cl::DemoEffectKind::blood);
  CHECK(seen[2].way.z > 0.99f);  // its way, by the direction table
  CHECK(seen[3].kind == cl::DemoEffectKind::beam && seen[3].end.x == 300);
}
