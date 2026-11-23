// games/shooters_edge/net/q2_proto_in.cpp: Quake II's protocols 35 and 36 read as protocol 34. A
// long solid packs back to the short box it came from; the server data loses its tail and keeps
// its hacks; a gamestate, a compressed packet and the streams come out as plain configstrings and
// baselines the recording's reader takes; a frame's split fields come back whole, its 16-bit
// angles as bytes; a compressed download is whole once its last piece is in.
//
// ge:summary Test: Quake II's protocols 35 and 36 rewritten as protocol 34
// ge:keywords test q2 protocol-35 protocol-36 zpacket zdownload gamestate long-solid angle16
// ge:skill net shooter testing
#include <cstdint>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/deflate.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/net/q2_proto_in.hpp"
#include "shooters_edge/server/q2_wire.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace cn = shooters_edge::net;
namespace sv = shooters_edge::server;

void u8(std::vector<uint8_t>& b, int v) { b.push_back(static_cast<uint8_t>(v)); }
void i16(std::vector<uint8_t>& b, int v) {
  u8(b, v & 0xFF);
  u8(b, (v >> 8) & 0xFF);
}
void i32(std::vector<uint8_t>& b, int64_t v) {
  for (int k = 0; k < 4; k++) u8(b, static_cast<int>((v >> (8 * k)) & 0xFF));
}
void str(std::vector<uint8_t>& b, const std::string& s) {
  b.insert(b.end(), s.begin(), s.end());
  u8(b, 0);
}

// A protocol 36 server data at `minor` with the strafe-jump hack.
std::vector<uint8_t> server_data_36(int minor) {
  std::vector<uint8_t> b;
  u8(b, 12);
  i32(b, 36);
  i32(b, 7);
  u8(b, 0);
  str(b, "baseq2");
  i16(b, 2);
  str(b, "The Edge");
  sv::Q2WireOptions opt;
  opt.protocol = 36;
  opt.minor = minor;
  sv::q2_server_data_tail(b, opt);
  if (minor >= 1024) b[b.size() - 2] = 1;  // the flags: the strafe-jump hack
  return b;
}

// The blocks read by the recording's reader from a fresh stream; false with the reader's error.
bool read_all(cl::Demo& d, const std::vector<std::vector<uint8_t>>& blocks, std::string& error) {
  for (const auto& blk : blocks) {
    i32(d.bytes, static_cast<int64_t>(blk.size()));
    d.bytes.insert(d.bytes.end(), blk.begin(), blk.end());
  }
  for (;;) {
    const cl::DemoRead got = cl::next_q2(d, error);
    if (got == cl::DemoRead::error) return false;
    if (got == cl::DemoRead::end) return true;
  }
}

}  // namespace

TEST(a_long_solid_packs_back_to_its_box) {
  CHECK(cn::q2_short_solid(0) == 0);
  CHECK(cn::q2_short_solid(31) == 31);
  for (const int solid : {2 | (3 << 5) | (8 << 10), 16 | (24 << 5) | (40 << 10), 1 | (4 << 10)})
    CHECK_MSG(cn::q2_short_solid(static_cast<uint32_t>(sv::q2_long_solid(solid))) == solid,
              std::to_string(solid));
}

TEST(the_server_data_loses_its_tail_and_keeps_the_hacks) {
  cn::Q2ProtoIn p;
  std::vector<std::vector<uint8_t>> blocks;
  std::string error;
  CHECK_MSG(cn::q2_proto_in(p, server_data_36(1024), blocks, error), error);
  CHECK(p.protocol == 36 && p.minor == 1024);
  CHECK(p.strafe_hack && !p.qw_move && !p.waterjump_hack);
  CHECK(p.server_state == 2);
  cl::Demo d;
  cl::q2_stream_begin(d);
  CHECK_MSG(read_all(d, blocks, error), error);
  CHECK(d.state.protocol == 34);
  CHECK(d.state.game == "baseq2");
  CHECK(d.state.player == 2);
}

