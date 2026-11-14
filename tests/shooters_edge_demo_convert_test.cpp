// Tests: demo formats converted as they are read (games/shooters_edge/client/demo_convert.hpp). A
// single-player demo written in code (demo_write.hpp) converts to a multiview recording that
// reads back frame for frame: the same map, the eye on the same path, the same view angles, the
// same status, the rocket renumbered past the player slots where the source had it; fed a few
// bytes at a time, the stream gives the same bytes as the whole file, every take whole records;
// the loop's form leaves the end out; the same demo converts to a client recording the oldest
// clients play (packets within their size, the header's two words one rising sequence, a command
// record leading each frame's packet with its angles, the commands one sequence ahead, a
// disconnect at the end) that reads back frame for frame, and a frame of more entities than the
// oldest client holds keeps the 64 nearest the view; a multiview recording converts to one
// player's view the same way, read back frame for frame, and a view chosen partway or switched
// starts with a whole signon and plays to the end; a client recording (written in code, or
// converted from a single-player demo) converts to a multiview recording that reads back frame for
// frame; a multiview recording converts to a single-player demo of one view (blocks within the
// oldest client's size, a disconnect at the end) whose eye rides that player frame for frame, and
// a client recording to one of its recorder's view that reads back as it does; the intermission
// views from its spot, the way its message looks, in either; what the target
// cannot hold is counted; a pair
// that does not exist and a damaged demo are refused with the reason; a file's format is told by
// its name.
//
// ge:summary Tests: a .dem converted to .mvd as a stream, read back frame for frame
// ge:keywords test demo-convert dem mvd stream whole-records no-end dropped format_of_name
// ge:keywords client-recording old-client 64-entities command-record multiview-to-client pov
// ge:keywords client-to-multiview mvd-to-dem client-to-dem
// ge:skill shooter testing
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_convert.hpp"
#include "shooters_edge/client/demo_convert_client.hpp"
#include "shooters_edge/client/demo_pack.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/client/sequel_demo.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace sh = ge::shooter;

cl::DemoScript script() { return cl::loop_script("halls", {128, 192, 24}, 64, 90); }

bool same_angle(float a, float b, float step) {
  const double d = std::fmod(std::fabs(static_cast<double>(a) - b), 360.0);
  return std::min(d, 360 - d) <= step;
}

struct Frames {
  std::vector<sh::Vec3f> eyes, angles;
  std::vector<int> health, rockets;
  std::vector<bool> rocket_seen;
  cl::DemoState last;
  cl::DemoRead end = cl::DemoRead::frame;
  std::string error;
};

// Every frame with a map, read; `rocket` the entity number the rocket goes by.
Frames read_frames(std::vector<uint8_t> bytes, cl::DemoFormat want, int rocket) {
  Frames f;
  cl::Demo d;
  if (!cl::open_demo(std::move(bytes), d, f.error)) {
    f.end = cl::DemoRead::error;
    return f;
  }
  CHECK(d.state.format == want);
  while ((f.end = cl::next_frame(d, f.error)) == cl::DemoRead::frame) {
    if (d.state.map.empty()) continue;
    f.eyes.push_back(d.state.eye);
    f.angles.push_back(d.state.angles);
    f.health.push_back(d.state.stats[cl::kStatHealth]);
    f.rockets.push_back(d.state.stats[cl::kStatRockets]);
    const auto& e = d.state.entities;
    f.rocket_seen.push_back(static_cast<size_t>(rocket) < e.size() &&
                            e[static_cast<size_t>(rocket)].present);
  }
  f.last = d.state;
  return f;
}

std::vector<uint8_t> converted(const std::vector<uint8_t>& dem, cl::ConvertNotes& notes,
                               const cl::ConvertOptions& opt = {}) {
  std::vector<uint8_t> out;
  std::string error;
  CHECK_MSG(cl::convert_demo(dem, cl::DemoFormat::multiview, opt, out, notes, error), error);
  return out;
}

// The records of a multiview recording, walked: false when one is cut.
bool whole_records(const std::vector<uint8_t>& b, int& count, int& last_kind_id) {
  size_t at = 0;
  count = 0;
  while (at < b.size()) {
    if (at + 2 > b.size()) return false;
    const int kind = b[at + 1] & 7;
    at += 2;
    if (kind == 3) at += 4;
    if (at + 4 > b.size()) return false;
    uint32_t len = 0;
    std::memcpy(&len, &b[at], 4);
    at += 4;
    if (at + len > b.size()) return false;
    last_kind_id = len > 0 ? b[at] : -1;
    at += len;
    count++;
  }
  return true;
}

