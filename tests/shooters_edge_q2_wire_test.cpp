// games/shooters_edge/server/q2_wire.hpp: Quake II's runner on a test hall recorded as the
// Quake II's own client recording. The signon opens with protocol 34's server data and names the
// map where the game looks for it; every frame deltas from the one before; the recording is one
// Quake II's reader walks whole, block by block, and it comes back byte for byte from the replay
// file; broadcast while it is recorded, it reaches a TV node's viewers whole, a late one too.
//
// ge:summary Test: Quake II's runner recorded through the server-side wire as a .dm2 and a .ged
// ge:keywords test q2 wire record dm2 ged signon frame delta configstring walk round-trip
// ge:keywords tv-node broadcast live-match late-viewer
// ge:skill shooter replay testing
#include "shooters_edge/server/q2_wire.hpp"

#include <algorithm>
#include <cstring>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "brush/areas.hpp"
#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classic/locs.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/q2_bar.hpp"
#include "shooters_edge/client/q2_demo.hpp"
#include "shooters_edge/client/q2_tv.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_server.hpp"
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
  sv::RulesRunner r = sv::q2_runner(sv::make_q2_server({}));
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
    int ticks, sv::Q2Recorder* out = nullptr,
    const std::function<void(const sv::Q2Recorder&, int)>& each_tick = nullptr,
    std::vector<std::string> stuffed = {}) {
  sv::RulesRunner r = start();
  sv::Q2Server& s = *sv::q2_of(r);
  r.connect(0, "eve", false, false);
  r.connect(1, "bob", false, false);
  sv::Q2WireOptions opt;
  opt.map_checksum = 12345;
  opt.stuffed = std::move(stuffed);
  sv::Q2Recorder rec;
  sv::q2_record_start(rec, *r, 0, opt);
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
    sv::q2_record_frame(rec, *r, static_cast<int32_t>(s.frame_number), &s.players[0].move);
    if (each_tick) each_tick(rec, t);
  }
  sv::q2_record_stop(rec);
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
  const std::vector<std::span<const uint8_t>> blocks = cl::q2_blocks(rec);
  CHECK(!blocks.empty());
  CHECK(blocks[0][0] == 12 && long_at(blocks[0], 1) == sv::kQ2Protocol);
  CHECK(long_at(blocks[0], 5) == 0x10000 + 1);  // a client's recording: 0x10000 + the count
  CHECK(blocks[0][9] == 1);                     // attract loop: a recording
  bool map = false, checksum = false, title = false, precache = false;
  for (const auto& b : blocks) {
    map = map || has(b, "maps/hall.bsp");
    checksum = checksum || has(b, "12345");
    title = title || has(b, "The Hall");
    precache = precache || has(b, "precache\n");
    CHECK(b.size() <= size_t(sv::kQ2MaxBlock));
  }
  CHECK(map && checksum && title && precache);
}

TEST(every_frame_deltas_from_the_one_before) {
  const std::vector<uint8_t> rec = record(80);  // 1.04 s: ten rules frames
  const std::vector<std::span<const uint8_t>> blocks = cl::q2_blocks(rec);
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
  CHECK(cl::is_q2_recording(rec));
  std::vector<uint8_t> file, back;
  std::string error;
  cl::DemoPackReport rep;
  CHECK_MSG(cl::pack_demo(rec, file, error, {}, &rep), error);
  CHECK(rep.format == "q2");
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
  for (const auto& b : cl::q2_blocks(rec))
    line = line || has(b, "//loc  512  256  192 red armor\n");
  CHECK(line);
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(rec, d, error), error);
  CHECK(cl::next_frame(d, error) == cl::DemoRead::frame);
  CHECK(d.state.places.points.size() == 1 && d.state.places.points[0].name == "red armor");
}