TEST(a_gamestate_and_the_streams_come_out_as_configstrings_and_baselines) {
  sv::Q2Signon parts;
  {
    std::vector<uint8_t> m;
    u8(m, 13);
    i16(m, 33);
    str(m, "maps/edge.bsp");
    parts.config_strings.emplace_back(33, m);
  }
  {
    sv::Q2EntityState s;
    s.model[0] = 3;
    s.origin[0] = 800;
    std::vector<uint8_t> m{14};
    sv::q2_write_entity(m, sv::q2_entity_flags({}, s, 5, false), 5, s);
    parts.baselines.emplace_back(5, m);
  }
  for (const int minor : {1015, 1024}) {
    std::vector<std::vector<uint8_t>> signon;
    sv::q2_signon_streams(parts, minor, signon);
    cn::Q2ProtoIn p;
    std::vector<std::vector<uint8_t>> blocks;
    std::string error;
    CHECK_MSG(cn::q2_proto_in(p, server_data_36(minor), blocks, error), error);
    for (const auto& m : signon) CHECK_MSG(cn::q2_proto_in(p, m, blocks, error), error);
    cl::Demo d;
    cl::q2_stream_begin(d);
    CHECK_MSG(read_all(d, blocks, error), error);
    CHECK(d.state.map == "edge");
    CHECK(d.baselines.size() > 5 && d.baselines[5].model == 3);
    CHECK(d.baselines[5].origin.x == 100.0f);
  }
}

TEST(a_compressed_packet_is_read_again_and_never_one_inside_another) {
  std::vector<uint8_t> inner;
  u8(inner, 13);
  i16(inner, 33);
  str(inner, "maps/zip.bsp");
  const std::vector<uint8_t> packed = ge::core::deflate_raw(inner);
  std::vector<uint8_t> z;
  u8(z, 21);
  i16(z, static_cast<int>(packed.size()));
  i16(z, static_cast<int>(inner.size()));
  z.insert(z.end(), packed.begin(), packed.end());
  cn::Q2ProtoIn p;
  std::vector<std::vector<uint8_t>> blocks;
  std::string error;
  CHECK_MSG(cn::q2_proto_in(p, server_data_36(1024), blocks, error), error);
  CHECK_MSG(cn::q2_proto_in(p, z, blocks, error), error);
  cl::Demo d;
  cl::q2_stream_begin(d);
  CHECK_MSG(read_all(d, blocks, error), error);
  CHECK(d.state.map == "zip");
  // A compressed packet holding another is refused.
  const std::vector<uint8_t> twice = ge::core::deflate_raw(z);
  std::vector<uint8_t> outer;
  u8(outer, 21);
  i16(outer, static_cast<int>(twice.size()));
  i16(outer, static_cast<int>(z.size()));
  outer.insert(outer.end(), twice.begin(), twice.end());
  CHECK(!cn::q2_proto_in(p, outer, blocks, error));
  CHECK_MSG(error.find("inside a compressed packet") != std::string::npos, error);
}

TEST(a_frames_split_fields_come_back_whole) {
  cn::Q2ProtoIn p;
  std::vector<std::vector<uint8_t>> blocks;
  std::string error;
  CHECK_MSG(cn::q2_proto_in(p, server_data_36(1024), blocks, error), error);
  sv::Q2Wire w;
  w.opt.protocol = 36;
  w.opt.minor = 1024;
  w.asked.recording = true;
  w.slot = 2;
  sv::Q2PlayerState a;
  a.origin[0] = 80;
  a.origin[2] = 200;
  a.view_angles[1] = 16384;
  a.stats[1] = 100;
  a.gun_index = 4;
  std::vector<uint8_t> f1;
  sv::q2_write_frame_top_ext(f1, w, 10, a);
  // One entity with a 16-bit yaw (bit 13) and a long solid.
  sv::Q2EntityState e;
  e.model[0] = 2;
  e.origin[1] = 64;
  e.solid = 31;
  uint32_t flags = sv::q2_entity_flags({}, e, 7, false) | (1u << 2) | (1u << 13);
  std::vector<uint8_t> ent;
  {  // header (the chain bits by hand: bits up to 27), number, fields in the later form
    flags |= (1u << 7) | (1u << 15) | (1u << 23);
    for (int k = 0; k < 4; k++) u8(ent, static_cast<int>((flags >> (8 * k)) & 0xFF));
    u8(ent, 7);
    if (flags & (1u << 11)) u8(ent, 2);    // model
    if (flags & (1u << 1)) i16(ent, 64);   // origin y
    i16(ent, 16384);                       // the yaw, a quarter turn as a 16-bit angle
    if (flags & (1u << 27)) i32(ent, 31);  // the long solid
  }
  f1.insert(f1.end(), ent.begin(), ent.end());
  u8(f1, 0);
  u8(f1, 0);
  CHECK_MSG(cn::q2_proto_in(p, f1, blocks, error), error);
  // The next frame moves only the origin's height: the later form sends z alone.
  w.last_frame = 10;
  sv::Q2PlayerState b = a;
  b.origin[2] = 216;
  std::vector<uint8_t> f2;
  sv::q2_write_frame_top_ext(f2, w, 11, b);
  u8(f2, 0);
  u8(f2, 0);
  CHECK_MSG(cn::q2_proto_in(p, f2, blocks, error), error);
  cl::Demo d;
  cl::q2_stream_begin(d);
  CHECK_MSG(read_all(d, blocks, error), error);
  CHECK_MSG(d.state.frames == 2,
            std::to_string(d.state.frames) + " frames, last " + std::to_string(d.last_frame));
  CHECK(d.last_frame == 11);  // the second frame found its delta
  const cl::DemoEntity& me = d.state.players[2];
  CHECK_MSG(me.origin.x == 10.0f && me.origin.z == 27.0f,
            std::to_string(me.origin.x) + " " + std::to_string(me.origin.z));
  CHECK(d.state.q2_stats[1] == 100);
  CHECK(d.state.stats[cl::kStatWeapon] == 4);
  CHECK(d.state.entities.size() > 7 && d.state.entities[7].present);
  CHECK(d.state.entities[7].angles.y == 90.0f);
  CHECK(d.state.entities[7].solid == 31);
  CHECK(p.client_number == 2);
}

