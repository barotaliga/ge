// Dzip archives (games/shooters_edge/client/demo_dzip.hpp): a demo's rewritten stream written by
// hand (the track line; a server info, a baseline by its step and flags and the time in the first
// block; the time's delta, an entity group moving the entity by a velocity, client data by its
// changes and a sound by its restored channel and origin in the second; the velocity carried on in
// the third) comes back as the recording written by hand beside it, byte for byte and under its
// checksum; a recording kept whole in the stream's raw tail opens and plays through `open_demo`,
// alone and inside a zip; the readme beside a demo is not counted; damage and two demos are
// refused in words.
//
// ge:summary Tests Dzip: the demo stream written back byte for byte, open_demo on .dz, refusals
// ge:keywords test dzip dz demo-archive entity-group client-data sound raw-tail checksum
// ge:skill shooter testing
#include <cstring>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_dzip.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "vfs/zip.hpp"

namespace cl = shooters_edge::client;

namespace {

struct Bytes {
  std::vector<uint8_t> b;
  Bytes& u8(uint32_t v) {
    b.push_back(static_cast<uint8_t>(v));
    return *this;
  }
  Bytes& u16(uint32_t v) { return u8(v).u8(v >> 8); }
  Bytes& u32(uint32_t v) { return u16(v & 0xFFFF).u16(v >> 16); }
  Bytes& text(const std::string& s) {
    b.insert(b.end(), s.begin(), s.end());
    return u8(0);
  }
  Bytes& add(const Bytes& o) {
    b.insert(b.end(), o.b.begin(), o.b.end());
    return *this;
  }
};

uint32_t bits_of(float f) {
  uint32_t u = 0;
  std::memcpy(&u, &f, 4);
  return u;
}

Bytes server_info() {
  Bytes m;
  m.u8(11).u32(15).u8(1).u8(0).text("Hall").text("maps/hall.bsp").text("progs/player.mdl");
  m.text("").text("");
  return m;
}

// The stream and the recording it stands for.
struct Pair {
  std::vector<uint8_t> stream, demo;
};

Pair hand_written() {
  Bytes s, d;
  s.u8('-').u8('1').u8('\n');
  d.u8('-').u8('1').u8('\n');
  const auto block = [&](const Bytes& m, uint32_t yaw) {
    d.u32(uint32_t(m.b.size())).u32(0).u32(yaw).u32(0).add(m);
  };
  const uint32_t yaw = bits_of(90.0f);
  // Block 1: the yaw's delta, the server info, entity 1's baseline (origin only), the time.
  s.u8(0x02).u32(yaw);
  s.add(server_info());
  s.u8(22).u16(1 | (1 << 13)).u8(2).u16(80).u16(160).u16(24);
  s.u8(7).u16(100);
  s.u8(0);
  Bytes m1;
  m1.add(server_info());
  m1.u8(22).u16(1).u8(2).u8(0).u8(0).u8(0).u16(80).u8(0).u16(160).u8(0).u16(24).u8(0);
  m1.u8(7).u32(100);
  block(m1, yaw);
  // Block 2: the time on 50; entity 1 into the frame, its x moving 8 (a velocity); health 100; a
  // sound on channel 1 at (100, 160, 24), 12 past the entity in x.
  s.u8(0).u8(7).u16(50);
  s.u8(0x30).u8(1).u8(0).u8(0x04).u8(8);
  s.u8(0x48).u8(0x10).u16(100);
  s.u8(0x39).u8(255).u16((1 << 3) | 1).u8(1).u16(12).u16(0).u16(0);
  s.u8(0);
  Bytes m2;
  m2.u8(7).u32(150);
  m2.u8(0x82).u8(1).u16(88);
  m2.u8(15).u16(0x0200).u32(0x4001).u16(100).u8(0).u8(0).u8(0).u8(0).u8(0).u8(0);
  m2.u8(6).u8(1).u8(255).u16((1 << 3) | 1).u8(1).u16(100).u16(160).u16(24);
  block(m2, yaw);
  // Block 3: the velocity carries x on to 96 with a record that changes nothing; then the end.
  s.u8(0).u8(0x30).u8(0).u8(0x01).u8(0).u8(2).u8(0);
  Bytes m3;
  m3.u8(0x82).u8(1).u16(96).u8(2);
  block(m3, yaw);
  return {s.b, d.b};
}

// A recording kept whole: the track line, then the rest in the raw tail.
std::vector<uint8_t> raw_tail(const std::vector<uint8_t>& demo) {
  size_t nl = 0;
  while (demo[nl] != '\n') nl++;
  Bytes s;
  s.b.assign(demo.begin(), demo.begin() + static_cast<std::ptrdiff_t>(nl + 1));
  s.u8(0xFF).u32(uint32_t(demo.size() - nl - 1));
  s.b.insert(s.b.end(), demo.begin() + static_cast<std::ptrdiff_t>(nl + 1), demo.end());
  return s.b;
}

std::vector<uint8_t> text(const std::string& t) { return {t.begin(), t.end()}; }

}  // namespace