TEST(a_recording_draws_the_games_own_bar_as_the_game_did_and_its_screen) {
  // A Quake II recording (and a Quake II server joined, read the same way) shows Quake II's own
  // status bar from its program, never the window's HUD: the same pictures and numbers as the
  // game drew for that player, the scoreboard over it while it was up.
  sv::RulesRunner r = start();
  sv::Q2Server& s = *sv::q2_of(r);
  r.connect(0, "eve", false, false);
  r.connect(1, "bob", false, false);
  sv::Q2Recorder rec;
  sv::q2_record_start(rec, *r, 0, {});
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
    sv::q2_record_frame(rec, *r, static_cast<int32_t>(s.frame_number), &s.players[0].move);
  }
  sv::q2_record_stop(rec);
  cl::Q2Bar live;
  cl::run_q2_bar(live, r->world, 0, 0, 640, 480);
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(rec.bytes, d, error), error);
  while (cl::next_frame(d, error) == cl::DemoRead::frame) {
  }
  sv::RulesWorld seen;
  CHECK(cl::q2_world_of(d.state, seen));
  cl::Q2Bar played;
  cl::run_q2_bar(played, seen, d.state.player, 0, 640, 480);
  CHECK_MSG(played.ops.size() == live.ops.size() && !live.ops.empty(),
            std::to_string(played.ops.size()) + " vs " + std::to_string(live.ops.size()));
  bool same = played.ops.size() == live.ops.size();
  for (size_t i = 0; same && i < live.ops.size(); i++)
    same = played.ops[i].x == live.ops[i].x && played.ops[i].y == live.ops[i].y &&
           played.ops[i].name == live.ops[i].name && played.ops[i].text == live.ops[i].text;
  CHECK(same);
  CHECK(std::any_of(played.ops.begin(), played.ops.end(),
                    [](const auto& op) { return op.text == "eve"; }));  // the scoreboard is up
  // A Quake recording has no such bar: the window's own HUD is drawn for it.
  cl::DemoState first;
  first.protocol = 15;
  first.status_bar = "xl 0";
  CHECK(!cl::q2_world_of(first, seen));
}

TEST(the_map_checksum_is_md4s_words_xored) {
  // RFC 1320's "abc", whose digest is a448017a af21d852 5fc10ae8 7aa6729d.
  const std::string abc = "abc";
  const std::span<const uint8_t> bytes(reinterpret_cast<const uint8_t*>(abc.data()), abc.size());
  CHECK(sv::q2_map_checksum(bytes) == 1570836014);
}