TEST(a_single_player_demo_converts_to_a_multiview_recording_frame_for_frame) {
  const cl::DemoScript s = script();
  const std::vector<uint8_t> dem = cl::write_single_demo(s);
  cl::ConvertNotes notes;
  const std::vector<uint8_t> mvd = converted(dem, notes);

  const Frames a = read_frames(dem, cl::DemoFormat::single, 2);
  const Frames b = read_frames(mvd, cl::DemoFormat::multiview, 33);  // 2 - 1 + 32
  CHECK_MSG(a.end == cl::DemoRead::end, a.error);
  CHECK_MSG(b.end == cl::DemoRead::end, b.error);
  CHECK(b.last.map == "halls" && b.last.levels == 1 && b.last.protocol == 28);
  CHECK(b.last.models == a.last.models && b.last.sounds == a.last.sounds);
  CHECK(b.last.static_sounds.size() == a.last.static_sounds.size());
  // The source reads its signon as a frame of its own; the target's signon rides its first frame.
  CHECK(a.eyes.size() >= s.origins.size() && b.eyes.size() == s.origins.size());
  const size_t lead = a.eyes.size() - s.origins.size();
  for (size_t i = 0; i < s.origins.size() && i < b.eyes.size(); i++) {
    const sh::Vec3f o = s.origins[i];
    CHECK(b.eyes[i].x == o.x && b.eyes[i].y == o.y && b.eyes[i].z == o.z + 22);
    CHECK(same_angle(b.angles[i].y, a.angles[lead + i].y, 360.0 / 65536));
    CHECK(same_angle(b.angles[i].x, a.angles[lead + i].x, 360.0 / 65536));
    CHECK(b.health[i] == a.health[lead + i] && b.rockets[i] == a.rockets[lead + i]);
    CHECK(b.rocket_seen[i] == a.rocket_seen[lead + i]);
  }
  CHECK(notes.frames == static_cast<int64_t>(s.origins.size()));
  CHECK(notes.levels == 1);
  CHECK(notes.dropped.contains("the sign-on stage numbers"));
  CHECK(notes.dropped.contains("the protocol's version message"));
}

TEST(the_stream_gives_the_same_bytes_in_whole_records_however_the_source_arrives) {
  const std::vector<uint8_t> dem = cl::write_single_demo(script());
  cl::ConvertNotes notes;
  const std::vector<uint8_t> whole = converted(dem, notes);
  for (size_t piece : {size_t{1}, size_t{7}, size_t{300}}) {
    cl::DemoConvert c;
    std::string error;
    CHECK_MSG(cl::convert_begin(c, cl::DemoFormat::single, cl::DemoFormat::multiview, {}, error),
              error);
    std::vector<uint8_t> all;
    int takes_with_bytes = 0;
    for (size_t at = 0; at < dem.size(); at += piece) {
      cl::convert_feed(c, std::span(dem).subspan(at, std::min(piece, dem.size() - at)));
      std::vector<uint8_t> took;
      CHECK_MSG(cl::convert_take(c, took, error), error);
      int count = 0, last = 0;
      CHECK(whole_records(took, count, last));  // never half a record
      if (!took.empty()) takes_with_bytes++;
      all.insert(all.end(), took.begin(), took.end());
    }
    cl::convert_end(c);
    CHECK_MSG(cl::convert_take(c, all, error), error);
    CHECK(all == whole);
    CHECK(takes_with_bytes > 10);  // frame by frame, not all at the end
  }
}

TEST(the_loops_form_leaves_the_end_out) {
  const std::vector<uint8_t> dem = cl::write_single_demo(script());
  cl::ConvertNotes notes;
  int count = 0, last = 0;
  CHECK(whole_records(converted(dem, notes), count, last));
  CHECK(last == 2);  // a disconnect ends it
  cl::ConvertOptions loop;
  loop.no_end = true;
  CHECK(whole_records(converted(dem, notes, loop), count, last));
  CHECK(last != 2);
}

// A client recording's records walked, against what the oldest client asks of one.
struct ClientWalk {
  bool whole = true;         // no record cut short
  size_t largest = 0;        // the largest packet record's length, header included
  bool one_sequence = true;  // both header words equal, rising by one a packet
  int sequences = 0;         // sequences records
  int frames_led = 0;        // packets with a command record just before them at their time
  int packets = 0;
  int last_id = -1;  // the last packet's first message
};