TEST(a_dzip_demo_stream_comes_back_as_the_recording_byte_for_byte) {
  const Pair p = hand_written();
  const auto readme = text("e1m1 in 0:21, easy skill");
  const auto archive =
      cl::dzip_write({{"RUN.DEM", 5, p.stream, p.demo}, {"run.txt", 0, readme, readme}});
  CHECK(cl::is_dzip(archive));
  std::vector<cl::DzipEntry> list;
  std::string error;
  CHECK_MSG(cl::dzip_list(archive, list, error), error);
  CHECK(list.size() == 2 && list[0].type == 5 && list[0].size == p.demo.size());
  std::vector<uint8_t> out;
  CHECK_MSG(cl::dzip_entry(archive, list[0], out, error), error);
  CHECK(out == p.demo);
  CHECK_MSG(cl::dzip_entry(archive, list[1], out, error), error);
  CHECK(out == readme);
}

TEST(a_dz_file_opens_and_plays_through_open_demo_alone_and_in_a_zip) {
  const auto demo = cl::write_single_demo(cl::loop_script("halls", {128, 192, 24}, 64, 90));
  const auto archive = cl::dzip_write(
      {{"halls.dem", 5, raw_tail(demo), demo}, {"readme.txt", 0, text("notes"), text("notes")}});
  for (const auto& file : {archive, ge::vfs::write_stored_zip({{"halls.dz", archive}})}) {
    cl::Demo d;
    std::string error;
    CHECK_MSG(cl::open_demo(file, d, error), error);
    CHECK(d.dzip && d.inner_name == "halls.dem");
    int frames = 0;
    for (;;) {
      const cl::DemoRead r = cl::next_frame(d, error);
      if (r != cl::DemoRead::frame) {
        CHECK_MSG(r == cl::DemoRead::end, error);
        break;
      }
      frames++;
    }
    CHECK(frames > 10);
    CHECK(d.state.map == "halls");
  }
}

TEST(a_damaged_dz_or_one_of_two_demos_is_refused_in_words) {
  const Pair p = hand_written();
  // A time delta changed in the stream: the recording written back fails its checksum.
  std::vector<uint8_t> stream = p.stream;
  for (size_t at = 3; at + 2 < stream.size(); at++) {
    if (stream[at] == 7 && stream[at + 1] == 100) {
      stream[at + 1] = 101;
      break;
    }
  }
  const auto archive = cl::dzip_write({{"run.dem", 5, stream, p.demo}});
  std::vector<uint8_t> out;
  std::string name, error;
  CHECK(!cl::dzip_unpack(archive, out, name, error));
  CHECK_MSG(error.find("checksum") != std::string::npos, error);
  const auto two = cl::dzip_write({{"a.dem", 5, p.stream, p.demo}, {"b.dem", 5, p.stream, p.demo}});
  CHECK(!cl::dzip_unpack(two, out, name, error));
  CHECK_MSG(error.find("a.dem, b.dem") != std::string::npos, error);
  std::vector<uint8_t> first = {'D', 'Z', 1, 0, 12, 0, 0, 0, 0, 0, 0, 0, 0};
  CHECK(!cl::dzip_unpack(first, out, name, error));
  CHECK_MSG(error.find("first version") != std::string::npos, error);
}