TEST(a_match_broadcast_as_it_is_recorded_reaches_every_viewer_whole) {
  cl::Q2Broadcast cast;
  cl::q2_broadcast_begin(cast, 0);
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
  const auto each_tick = [&](const sv::Q2Recorder& rec, int t) {
    // The recording's new whole blocks, each without its length, until the end marker.
    while (!ended && at + 4 <= rec.bytes.size()) {
      const auto n = static_cast<uint32_t>(long_at(rec.bytes, at));
      at += 4;
      if (n == 0xffffffffu) {
        ended = true;
        break;
      }
      cl::q2_broadcast_block(cast, {rec.bytes.data() + at, n});
      at += n;
    }
    if (ended) cl::q2_broadcast_end(cast);
    const int64_t now = static_cast<int64_t>(t) * 13;
    const std::span<const uint8_t> fresh = cl::q2_broadcast_fresh(cast);
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
// throws its glow once (Quake II's item back), and never while it waits or after.
TEST(an_item_back_carries_its_entity_event_for_one_frame) {
  auto map = std::const_pointer_cast<br::Map>(hall());
  br::Entity item;
  item.pairs = {{"classname", "item_health"}, {"origin", "0 200 24"}};
  map->entities.push_back(item);
  sv::RulesRunner r = sv::q2_runner(sv::make_q2_server({}));
  r.console("deathmatch 1");
  sv::MapSpec spec;
  spec.name = "hall";
  spec.brushes = map;
  std::string error;
  CHECK_MSG(r.start(spec, error), error);
  r.connect(0, "eve", false, false);
  sv::Q2Server& s = *sv::q2_of(r);
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
    const sv::Q2EntityState e = sv::q2_entity_state(*r, number);
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
  sv::RulesRunner r = sv::q2_runner(sv::make_q2_server({}));
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
  sv::Q2Server& s = *sv::q2_of(r);
  int pad_at = -1, exit_at = -1;
  for (size_t i = 0; i < s.entities.size(); i++) {
    if (s.entities[i].classname == "misc_teleporter") pad_at = static_cast<int>(i);
    if (s.entities[i].classname == "misc_teleporter_dest") exit_at = static_cast<int>(i);
  }
  CHECK(pad_at >= 0 && exit_at >= 0);
  if (pad_at < 0 || exit_at < 0) return;
  const sv::Q2EntityState a = sv::q2_entity_state(*r, 1 + sh::kMaxClients + pad_at);
  const sv::Q2EntityState b = sv::q2_entity_state(*r, 1 + sh::kMaxClients + exit_at);
  CHECK(a.present && b.present);
  CHECK(a.model[0] != 0 && a.model[0] == b.model[0]);
  CHECK(a.skin == 1 && b.skin == 0);
  CHECK(a.effects == 131072 && b.effects == 0);
  // The pad hums; its exit is quiet.
  CHECK(a.sound > 0 && size_t(a.sound) < s.sounds.size() && s.sounds[a.sound] == "world/amb10.wav");
  CHECK(b.sound == 0);
  // Both discs are solid: a client predicts against the box it is sent (half width 32, 24 down,
  // 16 up from 32 below), and a player dropped onto either stands on its top, 16 below its origin.
  const int disc = (2 << 10) | (3 << 5) | 4;
  CHECK_MSG(a.solid == disc && b.solid == disc,
            std::to_string(a.solid) + " " + std::to_string(b.solid));
  for (const float y : {-200.0f, 300.0f}) {
    const ge::classic::Q2Trace t = sv::map_trace(s, {0, y, 100}, {-16, -16, -24}, {16, 16, 32},
                                                 {0, y, -50}, sh::kSqMaskPlayerSolid);
    CHECK_MSG(t.fraction < 1 && t.end.z == 32 && t.normal.z == 1, std::to_string(t.end.z));
  }
  // A shot traced past the disc's side goes by.
  const ge::classic::Q2Trace by =
      sv::map_trace(s, {100, -200, 30}, {}, {}, {-100, -200, 30}, sh::kSqMaskPlayerSolid);
  CHECK(by.fraction == 1);
}

// The loops ride their entities on the wire, as a stock client hears them: a looping speaker
// (sent for its loop though it has no model), a rocket's roar in flight, the railgun's hum while a
// player holds it; a speaker that plays once sends nothing.
TEST(the_loops_ride_their_entities_on_the_wire) {
  auto map = std::const_pointer_cast<br::Map>(hall());
  br::Entity on, once;
  on.pairs = {{"classname", "target_speaker"},
              {"origin", "0 100 24"},
              {"noise", "world/amb5"},
              {"spawnflags", "1"}};
  once.pairs = {{"classname", "target_speaker"}, {"origin", "0 120 24"}, {"noise", "world/x"}};
  map->entities.push_back(on);
  map->entities.push_back(once);
  sv::RulesRunner r = sv::q2_runner(sv::make_q2_server({}));
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
  sv::Q2Server& s = *sv::q2_of(r);
  const auto index = [&s](const char* name) {
    const auto at = std::find(s.sounds.begin(), s.sounds.end(), name);
    return at == s.sounds.end() ? -1 : static_cast<int>(at - s.sounds.begin());
  };
  int on_at = -1, once_at = -1;
  for (size_t i = 0; i < s.entities.size(); i++)
    if (s.entities[i].classname == "target_speaker")
      (s.entities[i].noise == "world/amb5.wav" ? on_at : once_at) = static_cast<int>(i);
  CHECK(on_at >= 0 && once_at >= 0);
  if (on_at < 0 || once_at < 0) return;
  const sv::Q2EntityState a = sv::q2_entity_state(*r, 1 + sh::kMaxClients + on_at);
  const sv::Q2EntityState b = sv::q2_entity_state(*r, 1 + sh::kMaxClients + once_at);
  CHECK(a.present && a.model[0] == 0 && a.sound == index("world/amb5.wav"));
  CHECK(!b.present);
  // A rocket in flight and the railgun in hand.
  s.players[0].hand.weapon = int(sv::SqWeapon::railgun);
  sv::Q2Missile& m = s.missiles[0];
  m.live = true;
  m.kind = sv::SqShot::rocket;
  m.owner = 0;
  m.origin = {0, -100, 64};
  m.velocity = {0, 650, 0};
  m.dies_at = s.level_time + 10;
  p.sequence = 2;
  r.frame(&p, 1);
  CHECK(sv::q2_entity_state(*r, 1).sound == index("weapons/rg_hum.wav"));
  bool roars = false;
  for (int n = 1; n < s.world.entity_count && n < sv::kQ2Entities; n++)
    roars = roars || sv::q2_entity_state(*r, n).sound == index("weapons/rockfly.wav");
  CHECK(roars);
}

// The rules' named effects go out as the protocol's temporary entities, so a stock client (and a
// recording) draws the blasts, the blood, the rail's line and the teleports; a name the protocol
// has none for goes nowhere.
TEST(the_rules_effects_go_out_as_temporary_entities) {
  sv::RulesRunner r = start();
  sv::Q2Server& s = *sv::q2_of(r);
  r.connect(0, "eve", false, false);
  sv::Q2Recorder rec;
  sv::q2_record_start(rec, *r, 0, {});
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
    sv::q2_record_frame(rec, *r, static_cast<int32_t>(s.frame_number), &s.players[0].move);
  }
  sv::q2_record_stop(rec);
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(rec.bytes, d, error), error);
  std::vector<cl::DemoEffect> seen;
  bool blast_heard = false;  // the client plays an explosion's sound itself
  while (cl::next_frame(d, error) == cl::DemoRead::frame) {
    seen.insert(seen.end(), d.state.effects.begin(), d.state.effects.end());
    for (const cl::DemoSound& h : d.state.started)
      blast_heard =
          blast_heard || (h.name != nullptr && std::string_view(h.name) == "weapons/rocklx1a.wav");
  }
  CHECK(blast_heard);
  CHECK(!d.state.light_styles[0].empty());  // the map's styles, kept for the window
  CHECK_MSG(seen.size() == 4, std::to_string(seen.size()));
  if (seen.size() != 4) return;
  CHECK(seen[0].kind == cl::DemoEffectKind::explosion && seen[0].code == 7);
  CHECK(seen[0].at.x == 100 && seen[0].at.y == 50 && seen[0].at.z == 30);
  CHECK(seen[1].kind == cl::DemoEffectKind::named && seen[1].name == "sq_teleport");
  CHECK(seen[2].kind == cl::DemoEffectKind::blood);
  CHECK(seen[2].way.z > 0.99f);                              // its way, by the direction table
  CHECK(seen[2].q2_specks == 60 && seen[2].colour == 0xe8);  // the client's puff of blood
  // The rail's line, drawn by its name as our own game draws it (world_fx.cpp).
  CHECK(seen[3].kind == cl::DemoEffectKind::named_trail && seen[3].name == "te_railtrail");
  CHECK(seen[3].end.x == 300);
}