ClientWalk walk_client(const std::vector<uint8_t>& b) {
  ClientWalk w;
  size_t at = 0;
  uint32_t seq = 0;
  bool cmd_before = false;
  float cmd_time = -1;
  auto f32 = [&](size_t i) {
    float f = 0;
    std::memcpy(&f, &b[i], 4);
    return f;
  };
  auto u32 = [&](size_t i) {
    uint32_t v = 0;
    std::memcpy(&v, &b[i], 4);
    return v;
  };
  while (at < b.size()) {
    if (at + 5 > b.size()) return w.whole = false, w;
    const float t = f32(at);
    const int type = b[at + 4];
    at += 5;
    if (type == 0) {
      if (at + 36 > b.size()) return w.whole = false, w;
      cmd_before = true;
      cmd_time = t;
      at += 36;
    } else if (type == 2) {
      w.sequences++;
      at += 8;
    } else if (type == 1) {
      const uint32_t len = u32(at);
      if (at + 4 + len > b.size() || len < 8) return w.whole = false, w;
      w.largest = std::max<size_t>(w.largest, len);
      const uint32_t a = u32(at + 4), ack = u32(at + 8);
      if (a != ack || a != seq + 1) w.one_sequence = false;
      seq = a;
      if (cmd_before && cmd_time == t) w.frames_led++;
      cmd_before = false;
      w.packets++;
      w.last_id = len > 8 ? b[at + 12] : -1;
      at += 4 + len;
    } else {
      return w.whole = false, w;
    }
  }
  return w;
}

TEST(a_single_player_demo_converts_to_a_client_recording_the_oldest_clients_play) {
  const cl::DemoScript s = script();
  const std::vector<uint8_t> dem = cl::write_single_demo(s);
  cl::ConvertNotes notes;
  std::vector<uint8_t> rec;
  std::string error;
  CHECK_MSG(cl::convert_demo(dem, cl::DemoFormat::client, {}, rec, notes, error), error);

  const ClientWalk w = walk_client(rec);
  CHECK(w.whole && w.one_sequence);
  CHECK(w.largest <= 1450);
  CHECK(w.sequences == 1);
  CHECK(w.frames_led == static_cast<int>(s.origins.size()));  // every frame's packet led
  CHECK(w.last_id == 2);                                      // a disconnect ends it

  const Frames a = read_frames(dem, cl::DemoFormat::single, 2);
  const Frames b = read_frames(rec, cl::DemoFormat::client, 33);
  CHECK_MSG(b.end == cl::DemoRead::end, b.error);
  CHECK(b.last.map == "halls" && b.last.protocol == 28 && b.last.player == 0);
  CHECK(b.last.models == a.last.models && b.last.sounds == a.last.sounds);
  // The ends line up: the signon packets read as frames of their own ahead of the first.
  CHECK(b.eyes.size() >= s.origins.size());
  const size_t la = a.eyes.size() - s.origins.size(), lb = b.eyes.size() - s.origins.size();
  for (size_t i = 0; i < s.origins.size(); i++) {
    const sh::Vec3f o = s.origins[i];
    CHECK(b.eyes[lb + i].x == o.x && b.eyes[lb + i].y == o.y && b.eyes[lb + i].z == o.z + 22);
    CHECK(same_angle(b.angles[lb + i].y, a.angles[la + i].y, 0.001f));
    CHECK(b.health[lb + i] == a.health[la + i] && b.rockets[lb + i] == a.rockets[la + i]);
    CHECK(b.rocket_seen[lb + i] == a.rocket_seen[la + i]);
  }

  // Fed a few bytes at a time, the same bytes.
  cl::DemoConvert c;
  CHECK_MSG(cl::convert_begin(c, cl::DemoFormat::single, cl::DemoFormat::client, {}, error), error);
  std::vector<uint8_t> all;
  for (size_t at = 0; at < dem.size(); at += 7) {
    cl::convert_feed(c, std::span(dem).subspan(at, std::min<size_t>(7, dem.size() - at)));
    CHECK_MSG(cl::convert_take(c, all, error), error);
  }
  cl::convert_end(c);
  CHECK_MSG(cl::convert_take(c, all, error), error);
  CHECK(all == rec);
}

