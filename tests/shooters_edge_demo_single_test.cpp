// client/demo_single.cpp's larger limits: single-player demos of protocols 666 and 999 written by
// hand and read frame by frame. 666: a second baseline form with a word's model and alpha, an
// entity update run on to its third byte (alpha, scale, the model's and frame's high bytes, the
// next step's time), the status's high bytes, a sound by a word, the skybox and fog messages.
// 999: the flags word's coordinates (sixteenths in an i32, or floats) and angles (a short, or a
// float). Protocol 15's older transparency, and a protocol extension's marker refused.
//
// ge:summary Test: single-player demos of protocols 666 and 999 read frame by frame
// ge:keywords test single-player dem protocol-666 protocol-999 larger-limits alpha scale
// ge:keywords wide-coordinates baseline2 clientdata skybox fog transparency
// ge:skill shooter replay
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/client/demo.hpp"

namespace cl = shooters_edge::client;

namespace {

// The stream's encodings, as the demo's server info says them.
struct Wire {
  int protocol = 666;
  uint32_t flags = 0;  // 999's
};

struct Bytes {
  Wire wire;
  std::vector<uint8_t> b;
  void u8(int v) { b.push_back(static_cast<uint8_t>(v)); }
  void u16(int v) {
    u8(v & 0xFF);
    u8((v >> 8) & 0xFF);
  }
  void i32(int32_t v) {
    u16(v & 0xFFFF);
    u16((v >> 16) & 0xFFFF);
  }
  void f32(float v) {
    uint32_t u = 0;
    std::memcpy(&u, &v, 4);
    i32(static_cast<int32_t>(u));
  }
  void text(const std::string& s) {
    b.insert(b.end(), s.begin(), s.end());
    u8(0);
  }
  // A coordinate in units: floats, sixteenths in an i32, or eighths in an i16.
  void coord(float v) {
    if ((wire.flags & 16) != 0)
      f32(v);
    else if ((wire.flags & 128) != 0)
      i32(static_cast<int32_t>(v * 16.0f));
    else
      u16(static_cast<int>(v * 8.0f));
  }
  // An angle in degrees: a float, a short, or a byte.
  void angle(float v) {
    if ((wire.flags & 4) != 0)
      f32(v);
    else if ((wire.flags & 2) != 0)
      u16(static_cast<int>(v * 65536.0f / 360.0f));
    else
      u8(static_cast<int>(v * 256.0f / 360.0f));
  }
};

void block(std::vector<uint8_t>& file, const Bytes& w, float yaw = 0) {
  Bytes head;
  head.i32(static_cast<int32_t>(w.b.size()));
  head.f32(0);
  head.f32(yaw);
  head.f32(0);
  file.insert(file.end(), head.b.begin(), head.b.end());
  file.insert(file.end(), w.b.begin(), w.b.end());
}

// A level of a few frames: model 300 on a baseline of the second form, the player (entity 1) and
// entity 300 moving, the status past a byte.
std::vector<uint8_t> larger_limits_demo(Wire wire) {
  std::vector<uint8_t> file = {'-', '1', '\n'};
  Bytes w{wire, {}};
  w.u8(11);  // the server info
  w.i32(wire.protocol);
  if (wire.protocol == 999) w.i32(static_cast<int32_t>(wire.flags));
  w.u8(1);
  w.u8(0);
  w.text("The Big Hall");
  w.text("maps/bighall.bsp");
  for (int i = 2; i <= 300; i++) w.text("progs/thing" + std::to_string(i) + ".mdl");
  w.text("");
  w.text("misc/hum.wav");
  w.text("");
  w.u8(37);  // the skybox
  w.text("stormy");
  w.u8(41);  // fog: density, colour, the fade
  w.u8(30);
  w.u8(100);
  w.u8(110);
  w.u8(120);
  w.u16(0);
  // Entity 300's baseline, the second form: a word's model, alpha.
  w.u8(42);
  w.u16(300);
  w.u8(1 | 4);
  w.u16(300);
  w.u8(0);  // frame
  w.u8(0);  // colormap
  w.u8(0);  // skin
  for (const float o : {5000.0f, -200.0f, 64.0f}) {
    w.coord(o);
    w.angle(0);
  }
  w.u8(128);  // alpha: (128 - 1) / 254
  // The player's baseline, the first form.
  w.u8(22);
  w.u16(1);
  w.u8(2);
  w.u8(0);
  w.u8(0);
  w.u8(0);
  for (int i = 0; i < 3; i++) {
    w.coord(0);
    w.angle(0);
  }
  w.u8(5);  // the view entity
  w.u16(1);
  block(file, w);
  for (int f = 0; f < 3; f++) {
    w = Bytes{wire, {}};
    w.u8(7);  // the time
    w.f32(static_cast<float>(f) * 0.1f);
    // The status: health, then past a byte the armour and the shells by their high bytes, and
    // the view model by a word's.
    w.u8(15);
    w.u16((1 << 9) | (1 << 13) | (1 << 14) | (1 << 15));
    w.u8((1 << 0) | (1 << 1) | (1 << 3));  // bits 16, 17, 19
    w.i32(0);                              // items
    w.u8(0x2C);                            // armour 300 = 0x12C
    w.u8(0x2D);                            // the view model 301 = 0x12D
    w.u16(90 - f);
    w.u8(5);
    w.u8(0xF4);  // shells 500 = 0x1F4
    w.u8(0);
    w.u8(0);
    w.u8(0);
    w.u8(1);
    w.u8(0x01);  // the view model's high byte
    w.u8(0x01);  // the armour's
    w.u8(0x01);  // the shells'
    // The player moves in x.
    w.u8(0x80 | 2);
    w.u8(1);
    w.coord(4000.0f + static_cast<float>(f) * 10.0f);
    // Entity 300: a long number, its model's high byte, scale, the next step's time and its
    // origin in x past a short's reach (666 still clamps x to an i16 of eighths).
    w.u8(0x80 | 1 | 2);
    w.u8((1 << (14 - 8)) | (1 << (15 - 8)) | (1 << (10 - 8)));
    w.u8((1 << (18 - 16)) | (1 << (19 - 16)) | (1 << (20 - 16)));
    w.u16(300);
    w.u8(0x2C);  // model 300's low byte
    w.coord(wire.protocol == 999 ? 9000.0f + static_cast<float>(f) : 3000.0f);
    w.u8(32);    // scale: twice
    w.u8(0x01);  // the model's high byte
    w.u8(25);    // the next step a tenth of a second on
    // A sound by a word's index on entity 300 by a word.
    w.u8(6);
    w.u8(8 | 16);
    w.u16(300);
    w.u8(3);
    w.u16(1);
    w.coord(100);
    w.coord(200);
    w.coord(300);
    block(file, w, 90.0f);
  }
  Bytes end{wire, {}};
  end.u8(2);
  block(file, end);
  return file;
}

struct Frame {
  cl::DemoState s;
};

std::vector<Frame> read_all(const std::vector<uint8_t>& file, std::string& error,
                            cl::DemoRead& last) {
  cl::Demo d;
  std::vector<Frame> out;
  if (!cl::open_demo(file, d, error)) {
    last = cl::DemoRead::error;
    return out;
  }
  for (;;) {
    last = cl::next_frame(d, error);
    if (last != cl::DemoRead::frame) break;
    out.push_back({d.state});
  }
  return out;
}

}  // namespace