// A shot that strikes a wall throws its sparks off the wall: the temporary entity carries the
// surface's normal, so a client throws them back into the room, not along one fixed way.
TEST(a_shot_s_sparks_leave_the_wall_it_strikes) {
  sv::RulesRunner r = start();
  sv::Q2Server& s = *sv::q2_of(r);
  r.connect(0, "eve", false, false);
  sv::Q2Recorder rec;
  sv::q2_record_start(rec, *r, 0, {});
  uint32_t seq = 0;
  for (int t = 0; t < 120; t++) {
    sh::Packet p;
    p.client = 0;
    p.sequence = ++seq;
    p.cmds[0] = p.cmds[1] = p.cmds[2] = cmd(0, 180);  // facing a wall square on
    if (t > 10) p.cmds[0].buttons = p.cmds[1].buttons = p.cmds[2].buttons = sh::kButtonAttack;
    r.frame(&p, 1);
    sv::q2_record_frame(rec, *r, static_cast<int32_t>(s.frame_number), &s.players[0].move);
  }
  sv::q2_record_stop(rec);
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(rec.bytes, d, error), error);
  int hits = 0;
  while (cl::next_frame(d, error) == cl::DemoRead::frame)
    for (const cl::DemoEffect& e : d.state.effects) {
      if (e.code != 2 && e.code != 0) continue;  // the blaster's, a bullet's
      hits++;
      // On a wall at x = 512 or -512, thrown back toward the middle.
      const float side = e.at.x > 0 ? 1.0f : -1.0f;
      CHECK_MSG(e.at.x * side > 500 && e.way.x * side < -0.9f,
                std::to_string(e.at.x) + " " + std::to_string(e.way.x) + " " +
                    std::to_string(e.way.y) + " " + std::to_string(e.way.z));
    }
  CHECK_MSG(hits > 0, "no shot struck the wall");
}

