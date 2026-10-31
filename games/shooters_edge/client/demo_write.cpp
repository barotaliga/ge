// Recorded games written in code (demo_write.hpp).
//
// ge:summary The demo writers: a scripted eye path as single-player blocks or client records
// ge:keywords write_single_demo write_client_demo loop_script
// ge:skill shooter testing
// ge:test tests/shooters_edge_demo_test.cpp
#include "shooters_edge/client/demo_write.hpp"

#include <algorithm>
#include <array>
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

// --- the multiview recording ---

namespace {

// A record: the milliseconds, the kind and player, then (for kinds with messages) the mask of a
// multiple, the length and the messages.
void multiview_record(Out& file, int ms, int kind, int target, const Out& msg, uint32_t mask = 0) {
  file.u8(ms);
  file.u8(kind | target << 3);
  if (kind == 3) file.i32(static_cast<int32_t>(mask));
  file.i32(static_cast<int32_t>(msg.b.size()));
  file.append(msg);
}

// The recorder's playerinfo: the slot, the field bits, the frame, then the fields named.
void multiview_player(Out& m, int slot, sh::Vec3f o, sh::Vec3f a, int bits) {
  m.u8(42);
  m.u8(slot);
  m.i16(bits);
  m.u8(6);
  const float ov[3] = {o.x, o.y, o.z}, av[3] = {a.x, a.y, a.z};
  for (int k = 0; k < 3; k++)
    if ((bits & (1 << k)) != 0) m.coord(ov[k]);
  for (int k = 0; k < 3; k++)
    if ((bits & (8 << k)) != 0) m.i16(static_cast<int>(std::lround(av[k] * 65536 / 360)));
  if ((bits & 2048) != 0) m.u8(2);  // model
  if ((bits & 128) != 0) m.u8(0);   // skin
  if ((bits & 64) != 0) m.u8(0);    // effects
  if ((bits & 1024) != 0) m.u8(0);  // weapon frame
}

void multiview_stats(Out& m, int health, int rockets, int weapon, int items) {
  for (const std::pair<int, int>& st : {std::pair{0, health}, {8, rockets}, {10, weapon}}) {
    m.u8(3);  // updatestat
    m.u8(st.first);
    m.u8(st.second);
  }
  m.u8(38);  // updatestatlong: the items
  m.u8(15);
  m.i32(items);
}

}  // namespace