TEST(a_compressed_download_is_whole_once_its_last_piece_is_in) {
  std::vector<uint8_t> file(5000);
  for (size_t i = 0; i < file.size(); i++) file[i] = static_cast<uint8_t>((i * 7) % 251);
  ge::core::DeflateStream z;
  z.wrap = ge::core::DeflateWrap::raw;
  std::vector<uint8_t> first, second;
  ge::core::deflate_sync(z, std::span<const uint8_t>(file.data(), 3000), first);
  ge::core::deflate_sync(z, std::span<const uint8_t>(file.data() + 3000, 2000), second);
  ge::core::deflate_finish(z, second);
  cn::Q2ProtoIn p;
  std::vector<std::vector<uint8_t>> blocks;
  std::string error;
  CHECK_MSG(cn::q2_proto_in(p, server_data_36(1026), blocks, error), error);
  const auto piece = [&](const std::vector<uint8_t>& bytes, int percent) {
    std::vector<uint8_t> m;
    u8(m, 22);
    i16(m, static_cast<int>(bytes.size()));
    u8(m, percent);
    m.insert(m.end(), bytes.begin(), bytes.end());
    return m;
  };
  CHECK_MSG(cn::q2_proto_in(p, piece(first, 60), blocks, error), error);
  CHECK(!p.download_ready);
  CHECK_MSG(cn::q2_proto_in(p, piece(second, 100), blocks, error), error);
  CHECK(p.download_ready);
  CHECK(p.download == file);
  // The client is handed two empty pieces with their percents (each asks for the next).
  cl::Demo d;
  cl::q2_stream_begin(d);
  CHECK_MSG(read_all(d, blocks, error), error);
  CHECK(d.download_pieces.size() == 2);
  CHECK(d.download_pieces[0].percent == 60 && d.download_pieces[1].percent == 100);
}

TEST(a_setting_is_kept_and_a_bad_message_refused) {
  cn::Q2ProtoIn p;
  std::vector<std::vector<uint8_t>> blocks;
  std::string error;
  CHECK_MSG(cn::q2_proto_in(p, server_data_36(1024), blocks, error), error);
  std::vector<uint8_t> s;
  u8(s, 24);
  i32(s, 1);
  i32(s, 40);
  CHECK_MSG(cn::q2_proto_in(p, s, blocks, error), error);
  CHECK(p.server_fps == 40);
  CHECK(!cn::q2_proto_in(p, std::vector<uint8_t>{0x2A}, blocks, error));  // 10 with a top bit
  CHECK(!error.empty());
}

namespace {

// A 23-bit coordinate in its absolute form (three bytes, low bit 1).
void coord_abs(std::vector<uint8_t>& b, int v) {
  const auto w = (static_cast<uint32_t>(v) << 1) | 1u;
  u8(b, static_cast<int>(w & 0xFF));
  u8(b, static_cast<int>((w >> 8) & 0xFF));
  u8(b, static_cast<int>((w >> 16) & 0xFF));
}

}  // namespace