// A player who enters and later leaves is marked by the protocol's flashes 9 and 10 on his
// entity, so a joined client (and a recording) throws the green and the red burst and its sound.
TEST(a_player_entering_and_leaving_flashes_on_the_wire) {
  sv::RulesRunner r = start();
  sv::Q2Server& s = *sv::q2_of(r);
  r.connect(0, "eve", false, false);
  sv::Q2Recorder rec;
  sv::q2_record_start(rec, *r, 0, {});
  uint32_t seq = 0;
  for (int t = 0; t < 60; t++) {
    if (t == 10) r.connect(1, "bob", false, false);
    if (t == 40) r.drop(1);
    sh::Packet p;
    p.client = 0;
    p.sequence = ++seq;
    p.cmds[0] = p.cmds[1] = p.cmds[2] = cmd(0, 0);
    r.frame(&p, 1);
    sv::q2_record_frame(rec, *r, static_cast<int32_t>(s.frame_number), &s.players[0].move);
  }
  sv::q2_record_stop(rec);
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(rec.bytes, d, error), error);
  std::string got;
  int heard = 0;
  while (cl::next_frame(d, error) == cl::DemoRead::frame) {
    for (const cl::DemoEffect& e : d.state.effects)
      if (e.name.starts_with("sq_")) {
        got += e.name + ":" + std::to_string(e.entity) + " ";
        CHECK(e.at.x != 0 || e.at.y != 0);  // at a spawn point, never the map's middle
      }
    for (const cl::DemoSound& o : d.state.started)
      if (o.name != nullptr && std::string(o.name) == "weapons/grenlf1a.wav") heard++;
  }
  CHECK_MSG(got.find("sq_login:2 ") != std::string::npos, got);
  CHECK_MSG(got.find("sq_logout:2 ") != std::string::npos, got);
  CHECK_MSG(heard == 2, std::to_string(heard));
}

// The hit report (temporary entity 128, one short: the points dealt) goes only to a viewer that
// reads it; a stock client's stream never carries it.
TEST(the_hit_report_goes_only_to_a_viewer_that_reads_it) {
  for (const bool reads : {false, true}) {
    sv::RulesRunner r = start();
    sv::Q2Server& s = *sv::q2_of(r);
    r.connect(0, "eve", false, false);
    sv::Q2Recorder rec;
    sv::Q2WireOptions opt;
    opt.hit_reports = reads;
    sv::q2_record_start(rec, *r, 0, opt);
    uint32_t seq = 0;
    for (int t = 0; t < 30; t++) {
      sh::Packet p;
      p.client = 0;
      p.sequence = ++seq;
      p.cmds[0] = p.cmds[1] = p.cmds[2] = cmd(0, 0);
      r.frame(&p, 1);
      if (t == 20) {
        sv::NetEvent ev;
        ev.kind = sv::EventKind::point_effect;
        ev.to = 1u;
        ev.ent = 1;
        ev.values[0] = 25;
        ev.text = "te_damage_dealt";
        r->out.events.push_back(ev);
        r->out.events_total++;
      }
      sv::q2_record_frame(rec, *r, static_cast<int32_t>(s.frame_number), &s.players[0].move);
    }
    sv::q2_record_stop(rec);
    const uint8_t report[] = {3, 128, 25, 0};  // svc_temp_entity, 128, the short 25
    const bool carried = std::search(rec.bytes.begin(), rec.bytes.end(), std::begin(report),
                                     std::end(report)) != rec.bytes.end();
    CHECK_MSG(carried == reads, reads ? "the report is missing" : "a stock stream carried it");
  }
}