TEST(a_frame_of_more_entities_than_the_oldest_client_holds_keeps_the_64_nearest) {
  cl::ClientRecording rec;
  cl::ConvertNotes notes;
  std::vector<uint8_t> out;
  cl::ClientLevel l;
  l.game = "id1";
  l.title = "test";
  l.models = {"maps/halls.bsp", "progs/player.mdl", "progs/missile.mdl"};
  l.sounds = {"weapons/rocket1i.wav"};
  rec.level(out, 0, l, notes);
  cl::ClientFrame f;
  f.time = 0.1;
  f.players[0].present = true;
  for (int i = 0; i < 100; i++) {
    cl::ConvEnt e;
    e.model = 3;
    e.origin = {static_cast<int16_t>(8 * 10 * i), 0, 0};  // entity 33 + i, 10 units further a step
    f.entities.emplace(33 + i, e);
  }
  rec.frame(out, f, {}, notes);
  CHECK(notes.dropped["an entity past the 64 a frame holds"] == 36);
  const ClientWalk w = walk_client(out);
  CHECK(w.whole && w.one_sequence && w.largest <= 1450);
  // Read back: entities 33..96 are there, 97 on are not.
  std::vector<uint8_t> file = out;
  rec.end(file, 0.2, true, notes);
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(std::move(file), d, error), error);
  bool saw = false;
  while (cl::next_frame(d, error) == cl::DemoRead::frame) {
    const auto& ents = d.state.entities;
    int present = 0;
    for (const auto& e : ents) present += e.present ? 1 : 0;
    if (present == 0) continue;
    saw = true;
    CHECK(present == 64);
    CHECK(ents.size() > 96 && ents[96].present && (ents.size() <= 97 || !ents[97].present));
  }
  CHECK(saw);
}

// The eye a client recording's frames end on, and the view's slot, read back.
struct ViewRead {
  std::vector<sh::Vec3f> eyes;
  int player = -1, levels = 0;
  cl::DemoRead end = cl::DemoRead::frame;
  std::string error;
};

ViewRead read_view(std::vector<uint8_t> bytes) {
  ViewRead v;
  cl::Demo d;
  if (!cl::open_demo(std::move(bytes), d, v.error)) {
    v.end = cl::DemoRead::error;
    return v;
  }
  CHECK(d.state.format == cl::DemoFormat::client);
  while ((v.end = cl::next_frame(d, v.error)) == cl::DemoRead::frame) {
    if (d.state.map.empty()) continue;
    v.eyes.push_back(d.state.eye);
    v.player = d.state.player;
  }
  return v;
}

bool near_eye(sh::Vec3f eye, sh::Vec3f origin) {
  return std::fabs(eye.x - origin.x) <= 0.125f && std::fabs(eye.y - origin.y) <= 0.125f &&
         std::fabs(eye.z - (origin.z + 22)) <= 0.125f;
}

std::vector<uint8_t> multiview_to_client(const std::vector<uint8_t>& mvd, int pov, size_t switch_at,
                                         int then, cl::ConvertNotes& notes) {
  cl::DemoConvert c;
  std::string error;
  cl::ConvertOptions opt;
  opt.pov = pov;
  CHECK_MSG(cl::convert_begin(c, cl::DemoFormat::multiview, cl::DemoFormat::client, opt, error),
            error);
  std::vector<uint8_t> out;
  for (size_t at = 0; at < mvd.size(); at += 97) {
    if (at >= switch_at && c.options.pov != then) {
      cl::convert_set_pov(c, then);
      if (pov == -2) CHECK(out.empty());  // nothing written while no one watches
    }
    cl::convert_feed(c, std::span(mvd).subspan(at, std::min<size_t>(97, mvd.size() - at)));
    CHECK_MSG(cl::convert_take(c, out, error), error);
  }
  cl::convert_end(c);
  CHECK_MSG(cl::convert_take(c, out, error), error);
  notes = c.notes;
  return out;
}

TEST(a_multiview_recording_converts_to_one_players_view_for_the_oldest_clients) {
  const cl::MatchScript m = cl::highlight_match();
  const std::vector<uint8_t> mvd = cl::write_multiview_match(m);
  cl::ConvertNotes notes;
  std::vector<uint8_t> rec;
  std::string error;
  CHECK_MSG(cl::convert_demo(mvd, cl::DemoFormat::client, {}, rec, notes, error), error);
  const ClientWalk w = walk_client(rec);
  CHECK(w.whole && w.one_sequence && w.largest <= 1450);
  CHECK(w.sequences == 1 && w.last_id == 2);
  CHECK_MSG(w.frames_led == m.frames,
            "frames led " + std::to_string(w.frames_led) + " of " + std::to_string(m.frames));
  const ViewRead v = read_view(rec);
  CHECK_MSG(v.end == cl::DemoRead::end, v.error);
  CHECK(v.player == 0);  // the lowest slot playing
  CHECK(v.eyes.size() >= static_cast<size_t>(m.frames));
  // The ends line up: every frame's eye on slot 0's origin.
  const auto& o = m.players[0].origins;
  const size_t lead = v.eyes.size() - static_cast<size_t>(m.frames);
  for (int i = 0; i < m.frames; i++) {
    const sh::Vec3f e = v.eyes[lead + static_cast<size_t>(i)];
    const sh::Vec3f want = o[std::min(static_cast<size_t>(i), o.size() - 1)];
    CHECK_MSG(near_eye(e, want), "frame " + std::to_string(i) + ": eye " + std::to_string(e.x) +
                                     " " + std::to_string(e.y) + " " + std::to_string(e.z) +
                                     ", origin " + std::to_string(want.x) + " " +
                                     std::to_string(want.y) + " " + std::to_string(want.z));
  }

  // Fed a few bytes at a time, the same bytes.
  cl::ConvertNotes again;
  CHECK(multiview_to_client(mvd, -1, mvd.size(), -1, again) == rec);
}