std::vector<uint8_t> write_multiview_demo(const DemoScript& s) {
  Out file;
  const size_t frames = s.origins.size();
  constexpr int kAll = 6, kStats = 5, kSingle = 4, kMultiple = 3;

  Out sign;
  sign.u8(11);  // serverdata: the multiview family's hidden-messages bit, then the version
  sign.i32(0x3144564D);
  sign.i32(0x20);
  sign.i32(28);
  sign.i32(1);  // the spawn count
  sign.str("baseline");
  sign.f32(12.5f);  // the server's time where a client's slot would be
  sign.str("The Halls");
  for (int k = 0; k < 10; k++) sign.f32(k == 0 ? 800.0f : 320.0f);
  multiview_record(file, 0, kAll, 0, sign);
  Out lists;
  lists.u8(45);  // modellist
  lists.u8(0);
  for (const std::string& m : {"maps/" + s.map + ".bsp", std::string("progs/player.mdl"),
                               std::string("progs/missile.mdl")})
    lists.str(m);
  lists.str("");
  lists.u8(0);
  lists.u8(22);  // spawnbaseline: a rocket
  lists.i16(40);
  lists.u8(3);
  for (int k = 0; k < 3; k++) lists.u8(0);
  for (int k = 0; k < 3; k++) {
    lists.coord(0);
    lists.angle(0);
  }
  multiview_record(file, 0, kAll, 0, lists);
  // Each player's stats: the scripted player's, and a second player's that are not the view's.
  Out mine, other;
  multiview_stats(mine, s.health, s.rockets, s.active_weapon, s.items);
  multiview_stats(other, 37, 3, 1, 1);
  multiview_record(file, 0, kStats, 0, mine);
  multiview_record(file, 0, kStats, 3, other);

  const int ms = std::max(2, static_cast<int>(std::lround(s.frame_s * 1000)));
  for (size_t i = 0; i < frames; i++) {
    const sh::Vec3f o = at(s.origins, i), a = at(s.angles, i);
    const sh::Vec3f prev_o = at(s.origins, i == 0 ? 0 : i - 1);
    // The frame: the players and the entity list for everyone. The scripted player in slot 0
    // leaves out an origin that did not move; the second (slot 3) stands still after the first
    // frame, so it writes only its frame.
    Out m;
    int bits = 8 | 16 | 32;
    if (i == 0) bits |= 1 | 2 | 4 | 64 | 128 | 1024 | 2048;
    if (i > 0 && o.x != prev_o.x) bits |= 1;
    if (i > 0 && o.y != prev_o.y) bits |= 2;
    if (i > 0 && o.z != prev_o.z) bits |= 4;
    multiview_player(m, 0, o, a, bits);
    multiview_player(m, 3, {-64, 32, 24}, {0, 90, 0}, i == 0 ? 1 | 2 | 4 | 8 | 16 | 32 | 2048 : 0);
    if (i == 0) {
      m.u8(47);
    } else {
      m.u8(48);
      m.u8(static_cast<int>((i + 200) & 255));  // the previous frame's sequence, not a ring slot
    }
    m.i16(40 | 1 << 9);  // entity 40, origin x
    m.coord(o.x + static_cast<float>(i));
    m.i16(0);
    multiview_record(file, i == 0 ? 0 : ms, kAll, 0, m);
    Out fx;
    if (i % 10 == 5) {
      fx.u8(23);  // temp entity: an explosion
      fx.u8(3);
      for (int k = 0; k < 3; k++) fx.coord(o.z);
      fx.u8(10);  // setangle, turning the second player
      fx.u8(3);
      for (int k = 0; k < 3; k++) fx.angle(45);
      multiview_record(file, 0, kAll, 0, fx);
    }
    Out hidden;  // the recorder's own data: a block of type 1, which no reader shows
    hidden.i32(4);
    hidden.i16(1);
    hidden.i32(static_cast<int32_t>(i));
    multiview_record(file, 0, kMultiple, 0, hidden, 0);
    if (i == frames / 2) {
      Out to_other;  // a print to the second player alone, and his health
      to_other.u8(8);
      to_other.u8(2);
      to_other.str("for slot 3\n");
      multiview_record(file, 0, kSingle, 3, to_other);
      Out hurt;
      hurt.u8(3);
      hurt.u8(0);
      hurt.u8(12);
      multiview_record(file, 0, kStats, 3, hurt);
    }
  }
  Out end;
  end.u8(2);  // disconnect
  end.str("EndOfDemo");
  multiview_record(file, ms, kAll, 0, end);
  return file.b;
}