// AN EXTENDED GAME'S LOOPING SOUNDS AND NO-CULL ENTITIES: two rooms that cannot see each other
// (x >= 0 cluster 0, x < 0 cluster 1) but hear each other. A looping sound goes by the hearing set
// and its reach (80 units, then 1 / 0.003 for the default: about 413), not by sight; past its
// reach one without a model is gone and one with a model is kept only where seen.
TEST(an_extended_games_loops_go_by_hearing_and_reach) {
  br::Map m;
  m.planes.push_back(br::Plane{{1, 0, 0}, 0, 0});
  m.nodes.push_back(br::Node{0, {-1, -2}});
  m.models.emplace_back();
  m.leaves.resize(2);
  m.leaves[0].cluster = 0;
  m.leaves[1].cluster = 1;
  m.clusters = 2;
  m.row_bytes = 1;
  m.pvs = {0x01, 0x02};
  m.phs = {0x03, 0x03};
  sv::Q2Sight sight;
  sv::q2_sight_reset(sight, &m);
  const auto link = [&](int n, float x) {
    const float p[3] = {x, 0, 0};
    sv::q2_sight_link(sight, n, p, p);
  };
  link(5, -200);   // a speaker behind the wall, in reach
  link(6, -1000);  // far behind it
  link(7, 1000);   // far, in sight
  sv::Q2Wire w;
  w.slot = 0;
  w.sight = &sight;
  const float eye[3] = {30, 0, 0};
  CHECK(sv::q2_sight_view(w, eye));
  const auto at = [](float x, int model, int sound, uint8_t attenuation = 0) {
    sv::Q2EntityState s;
    s.present = true;
    s.origin[0] = static_cast<int32_t>(x * 8);
    s.model[0] = model;
    s.sound = sound;
    s.loop_attenuation = attenuation;
    return s;
  };
  // The ordinary cut: a speaker is kept by sight and its 400 units.
  CHECK(!sv::q2_sight_keeps(w, 5, at(-200, 0, 1), eye));
  CHECK(!sv::q2_sight_keeps(w, 7, at(1000, 0, 1), eye) &&
        sv::q2_sight_keeps(w, 7, at(1000, 1, 1), eye));
  sight.extended = true;
  CHECK(sv::q2_sight_keeps(w, 5, at(-200, 0, 1), eye));        // heard, in reach
  CHECK(!sv::q2_sight_keeps(w, 6, at(-1000, 0, 1), eye));      // heard, out of reach
  CHECK(sv::q2_sight_keeps(w, 6, at(-1000, 0, 1, 192), eye));  // no attenuation: never fades
  CHECK(!sv::q2_sight_keeps(w, 6, at(-1000, 1, 1), eye));      // a model past reach, unseen
  CHECK(sv::q2_sight_keeps(w, 7, at(1000, 1, 1), eye));        // a model past reach, seen
  CHECK(!sv::q2_sight_keeps(w, 7, at(1000, 0, 1), eye));       // the default's 413 units
  CHECK(sv::q2_sight_keeps(w, 7, at(1000, 0, 1, 64), eye));    // attenuation 1: 1747 units
  CHECK(!sv::q2_sight_keeps(w, 6, at(-1000, 1, 0), eye));      // no loop: by sight, unseen
  sight.links[6].everywhere = true;                            // no-cull
  CHECK(sv::q2_sight_keeps(w, 6, at(-1000, 1, 0), eye));
}

// A SHUT DOOR CUTS THE FRAME: two rooms (areas 1 and 2, split at x = 0) and the portal between
// them. Shut, a viewer in room 1 is sent what is in room 1 and the door spanning both, not what
// is in room 2, and its area bits mark room 1 alone; open, both rooms.
TEST(a_shut_door_cuts_the_entities_and_the_area_bits) {
  br::Map m;
  m.planes.push_back(br::Plane{{1, 0, 0}, 0, 0});
  m.nodes.push_back(br::Node{0, {-1, -2}});  // in front: leaf 0; behind: leaf 1
  m.models.emplace_back();
  m.leaves.resize(2);
  m.leaves[0].cluster = 0;
  m.leaves[0].area = 1;
  m.leaves[1].cluster = 1;
  m.leaves[1].area = 2;
  m.clusters = 2;
  m.row_bytes = 1;
  m.pvs = {0x03, 0x03};
  m.area_portals = {{1, 2}, {1, 1}};
  m.areas = {{0, 0}, {1, 0}, {1, 1}};
  sv::Q2Sight sight;
  sv::q2_sight_reset(sight, &m);
  const auto link = [&](int n, float x0, float x1) {
    const float lo[3] = {x0, -8, -8}, hi[3] = {x1, 8, 8};
    sv::q2_sight_link(sight, n, lo, hi);
  };
  link(5, 10, 20);    // in room 1
  link(6, -20, -10);  // in room 2
  link(7, -4, 4);     // the door: both
  sv::Q2Wire w;
  w.slot = 0;
  w.sight = &sight;
  sv::Q2EntityState s;
  s.present = true;
  s.model[0] = 1;
  const float eye[3] = {30, 0, 0};
  std::vector<uint8_t> shut(2, 0), open(2, 0);
  open[1] = 1;
  br::flood_areas(m, shut, &sight.groups);
  CHECK(sv::q2_sight_view(w, eye));
  CHECK(w.area == 1);
  CHECK(sv::q2_sight_keeps(w, 5, s, eye) && !sv::q2_sight_keeps(w, 6, s, eye) &&
        sv::q2_sight_keeps(w, 7, s, eye));
  std::vector<uint8_t> bits;
  sv::q2_write_area_bits(bits, w);
  CHECK(bits == std::vector<uint8_t>({1, 0x02}));
  br::flood_areas(m, open, &sight.groups);
  CHECK(sv::q2_sight_view(w, eye));
  CHECK(sv::q2_sight_keeps(w, 6, s, eye));
  bits.clear();
  sv::q2_write_area_bits(bits, w);
  CHECK(bits == std::vector<uint8_t>({1, 0x06}));
  // A sound or an effect beyond the door is heard only while it stands open; one in a solid's
  // area 0 never (no leaf of this map is one, so the shut door alone is shown).
  const float ear[3] = {30, 0, 0};
  CHECK(sv::q2_sight_hears(w, ear, {-15, 0, 0}) && sv::q2_sight_hears(w, ear, {15, 0, 0}));
  br::flood_areas(m, shut, &sight.groups);
  CHECK(sv::q2_sight_view(w, eye));
  CHECK(!sv::q2_sight_hears(w, ear, {-15, 0, 0}) && sv::q2_sight_hears(w, ear, {15, 0, 0}));
  // A map with no area data: every entity by its clusters, the 32 bytes of every area.
  sight.groups.clear();
  CHECK(sv::q2_sight_view(w, eye));
  bits.clear();
  sv::q2_write_area_bits(bits, w);
  CHECK(bits.size() == 33 && bits[0] == 32 && bits[32] == 0xFF);
}