TEST(a_view_chosen_partway_starts_with_a_whole_signon_and_a_view_switched_starts_again) {
  const cl::MatchScript m = cl::highlight_match();
  const std::vector<uint8_t> mvd = cl::write_multiview_match(m);
  // No one watches until halfway: then slot 2.
  cl::ConvertNotes notes;
  const std::vector<uint8_t> late = multiview_to_client(mvd, -2, mvd.size() / 2, 2, notes);
  ClientWalk w = walk_client(late);
  CHECK(w.whole && w.one_sequence && w.largest <= 1450 && w.sequences == 1);
  ViewRead v = read_view(late);
  CHECK_MSG(v.end == cl::DemoRead::end, v.error);
  CHECK(v.player == 2);
  CHECK(v.eyes.size() > 10 && v.eyes.size() < static_cast<size_t>(m.frames));
  const auto& o2 = m.players[2].origins;
  CHECK(near_eye(v.eyes.back(), o2.back()));

  // Slot 0 first, then slot 1 from halfway: a second signon, the rest on slot 1.
  const std::vector<uint8_t> both = multiview_to_client(mvd, 0, mvd.size() / 2, 1, notes);
  w = walk_client(both);
  CHECK(w.whole && w.one_sequence && w.largest <= 1450 && w.sequences == 2);
  v = read_view(both);
  CHECK_MSG(v.end == cl::DemoRead::end, v.error);
  CHECK(v.player == 1);
  CHECK(near_eye(v.eyes.back(), m.players[1].origins.back()));
}

TEST(a_client_recording_converts_to_a_multiview_recording_frame_for_frame) {
  const cl::DemoScript s = script();
  // Two client recordings: one written in code, one converted from a single-player demo.
  cl::ConvertNotes notes;
  std::vector<uint8_t> from_dem;
  std::string error;
  CHECK_MSG(cl::convert_demo(cl::write_single_demo(s), cl::DemoFormat::client, {}, from_dem, notes,
                             error),
            error);
  for (const std::vector<uint8_t>& rec : {cl::write_client_demo(s), from_dem}) {
    std::vector<uint8_t> mvd;
    cl::ConvertNotes n;
    CHECK_MSG(cl::convert_demo(rec, cl::DemoFormat::multiview, {}, mvd, n, error), error);
    int records = 0, last = -1;
    CHECK(whole_records(mvd, records, last));
    CHECK(last == 2);  // a disconnect ends it
    const Frames a = read_frames(rec, cl::DemoFormat::client, 33);
    const Frames b = read_frames(mvd, cl::DemoFormat::multiview, 33);
    CHECK_MSG(b.end == cl::DemoRead::end, b.error);
    CHECK(b.last.map == a.last.map && b.last.models == a.last.models);
    CHECK_MSG(b.eyes.size() >= s.origins.size() && a.eyes.size() >= s.origins.size(),
              std::to_string(a.eyes.size()) + " / " + std::to_string(b.eyes.size()) + " of " +
                  std::to_string(s.origins.size()));
    if (b.eyes.size() < s.origins.size() || a.eyes.size() < s.origins.size()) continue;
    const size_t la = a.eyes.size() - s.origins.size(), lb = b.eyes.size() - s.origins.size();
    for (size_t i = 0; i < s.origins.size(); i++) {
      const sh::Vec3f x = a.eyes[la + i], y = b.eyes[lb + i];
      CHECK_MSG(x.x == y.x && x.y == y.y && x.z == y.z,
                "frame " + std::to_string(i) + " of " + std::to_string(a.eyes.size()) + "/" +
                    std::to_string(b.eyes.size()) + ": " + std::to_string(x.x) + "," +
                    std::to_string(x.z) + " vs " + std::to_string(y.x) + "," + std::to_string(y.z));
      CHECK(same_angle(b.angles[lb + i].y, a.angles[la + i].y, 0.01f));
      CHECK(b.health[lb + i] == a.health[la + i] && b.rockets[lb + i] == a.rockets[la + i]);
      CHECK(b.rocket_seen[lb + i] == a.rocket_seen[la + i]);
    }
    // Fed a few bytes at a time, the same bytes.
    cl::DemoConvert c;
    CHECK_MSG(cl::convert_begin(c, cl::DemoFormat::client, cl::DemoFormat::multiview, {}, error),
              error);
    std::vector<uint8_t> all;
    for (size_t at = 0; at < rec.size(); at += 11) {
      cl::convert_feed(c, std::span(rec).subspan(at, std::min<size_t>(11, rec.size() - at)));
      CHECK_MSG(cl::convert_take(c, all, error), error);
    }
    cl::convert_end(c);
    CHECK_MSG(cl::convert_take(c, all, error), error);
    CHECK(all == mvd);
  }
}