// Under the extended limits (the server data's flags) the stream becomes the recording of that
// level: the sounds' configstrings at their extended place, an entity past 1024 far out with its
// alpha, the player's 23-bit origin, a word gun index and a stat under the varint mask.
TEST(extended_limits_come_out_as_the_recording_of_their_level) {
  for (const int minor : {1025, 1026}) {
    std::vector<uint8_t> sd = server_data_36(minor);
    sd[sd.size() - 2] = 1 | 8 | 16;  // the strafe hack, extensions and extensions 2
    cn::Q2ProtoIn p;
    std::vector<std::vector<uint8_t>> blocks;
    std::string error;
    CHECK_MSG(cn::q2_proto_in(p, sd, blocks, error), error);
    CHECK(p.ext == (minor == 1026 ? 3 : 2) && p.strafe_hack);
    std::vector<uint8_t> cs;  // the configstring stream: a sound, then the extended end
    u8(cs, 25);
    i16(cs, 8254 + 1);
    str(cs, "misc/far.wav");
    i16(cs, 13630);
    CHECK_MSG(cn::q2_proto_in(p, cs, blocks, error), error);
    std::vector<uint8_t> pr;  // a print at the extended centre level
    u8(pr, 10);
    u8(pr, 5);
    str(pr, "Round two");
    CHECK_MSG(cn::q2_proto_in(p, pr, blocks, error), error);
    std::vector<uint8_t> f;   // a frame: origin (x, y) and the gun under the flags; z and stats
    u8(f, 20 | (2 << 5));     // extra flags' bit 5 (stats) in the id's top bits
    i32(f, 10 | (31 << 27));  // frame 10, no delta
    u8(f, 0x80);              // extra flags' bit 3 (origin z) in the high nibble
    u8(f, 0);                 // no areas
    i16(f, (1 << 1) | (1 << 12) | (minor == 1026 ? (1 << 15) : 0));
    if (minor == 1026) u8(f, 1);  // the fog level's third flags byte: fog
    coord_abs(f, 80);             // origin x: 10 units
    i16(f, 0);                    // origin y: no change
    coord_abs(f, 70000);          // origin z: 8750 units, past the ordinary wire's reach
    i16(f, 300);                  // the gun's index, a word
    if (minor == 1026) {          // the fog after the blend's place: a colour, read past
      u8(f, 1);
      u8(f, 10);
      u8(f, 20);
      u8(f, 30);
    }
    u8(f, 1 << 3);  // the stats' mask as a varint: stat 3
    i16(f, 77);
    sv::Q2EntityState e;  // entity 1500 far out, half seen
    e.model[0] = 2;
    e.origin[0] = 72000;
    e.alpha = 128;
    sv::q2_write_entity_ext(f, sv::q2_entity_flags_ext({}, e, 1500, false, 2), 1500, {}, e, 2);
    u8(f, 0);
    u8(f, 0);
    CHECK_MSG(cn::q2_proto_in(p, f, blocks, error), error);
    CHECK(!blocks.empty() && blocks[0].size() > 5);
    int32_t written = 0;
    for (int k = 0; k < 4; k++) written |= blocks[0][1 + k] << (8 * k);
    CHECK_MSG(written == 3435, std::to_string(written));
    cl::Demo d;
    cl::q2_stream_begin(d);
    CHECK_MSG(read_all(d, blocks, error), error);
    CHECK(d.state.sounds.size() > 1 && d.state.sounds[1] == "misc/far.wav");
    CHECK(d.state.centre == "Round two");
    const cl::DemoEntity& me = d.state.players[2];
    CHECK_MSG(me.origin.x == 10.0f && me.origin.z == 8750.0f,
              std::to_string(me.origin.x) + " " + std::to_string(me.origin.z));
    CHECK(d.state.stats[cl::kStatWeapon] == 300);
    CHECK(d.state.q2_stats[3] == 77);
    CHECK(d.state.entities.size() > 1500);
    if (d.state.entities.size() > 1500) {
      const cl::DemoEntity& far = d.state.entities[1500];
      CHECK(far.present && far.origin.x == 9000.0f && far.alpha < 0.6f);
    }
  }
  // The second extensions without the first are refused.
  std::vector<uint8_t> bad = server_data_36(1025);
  bad[bad.size() - 2] = 16;
  cn::Q2ProtoIn p;
  std::vector<std::vector<uint8_t>> blocks;
  std::string error;
  CHECK(!cn::q2_proto_in(p, bad, blocks, error));
  CHECK_MSG(error.find("second protocol extensions") != std::string::npos, error);
}