// A frame sized to the packet's room after a long reliable message keeps its head, the player and
// what fits; it does not grow past the room and lose itself whole.
TEST(a_frame_short_of_room_sends_what_fits) {
  sv::RulesRunner r = start();
  sv::Q2Server& s = *sv::q2_of(r);
  r.connect(0, "eve", false, false);
  sh::Packet p;
  p.client = 0;
  p.sequence = 1;
  p.cmds[0] = p.cmds[1] = p.cmds[2] = cmd(0, 0);
  r.frame(&p, 1);
  sv::Q2Wire w;
  std::vector<std::vector<uint8_t>> blocks;
  sv::q2_signon(w, *r, 0, {}, blocks);
  for (int i = 0; i < 120; i++) {  // more effects than the room holds
    sv::NetEvent ev;
    ev.kind = sv::EventKind::point_effect;
    ev.to = 0xFFFFFFFFu;
    ev.origin = {float(i), 0, 30};
    ev.text = "te_explosion";
    r->out.events.push_back(ev);
    r->out.events_total++;
  }
  w.room = 400;
  w.room_grows = false;
  std::vector<uint8_t> block, rel;
  sv::q2_frame(w, *r, static_cast<int32_t>(s.frame_number), &s.players[0].move, block, &rel);
  CHECK_MSG(block.size() <= 400 && block.size() > 200, std::to_string(block.size()));
  CHECK(!block.empty() && block[0] == 20);  // the frame's head kept
}

// A player's `msg` level holds back the prints below it: at 2 the pickup line (level 0) and the
// obituary (1) stay home and the high print (2) and the chat line (3) are sent.
TEST(a_players_msg_level_holds_back_the_prints_below_it) {
  sv::RulesRunner r = start();
  sv::Q2Server& s = *sv::q2_of(r);
  r.connect(0, "eve", false, false);
  r.user_info(0, "msg", "2");
  sh::Packet p;
  p.client = 0;
  p.sequence = 1;
  p.cmds[0] = p.cmds[1] = p.cmds[2] = cmd(0, 0);
  r.frame(&p, 1);
  sv::Q2Wire w;
  w.slot = 0;
  std::vector<std::vector<uint8_t>> blocks;
  sv::q2_signon(w, *r, 0, {}, blocks);
  const char* lines[] = {"pickup-line", "obituary-line", "high-line", "chat-line"};
  for (int level = 0; level < 4; level++) {
    sv::NetEvent ev;
    ev.kind = sv::EventKind::print;
    ev.to = 1;
    ev.level = level;
    ev.text = lines[level];
    r->out.events.push_back(ev);
    r->out.events_total++;
  }
  std::vector<uint8_t> block, rel;
  sv::q2_frame(w, *r, static_cast<int32_t>(s.frame_number), &s.players[0].move, block, &rel);
  const std::string sent(rel.begin(), rel.end());
  CHECK(sent.find("pickup-line") == std::string::npos);
  CHECK(sent.find("obituary-line") == std::string::npos);
  CHECK(sent.find("high-line") != std::string::npos);
  CHECK(sent.find("chat-line") != std::string::npos);
}