// A single-player demo's blocks walked: the music line, then blocks of a length, the view's
// angles and at least one message; false when one is cut or holds more than the oldest client
// reads.
struct BlockWalk {
  bool whole = false;
  int blocks = 0;
  size_t largest = 0;
  int last_id = -1;
};

BlockWalk walk_blocks(const std::vector<uint8_t>& b) {
  BlockWalk w;
  size_t at = 0;
  while (at < b.size() && b[at] != '\n') at++;
  if (at == b.size()) return w;
  at++;
  while (at < b.size()) {
    if (at + 16 > b.size()) return w;
    uint32_t len = 0;
    std::memcpy(&len, &b[at], 4);
    at += 16;
    if (len == 0 || at + len > b.size()) return w;
    w.largest = std::max<size_t>(w.largest, len);
    w.last_id = b[at];
    at += len;
    w.blocks++;
  }
  w.whole = true;
  return w;
}

std::vector<sh::Vec3f> single_eyes(std::vector<uint8_t> bytes, cl::DemoRead& end,
                                   std::string& error) {
  std::vector<sh::Vec3f> eyes;
  cl::Demo d;
  if (!cl::open_demo(std::move(bytes), d, error)) {
    end = cl::DemoRead::error;
    return eyes;
  }
  CHECK(d.state.format == cl::DemoFormat::single);
  while ((end = cl::next_frame(d, error)) == cl::DemoRead::frame)
    if (!d.state.map.empty()) eyes.push_back(d.state.eye);
  return eyes;
}

TEST(a_multiview_recording_converts_to_a_single_player_demo_of_one_view) {
  const cl::MatchScript m = cl::highlight_match();
  const std::vector<uint8_t> mvd = cl::write_multiview_match(m);
  for (const int pov : {0, 2}) {
    cl::ConvertOptions opt;
    opt.pov = pov;
    cl::ConvertNotes notes;
    std::vector<uint8_t> dem;
    std::string error;
    CHECK_MSG(cl::convert_demo(mvd, cl::DemoFormat::single, opt, dem, notes, error), error);
    const BlockWalk w = walk_blocks(dem);
    CHECK(w.whole && w.largest <= 8000);
    CHECK(w.last_id == 2);  // a disconnect ends it
    cl::DemoRead end = cl::DemoRead::frame;
    const std::vector<sh::Vec3f> eyes = single_eyes(dem, end, error);
    CHECK_MSG(end == cl::DemoRead::end, error);
    CHECK_MSG(eyes.size() >= static_cast<size_t>(m.frames),
              std::to_string(eyes.size()) + " of " + std::to_string(m.frames));
    if (eyes.size() < static_cast<size_t>(m.frames)) continue;
    // The ends line up: every frame's eye on the view's origin.
    const auto& o = m.players[static_cast<size_t>(pov)].origins;
    const size_t lead = eyes.size() - static_cast<size_t>(m.frames);
    for (int i = 0; i < m.frames; i++) {
      const sh::Vec3f e = eyes[lead + static_cast<size_t>(i)];
      const sh::Vec3f want = o[std::min(static_cast<size_t>(i), o.size() - 1)];
      CHECK_MSG(near_eye(e, want), "view " + std::to_string(pov) + " frame " + std::to_string(i) +
                                       ": eye " + std::to_string(e.x) + " " + std::to_string(e.y) +
                                       " " + std::to_string(e.z) + ", origin " +
                                       std::to_string(want.x) + " " + std::to_string(want.y) + " " +
                                       std::to_string(want.z));
    }
    // Fed a few bytes at a time, the same bytes.
    cl::DemoConvert c;
    CHECK_MSG(cl::convert_begin(c, cl::DemoFormat::multiview, cl::DemoFormat::single, opt, error),
              error);
    std::vector<uint8_t> all;
    for (size_t at = 0; at < mvd.size(); at += 97) {
      cl::convert_feed(c, std::span(mvd).subspan(at, std::min<size_t>(97, mvd.size() - at)));
      CHECK_MSG(cl::convert_take(c, all, error), error);
    }
    cl::convert_end(c);
    CHECK_MSG(cl::convert_take(c, all, error), error);
    CHECK(all == dem);
  }
}