std::vector<uint8_t> write_multiview_match(const MatchScript& s) {
  Out file;
  constexpr int kAll = 6, kStats = 5;
  constexpr int kRocketBase = 100;  // the rockets' entity numbers
  const auto held = [](const std::vector<int>& v, size_t i, int none) {
    return v.empty() ? none : v[std::min(i, v.size() - 1)];
  };
  Out sign;
  sign.u8(11);  // serverdata
  sign.i32(0x3144564D);
  sign.i32(0x20);
  sign.i32(28);
  sign.i32(1);
  sign.str("baseline");
  sign.f32(0);
  sign.str("The Match");
  for (int k = 0; k < 10; k++) sign.f32(k == 0 ? 800.0f : 320.0f);
  multiview_record(file, 0, kAll, 0, sign);
  Out lists;
  lists.u8(45);  // modellist: the map, the player, the rocket (3)
  lists.u8(0);
  for (const std::string& m : {"maps/" + s.map + ".bsp", std::string("progs/player.mdl"),
                               std::string("progs/missile.mdl")})
    lists.str(m);
  lists.str("");
  lists.u8(0);
  for (size_t r = 0; r < s.rockets.size(); r++) {
    lists.u8(22);  // spawnbaseline: the rocket's model
    lists.i16(kRocketBase + static_cast<int>(r));
    lists.u8(3);
    for (int k = 0; k < 3; k++) lists.u8(0);
    for (int k = 0; k < 3; k++) {
      lists.coord(0);
      lists.angle(0);
    }
  }
  for (size_t k = 0; k < s.players.size(); k++) {
    lists.u8(40);  // updateuserinfo
    lists.u8(static_cast<int>(k));
    lists.i32(static_cast<int32_t>(k + 1));
    lists.str("\\name\\" + s.players[k].name + "\\team\\red");
  }
  multiview_record(file, 0, kAll, 0, lists);

  const int ms = std::max(2, static_cast<int>(std::lround(s.frame_s * 1000)));
  std::vector<std::array<int, 4>> last(s.players.size(), {-1000, -1000, -1000, -1000});
  for (int i = 0; i < s.frames; i++) {
    const auto f = static_cast<size_t>(i);
    Out m;
    for (size_t k = 0; k < s.players.size(); k++) {
      const MatchPlayer& p = s.players[k];
      const int health = held(p.health, f, 100);
      const int bits =
          1 | 2 | 4 | 8 | 16 | 32 | 2048 | (health <= 0 ? 256 : 0) | (health <= -40 ? 512 : 0);
      multiview_player(m, static_cast<int>(k), at(p.origins, f), {0, p.yaw, 0}, bits);
    }
    m.u8(47);  // packetentities: the rockets in flight, the whole list
    for (size_t r = 0; r < s.rockets.size(); r++) {
      const MatchRocket& rk = s.rockets[r];
      if (i < rk.from || i > rk.to) continue;
      const float u = rk.to == rk.from
                          ? 1.0f
                          : static_cast<float>(i - rk.from) / static_cast<float>(rk.to - rk.from);
      const auto eighth = [](float v) { return std::round(v * 8) / 8; };
      m.i16((kRocketBase + static_cast<int>(r)) | 1 << 9 | 1 << 10 | 1 << 11);
      m.coord(eighth(rk.start.x + (rk.end.x - rk.start.x) * u));
      m.coord(eighth(rk.start.y + (rk.end.y - rk.start.y) * u));
      m.coord(eighth(rk.start.z + (rk.end.z - rk.start.z) * u));
    }
    m.i16(0);
    for (size_t k = 0; k < s.players.size(); k++) {  // frags as they change
      const int frags = held(s.players[k].frags, f, 0);
      if (frags == last[k][3]) continue;
      last[k][3] = frags;
      m.u8(14);
      m.u8(static_cast<int>(k));
      m.i16(frags);
    }
    multiview_record(file, i == 0 ? 0 : ms, kAll, 0, m);
    for (size_t k = 0; k < s.players.size(); k++) {  // each player's stats as they change
      const MatchPlayer& p = s.players[k];
      const std::array<int, 3> now{held(p.health, f, 100), held(p.armour, f, 0),
                                   held(p.weapon, f, 32)};
      if (now[0] == last[k][0] && now[1] == last[k][1] && now[2] == last[k][2]) continue;
      Out st;
      for (int j = 0; j < 3; j++) {
        if (now[static_cast<size_t>(j)] == last[k][static_cast<size_t>(j)]) continue;
        st.u8(38);  // updatestatlong: health 0, armour 4, the weapon in hand 10
        st.u8(j == 0 ? 0 : j == 1 ? 4 : 10);
        st.i32(now[static_cast<size_t>(j)]);
        last[k][static_cast<size_t>(j)] = now[static_cast<size_t>(j)];
      }
      multiview_record(file, 0, kStats, static_cast<int>(k), st);
    }
  }
  Out end;
  end.u8(2);
  end.str("EndOfDemo");
  multiview_record(file, ms, kAll, 0, end);
  return file.b;
}