TEST(a_protocol_666_demo_reads_its_wider_fields) {
  std::string error;
  cl::DemoRead last{};
  const std::vector<Frame> frames = read_all(larger_limits_demo({666, 0}), error, last);
  CHECK_MSG(last == cl::DemoRead::end, error);
  CHECK(frames.size() == 4);  // the signon, then three
  const cl::DemoState& s = frames.back().s;
  CHECK(s.protocol == 666);
  CHECK(s.map == "bighall");
  CHECK(s.models.size() == 301 && s.models[300] == "progs/thing300.mdl");
  CHECK(s.entities.size() > 300 && s.entities[300].present);
  const cl::DemoEntity& e = s.entities[300];
  CHECK(e.model == 300);
  CHECK(e.alpha == 127.0f / 254.0f);  // the baseline's, kept by the update
  CHECK(e.scale == 2.0f);
  CHECK(e.origin.x == 3000.0f && e.origin.y == -200.0f && e.origin.z == 64.0f);
  CHECK(s.stats[cl::kStatArmour] == 300);
  CHECK(s.stats[cl::kStatShells] == 500);
  CHECK(s.stats[cl::kStatWeapon] == 301);
  CHECK(s.stats[cl::kStatHealth] == 88);
  CHECK(s.started.size() == 1 && s.started[0].sound == 1 && s.started[0].entity == 300 &&
        s.started[0].channel == 3);
}

TEST(a_protocol_999_demo_reads_its_coordinates_and_angles_as_its_flags_say) {
  // Sixteenths in an i32 and short angles (130), then floats for both (16 and 4).
  for (const uint32_t flags : {130u, 20u}) {
    std::string error;
    cl::DemoRead last{};
    const std::vector<Frame> frames = read_all(larger_limits_demo({999, flags}), error, last);
    CHECK_MSG(last == cl::DemoRead::end, error);
    CHECK(frames.size() == 4);
    const cl::DemoState& s = frames.back().s;
    CHECK(s.protocol == 999);
    const cl::DemoEntity& e = s.entities[300];
    CHECK_MSG(e.origin.x == 9002.0f && e.origin.y == -200.0f, std::to_string(e.origin.x));
    CHECK(e.model == 300 && e.scale == 2.0f);
    CHECK(s.entities[1].origin.x == 4020.0f);
    CHECK(s.started[0].origin.z == 300.0f);
    CHECK(s.angles.y == 90.0f);  // the block's own
  }
}

TEST(protocol_15_s_older_transparency_reads_as_an_alpha) {
  std::vector<uint8_t> file = {'-', '1', '\n'};
  Bytes w{{15, 0}, {}};
  w.u8(11);
  w.i32(15);
  w.u8(1);
  w.u8(0);
  w.text("Hall");
  w.text("maps/hall.bsp");
  w.text("progs/glass.mdl");
  w.text("");
  w.text("");
  // Entity 2 with the older transparency: mode 2, alpha 0.25, a glow.
  w.u8(0x80 | 1);
  w.u8(1 << (15 - 8));
  w.u8(2);
  w.f32(2.0f);
  w.f32(0.25f);
  w.f32(1.0f);
  block(file, w);
  Bytes end{{15, 0}, {}};
  end.u8(2);
  block(file, end);
  std::string error;
  cl::DemoRead last{};
  const std::vector<Frame> frames = read_all(file, error, last);
  CHECK_MSG(last == cl::DemoRead::end, error);
  CHECK(frames.size() == 1 && frames[0].s.entities[2].alpha == 0.25f);
}

TEST(a_protocol_extension_s_marker_is_refused_in_words) {
  std::vector<uint8_t> file = {'-', '1', '\n'};
  Bytes w{{15, 0}, {}};
  w.u8(11);
  w.i32(0x32455446);
  w.i32(0);
  block(file, w);
  std::string error;
  cl::DemoRead last{};
  read_all(file, error, last);
  CHECK(last == cl::DemoRead::error);
  CHECK_MSG(error.find("extension") != std::string::npos, error);
}