TEST(the_intermission_views_from_its_spot_in_a_single_player_demo) {
  cl::MatchScript m = cl::highlight_match();
  m.intermission = m.frames - 20;
  m.spot = {-336, -74, 388};
  m.look = {22.5f, 45, 0};
  const std::vector<uint8_t> mvd = cl::write_multiview_match(m);
  for (const cl::DemoFormat to : {cl::DemoFormat::single, cl::DemoFormat::client}) {
    cl::ConvertNotes notes;
    std::vector<uint8_t> out;
    std::string error;
    CHECK_MSG(cl::convert_demo(mvd, to, {}, out, notes, error), error);
    cl::Demo d;
    CHECK_MSG(cl::open_demo(out, d, error), error);
    int at_spot = 0;
    while (cl::next_frame(d, error) == cl::DemoRead::frame) {
      if (!d.state.intermission) continue;
      const sh::Vec3f e = d.state.eye, a = d.state.angles;
      CHECK_MSG(e.x == m.spot.x && e.y == m.spot.y && e.z == m.spot.z,
                std::to_string(e.x) + " " + std::to_string(e.y) + " " + std::to_string(e.z));
      CHECK_MSG(same_angle(a.x, m.look.x, 0.01f) && same_angle(a.y, m.look.y, 0.01f),
                std::to_string(a.x) + " " + std::to_string(a.y));
      at_spot++;
    }
    CHECK_MSG(at_spot >= 19, cl::format_words(to) + ": " + std::to_string(at_spot));
  }
}

TEST(a_client_recording_converts_to_a_single_player_demo_of_its_recorders_view) {
  const cl::DemoScript s = script();
  const std::vector<uint8_t> rec = cl::write_client_demo(s);
  cl::ConvertNotes notes;
  std::vector<uint8_t> dem;
  std::string error;
  CHECK_MSG(cl::convert_demo(rec, cl::DemoFormat::single, {}, dem, notes, error), error);
  const BlockWalk w = walk_blocks(dem);
  CHECK(w.whole && w.largest <= 8000 && w.last_id == 2);
  const Frames a = read_frames(rec, cl::DemoFormat::client, 33);
  const Frames b = read_frames(dem, cl::DemoFormat::single, 17);
  CHECK_MSG(b.end == cl::DemoRead::end, b.error);
  CHECK(b.last.map == a.last.map);
  CHECK_MSG(b.eyes.size() >= s.origins.size(),
            std::to_string(b.eyes.size()) + " of " + std::to_string(s.origins.size()));
  if (b.eyes.size() < s.origins.size() || a.eyes.size() < s.origins.size()) return;
  const size_t la = a.eyes.size() - s.origins.size(), lb = b.eyes.size() - s.origins.size();
  for (size_t i = 0; i < s.origins.size(); i++) {
    const sh::Vec3f x = a.eyes[la + i], y = b.eyes[lb + i];
    CHECK_MSG(std::fabs(x.x - y.x) <= 0.125f && std::fabs(x.y - y.y) <= 0.125f &&
                  std::fabs(x.z - y.z) <= 0.125f,
              "frame " + std::to_string(i) + ": " + std::to_string(x.x) + "," +
                  std::to_string(x.y) + "," + std::to_string(x.z) + " vs " + std::to_string(y.x) +
                  "," + std::to_string(y.y) + "," + std::to_string(y.z));
    CHECK(same_angle(b.angles[lb + i].y, a.angles[la + i].y, 1.5f));
    CHECK(b.health[lb + i] == a.health[la + i] && b.rockets[lb + i] == a.rockets[la + i]);
    CHECK(b.rocket_seen[lb + i] == a.rocket_seen[la + i]);
  }
}