MatchScript highlight_match(const std::string& map) {
  MatchScript s;
  s.map = map;
  s.frame_s = 0.05f;
  s.frames = 600;
  const auto frame = [](double t) { return static_cast<size_t>(std::lround(t * 20)); };
  const size_t n = static_cast<size_t>(s.frames);
  const std::array<std::string, 4> names{"watched", "alpha", "bravo", "charlie"};
  // On the fixture halls (plugins/brush): the watched player in the hall's south-west corner
  // looking across it, two in the hall, one at the far end of the vault.
  const std::array<sh::Vec3f, 4> home{
      sh::Vec3f{40, 40, 24}, {400, 200, 24}, {430, 240, 24}, {735, 795, 24}};
  for (size_t k = 0; k < 4; k++) {
    MatchPlayer p;
    p.name = names[k];
    p.origins.assign(n, home[k]);
    p.health.assign(n, 100);
    p.armour.assign(n, 0);
    p.weapon.assign(n, 32);  // the rocket launcher
    p.frags.assign(n, 0);
    p.yaw = k == 0 ? 35.0f : 225.0f;
    s.players.push_back(p);
  }
  // A death of `victim` at `t` by `killer` (-1: none), health `health` for 2 s, then back.
  const auto die = [&](int victim, int killer, double t, int health) {
    MatchPlayer& v = s.players[static_cast<size_t>(victim)];
    for (size_t i = frame(t); i < std::min(n, frame(t + 2)); i++) v.health[i] = health;
    if (killer >= 0) {
      MatchPlayer& k = s.players[static_cast<size_t>(killer)];
      for (size_t i = frame(t); i < n; i++) k.frags[i]++;
    }
  };
  // 5 s: one rocket, two kills.
  s.rockets.push_back(
      {static_cast<int>(frame(4.6)), static_cast<int>(frame(5.0)), {40, 40, 40}, {410, 220, 30}});
  die(1, 0, 5.0, -50);
  die(2, 0, 5.0, -10);
  // 10 s: a long rocket.
  s.rockets.push_back(
      {static_cast<int>(frame(8.5)), static_cast<int>(frame(10.0)), {40, 40, 40}, {730, 790, 30}});
  die(3, 0, 10.0, -20);
  // 15 s: slot 1 jumps (up 100 units over 14..15 s) and is gibbed in the air.
  for (size_t i = frame(14.0); i <= frame(15.0); i++)
    s.players[1].origins[i].z = 24 + static_cast<float>(i - frame(14.0)) * 5;
  s.rockets.push_back({static_cast<int>(frame(14.6)),
                       static_cast<int>(frame(15.0)),
                       {40, 40, 40},
                       {400, 200, 120}});
  die(1, 0, 15.0, -60);
  // 20 s: slot 2 with 100 health and 100 armour beamed down in 1.5 s.
  for (size_t i = frame(17.0); i < frame(18.5); i++) s.players[2].armour[i] = 100;
  for (size_t i = frame(18.5); i < frame(20.0); i++) {
    const double u = static_cast<double>(i - frame(18.5)) / static_cast<double>(frame(1.5));
    s.players[2].armour[i] = static_cast<int>(100 * (1 - u));
    s.players[2].health[i] = static_cast<int>(100 - 99 * u);
  }
  for (size_t i = frame(18.0); i < frame(20.5); i++) s.players[0].weapon[i] = 64;
  die(2, 0, 20.0, -5);
  // 22 s: slot 3 by the shotgun.
  for (size_t i = frame(21.5); i < frame(22.5); i++) s.players[0].weapon[i] = 1;
  die(3, 0, 22.0, -3);
  // 25 s: slot 3 kills the watched player from far off.
  s.rockets.push_back(
      {static_cast<int>(frame(24.0)), static_cast<int>(frame(25.0)), {730, 790, 40}, {50, 50, 30}});
  die(0, 3, 25.0, -45);
  // 28 s: the watched player, back at 27 s, teleports onto slot 1 in the side room.
  for (size_t i = frame(26.0); i < n; i++) s.players[1].origins[i] = {800, 256, 24};
  for (size_t i = frame(28.0); i < n; i++) s.players[0].origins[i] = {800, 256, 24};
  die(1, 0, 28.0, -100);
  return s;
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
