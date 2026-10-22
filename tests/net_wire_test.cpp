// engine/net/wire.hpp: the writer and reader agree, never pass the buffer's end, and the
// sequence arithmetic wraps.
//
// ge:summary Test: the wire's writer, reader, varints and sequence arithmetic
// ge:keywords test net wire varint overflow bad sequence wrap
// ge:skill net testing
#include "net/wire.hpp"

#include "check.hpp"

using namespace ge::net;

TEST(wire_round_trips_every_width) {
  uint8_t buf[64];
  Writer w = make_writer(buf, sizeof buf);
  write_u8(w, 0xab);
  write_u16(w, 0x1234);
  write_u32(w, 0xdeadbeef);
  write_u64(w, 0x0123456789abcdefull);
  for (const uint64_t v : {0ull, 127ull, 128ull, 16383ull, 16384ull, ~0ull}) write_varint(w, v);
  CHECK(!w.overflow);
  CHECK(buf[1] == 0x34 && buf[2] == 0x12);  // little endian
  Reader r = make_reader(buf, w.size);
  CHECK(read_u8(r) == 0xab);
  CHECK(read_u16(r) == 0x1234);
  CHECK(read_u32(r) == 0xdeadbeef);
  CHECK(read_u64(r) == 0x0123456789abcdefull);
  for (const uint64_t v : {0ull, 127ull, 128ull, 16383ull, 16384ull, ~0ull})
    CHECK(read_varint(r) == v);
  CHECK(!r.bad && remaining(r) == 0);
  CHECK(varint_size(127) == 1 && varint_size(128) == 2 && varint_size(~0ull) == 10);
}

TEST(wire_stops_at_the_end) {
  uint8_t buf[5];
  Writer w = make_writer(buf, sizeof buf);
  write_u32(w, 1);
  write_u16(w, 2);  // one byte short: nothing of it is written
  CHECK(w.overflow && w.size == 4);
  write_u8(w, 3);  // after an overflow nothing more is written
  CHECK(w.size == 4);

  Reader r = make_reader(buf, 4);
  read_u32(r);
  CHECK(read_u16(r) == 0 && r.bad);
  CHECK(read_view(r, 1) == nullptr);

  const uint8_t endless[11] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01};
  Reader v = make_reader(endless, sizeof endless);
  CHECK(read_varint(v) == 0 && v.bad);  // more than 64 bits
}

TEST(wire_pads_to_a_size) {
  uint8_t buf[300];
  Writer w = make_writer(buf, sizeof buf);
  write_u8(w, 9);
  write_pad_to(w, 200);
  CHECK(w.size == 200 && !w.overflow && buf[199] == 0);
}

TEST(sequences_wrap) {
  CHECK(seq_newer(1, 0));
  CHECK(!seq_newer(0, 1));
  CHECK(seq_newer(0, 65535));
  CHECK(seq_newer(5, 65530));
  CHECK(!seq_newer(7, 7));
  CHECK(seq_diff(2, 65534) == 4);
  CHECK(seq_diff(65534, 2) == -4);
  CHECK(seq_diff(100, 100) == 0);
}