TEST(a_pair_that_does_not_exist_and_a_damaged_demo_are_refused) {
  cl::DemoConvert c;
  std::string error;
  CHECK(!cl::convert_begin(c, cl::DemoFormat::sequel, cl::DemoFormat::single, {}, error));
  CHECK(cl::convert_supported(cl::DemoFormat::multiview, cl::DemoFormat::single));
  CHECK(cl::convert_supported(cl::DemoFormat::client, cl::DemoFormat::single));
  CHECK(error.find("does not convert") != std::string::npos);
  std::vector<uint8_t> dem = cl::write_single_demo(script());
  // The first frame block's first message id made one the protocol does not have.
  const size_t header = 3;
  uint32_t len = 0;
  std::memcpy(&len, &dem[header], 4);
  dem[header + 16 + len + 16] = 99;
  CHECK(cl::convert_begin(c, cl::DemoFormat::single, cl::DemoFormat::multiview, {}, error));
  cl::convert_feed(c, dem);
  cl::convert_end(c);
  std::vector<uint8_t> out;
  CHECK(!cl::convert_take(c, out, error));
  CHECK_MSG(error.find("message id 99") != std::string::npos, error);
  CHECK(!cl::convert_take(c, out, error));  // it stays stopped
}

TEST(a_sequel_recording_of_protocol_26_becomes_protocol_34_byte_for_byte) {
  const std::vector<uint8_t> dm2 = cl::sequel_fixture(4, 7);
  const std::vector<uint8_t> old = cl::sequel_as_protocol_26(dm2);
  CHECK(old.size() < dm2.size());
  for (const std::vector<uint8_t>* from : {&old, &dm2}) {
    std::vector<uint8_t> out;
    cl::ConvertNotes notes;
    std::string error;
    CHECK_MSG(cl::convert_demo(*from, cl::DemoFormat::sequel, {}, out, notes, error), error);
    CHECK(out == dm2);  // 34 comes back as it was; 26 becomes it
    CHECK(notes.frames == 40);
    CHECK(notes.rewritten.contains("a frame given its suppress count") == (from == &old));
  }
  // As a stream a block at a time, the loop's form without the end.
  cl::DemoConvert c;
  std::string error;
  cl::ConvertOptions loop;
  loop.no_end = true;
  CHECK(cl::convert_begin(c, cl::DemoFormat::sequel, cl::DemoFormat::sequel, loop, error));
  std::vector<uint8_t> all;
  for (size_t at = 0; at < old.size(); at += 5) {
    cl::convert_feed(c, std::span(old).subspan(at, std::min<size_t>(5, old.size() - at)));
    CHECK_MSG(cl::convert_take(c, all, error), error);
  }
  cl::convert_end(c);
  CHECK(cl::convert_take(c, all, error));
  CHECK(all.size() == dm2.size() - 4);
  CHECK(std::equal(all.begin(), all.end(), dm2.begin()));
}

// A converter's banner (a print) before the server data still opens as a multiview recording.
TEST(a_multiview_recording_opening_with_a_print_reads) {
  cl::ConvertNotes notes;
  std::vector<uint8_t> mvd = converted(cl::write_single_demo(script()), notes);
  const std::vector<uint8_t> banner = {8, 3, 'c', 'o', 'n', 'v', 'e', 'r', 't', 'e', 'd', '\n', 0};
  uint32_t len = 0;
  std::memcpy(&len, &mvd[2], 4);
  len += static_cast<uint32_t>(banner.size());
  std::memcpy(&mvd[2], &len, 4);
  mvd.insert(mvd.begin() + 6, banner.begin(), banner.end());
  const Frames f = read_frames(mvd, cl::DemoFormat::multiview, 33);
  CHECK_MSG(f.end == cl::DemoRead::end, f.error);
  CHECK(f.last.map == "halls" && f.eyes.size() == script().origins.size());
}

TEST(a_files_format_is_told_by_its_name) {
  cl::DemoFormat f{};
  CHECK(cl::format_of_name("demos/a.dem", f) && f == cl::DemoFormat::single);
  CHECK(cl::format_of_name("A" + cl::plain_extension(cl::DemoFormat::client), f) &&
        f == cl::DemoFormat::client);
  CHECK(cl::format_of_name("m.mvdz", f) && f == cl::DemoFormat::multiview);
  CHECK(cl::format_of_name("m.mvd.gz", f) && f == cl::DemoFormat::multiview);
  CHECK(cl::format_of_name("x.dm2", f) && f == cl::DemoFormat::sequel);
  CHECK(!cl::format_of_name("x.txt", f));
}

}  // namespace
