// Recorded games written in code (demo_write.hpp).
//
// ge:summary The demo writers: a scripted eye path as single-player blocks or client records
// ge:keywords write_single_demo write_client_demo loop_script
// ge:skill shooter testing
// ge:test tests/shooters_edge_demo_test.cpp
#include "shooters_edge/client/demo_write.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/libm/libm.hpp"

namespace shooters_edge::client {

namespace {

namespace sh = ge::shooter;
namespace libm = ge::core::libm;

struct Out {
  std::vector<uint8_t> b;
  void u8(int v) { b.push_back(static_cast<uint8_t>(v)); }
  void i16(int v) {
    u8(v & 0xFF);
    u8((v >> 8) & 0xFF);
  }
  void i32(int32_t v) {
    const auto u = static_cast<uint32_t>(v);
    for (int i = 0; i < 4; i++) u8(static_cast<int>((u >> (8 * i)) & 0xFF));
  }
  void f32(float f) {
    uint32_t u = 0;
    std::memcpy(&u, &f, sizeof u);
    i32(static_cast<int32_t>(u));
  }
  void str(const std::string& s) {
    b.insert(b.end(), s.begin(), s.end());
    u8(0);
  }
  void coord(float f) { i16(static_cast<int>(std::lround(f * 8))); }
  void angle(float deg) { u8(static_cast<int>(std::lround(deg * 256 / 360)) & 255); }
  void append(const Out& o) { b.insert(b.end(), o.b.begin(), o.b.end()); }
};

sh::Vec3f at(const std::vector<sh::Vec3f>& v, size_t i) {
  return v.empty() ? sh::Vec3f{} : v[std::min(i, v.size() - 1)];
}

// --- the single-player protocol ---

void single_block(Out& file, const Out& msg, sh::Vec3f view) {
  file.i32(static_cast<int32_t>(msg.b.size()));
  file.f32(view.x);
  file.f32(view.y);
  file.f32(view.z);
  file.append(msg);
}

// --- the multiplayer protocol ---

void client_packet(Out& file, float t, uint32_t seq, const Out& msg) {
  file.f32(t);
  file.u8(1);
  file.i32(static_cast<int32_t>(msg.b.size() + 8));
  file.i32(static_cast<int32_t>(seq));
  file.i32(static_cast<int32_t>(seq));  // the acknowledgement
  file.append(msg);
}

}  // namespace

std::vector<uint8_t> write_single_demo(const DemoScript& s) {
  Out file;
  const std::string track = "-1\n";
  file.b.assign(track.begin(), track.end());
  const size_t frames = s.origins.size();

  Out sign;    // the level's start
  sign.u8(4);  // version
  sign.i32(15);
  sign.u8(11);  // serverinfo
  sign.i32(15);
  sign.u8(1);
  sign.u8(0);
  sign.str("The Halls");
  sign.str("maps/" + s.map + ".bsp");
  for (const char* m : {"progs/player.mdl", "progs/missile.mdl"}) sign.str(m);
  sign.str("");
  for (const char* snd : {"weapons/rocket1i.wav", "misc/water1.wav"}) sign.str(snd);
  sign.str("");
  sign.u8(32);  // cdtrack
  sign.u8(2);
  sign.u8(2);
  sign.u8(5);  // setview
  sign.i16(1);
  sign.u8(12);  // lightstyle
  sign.u8(0);
  sign.str("m");
  for (int ent : {1, 2}) {  // spawnbaseline: the player, then a rocket
    sign.u8(22);
    sign.i16(ent);
    sign.u8(ent == 1 ? 2 : 3);
    for (int k = 0; k < 3; k++) sign.u8(0);
    for (int k = 0; k < 3; k++) {
      sign.coord(0);
      sign.angle(0);
    }
  }
  sign.u8(29);  // spawnstaticsound
  for (int k = 0; k < 3; k++) sign.coord(16);
  sign.u8(2);
  sign.u8(255);
  sign.u8(64);
  sign.u8(25);  // signonnum
  sign.u8(3);
  single_block(file, sign, at(s.angles, 0));

  for (size_t i = 0; i < frames; i++) {
    Out m;
    m.u8(7);  // time
    m.f32(static_cast<float>(i) * s.frame_s);
    m.u8(15);  // clientdata: the view height, the items, the armour
    m.i16(1 | (1 << 9) | (1 << 13));
    m.u8(22);
    m.i32(s.items);
    m.u8(100);
    m.i16(s.health);
    m.u8(s.rockets);
    for (int k = 0; k < 4; k++) m.u8(k == 2 ? s.rockets : 10);
    m.u8(s.active_weapon);
    const sh::Vec3f o = at(s.origins, i), a = at(s.angles, i);
    // The player's update: origin and angles, with the more-bits byte.
    m.u8(0x80 | 1 | 2 | 4 | 8 | 16);
    m.u8((1 << 8 | 1 << 9) >> 8);
    m.u8(1);
    m.coord(o.x);
    m.angle(a.x);
    m.coord(o.y);
    m.angle(a.y);
    m.coord(o.z);
    m.angle(0);
    if (i % 10 == 5) {  // a rocket in flight, its sound and its blast
      m.u8(0x80 | 2 | 4 | 8);
      m.u8(2);
      m.coord(o.x + 64);
      m.coord(o.y);
      m.coord(o.z);
      m.u8(6);  // sound: the volume byte, the channel, the sound, the point
      m.u8(1);
      m.u8(200);
      m.i16(2 << 3 | 1);
      m.u8(1);
      for (int k = 0; k < 3; k++) m.coord(o.x);
      m.u8(23);  // temp entity: an explosion
      m.u8(3);
      for (int k = 0; k < 3; k++) m.coord(o.y);
    }
    if (i == frames / 2) {
      m.u8(8);  // print
      m.str("halfway\n");
    }
    single_block(file, m, a);
  }
  Out end;
  end.u8(2);  // disconnect
  single_block(file, end, at(s.angles, frames));
  return file.b;
}

std::vector<uint8_t> write_client_demo(const DemoScript& s, int cmd_size) {
  Out file;
  uint32_t seq = 0;
  const size_t frames = s.origins.size();

  Out sign;
  sign.u8(11);  // serverdata, no extensions
  sign.i32(28);
  sign.i32(1);  // the spawn count
  sign.str("baseline");
  sign.u8(0);  // the player's slot
  sign.str("The Halls");
  for (int k = 0; k < 10; k++) sign.f32(k == 0 ? 800.0f : 320.0f);
  sign.u8(9);  // stufftext
  sign.str("fullserverinfo \"\\maxfps\\77\"\n");
  client_packet(file, 0, ++seq, sign);
  Out lists;
  lists.u8(45);  // modellist
  lists.u8(0);
  for (const std::string& m : {"maps/" + s.map + ".bsp", std::string("progs/player.mdl"),
                               std::string("progs/missile.mdl")})
    lists.str(m);
  lists.str("");
  lists.u8(0);
  lists.u8(46);  // soundlist
  lists.u8(0);
  lists.str("weapons/rocket1i.wav");
  lists.str("");
  lists.u8(0);
  client_packet(file, 0, ++seq, lists);
  Out base;
  base.u8(22);  // spawnbaseline: a rocket
  base.i16(40);
  base.u8(3);
  for (int k = 0; k < 3; k++) base.u8(0);
  for (int k = 0; k < 3; k++) {
    base.coord(0);
    base.angle(0);
  }
  for (const std::pair<int, int>& st :
       {std::pair{0, s.health}, {6, 10}, {8, s.rockets}, {10, s.active_weapon}}) {
    base.u8(3);  // updatestat
    base.u8(st.first);
    base.u8(st.second);
  }
  base.u8(38);  // updatestatlong: the items
  base.u8(15);
  base.i32(s.items);
  client_packet(file, 0, ++seq, base);
  file.f32(0);
  file.u8(2);  // the sequences
  file.i32(static_cast<int32_t>(seq));
  file.i32(static_cast<int32_t>(seq));

  for (size_t i = 0; i < frames; i++) {
    const float t = static_cast<float>(i) * s.frame_s;
    const sh::Vec3f o = at(s.origins, i), a = at(s.angles, i);
    file.f32(t);
    file.u8(0);  // the command: the struct (msec, padding, angles, moves, buttons, impulse)
    Out cmd;
    cmd.u8(13);
    for (int k = 0; k < 3; k++) cmd.u8(0);
    cmd.f32(a.x);
    cmd.f32(a.y);
    cmd.f32(a.z);
    cmd.i16(400);
    cmd.i16(0);
    cmd.i16(0);
    cmd.u8(0);
    cmd.u8(0);
    while (static_cast<int>(cmd.b.size()) < cmd_size) cmd.u8(0);
    file.append(cmd);
    file.f32(a.x);
    file.f32(a.y);
    file.f32(a.z);

    Out m;
    m.u8(42);  // playerinfo: the slot, the flags (msec, velocity x), the origin, the frame
    m.u8(0);
    m.i16(0x1 | 0x4);
    m.coord(o.x);
    m.coord(o.y);
    m.coord(o.z);
    m.u8(6);
    m.u8(13);
    m.i16(320);
    // The rocket: a full list on the first frame, a delta list after it.
    if (i == 0) {
      m.u8(47);
    } else {
      m.u8(48);
      m.u8(static_cast<int>(seq & 63));
    }
    m.i16(40 | 1 << 9);  // entity 40, origin x
    m.coord(o.x + static_cast<float>(i));
    m.i16(0);
    if (i % 10 == 5) {
      m.u8(23);  // temp entity: an explosion
      m.u8(3);
      for (int k = 0; k < 3; k++) m.coord(o.z);
    }
    client_packet(file, t, ++seq, m);
  }
  Out end;
  end.u8(2);  // disconnect
  client_packet(file, static_cast<float>(frames) * s.frame_s, ++seq, end);
  return file.b;
}

DemoScript loop_script(const std::string& map, sh::Vec3f centre, float radius, int frames) {
  DemoScript s;
  s.map = map;
  constexpr double kTau = 6.283185307179586;
  for (int i = 0; i < frames; i++) {
    const double a = kTau * i / frames;
    const auto eighth = [](double v) { return static_cast<float>(std::round(v * 8) / 8); };
    s.origins.push_back({eighth(centre.x + radius * libm::cos(a)),
                         eighth(centre.y + radius * libm::sin(a)), centre.z});
    // Looking along the loop (yaw counter-clockwise from +x), the pitch swaying 10 degrees.
    const double yaw = a * 360 / kTau + 90;
    s.angles.push_back({static_cast<float>(10 * libm::sin(2 * a)),
                        static_cast<float>(yaw - 360 * std::floor(yaw / 360)), 0});
  }
  return s;
}

}  // namespace shooters_edge::client
