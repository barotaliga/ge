// Tests: the sequel's client recordings on the replay file version 2
// (games/shooters_edge/client/sequel_demo.hpp): a made-up recording packs through the field codec,
// every block walked, smaller than zstd, and comes back byte for byte; a block with a message the
// walk does not know is kept as its bytes and still comes back; the framing check refuses what is
// not a recording; with GE_SEQUEL_PAK naming the demo's pack, its own recording does the same.
//
// ge:summary Tests: the sequel's .dm2 recordings packed into .ger and back byte for byte
// ge:keywords test sequel dm2 replay-v2 pack unpack round-trip field-codec fixture
// ge:skill replay shooter testing
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/sequel_demo.hpp"
#include "vfs/pak.hpp"
#include "vfs/zstd.hpp"

namespace cl = shooters_edge::client;

namespace {

struct Trip {
  bool same = false;
  size_t packed = 0, zstd = 0;
  cl::DemoPackReport rep;
  std::string error;
};

Trip round_trip(const std::vector<uint8_t>& rec) {
  Trip t;
  std::vector<uint8_t> file, back;
  if (!cl::pack_demo(rec, file, t.error, {}, &t.rep)) return t;
  if (!cl::unpack_demo(file, back, t.error)) return t;
  t.same = back == rec;
  t.packed = file.size();
  t.zstd = ge::vfs::zstd_pack(rec).size();
  return t;
}

}  // namespace

TEST(a_made_up_recording_packs_smaller_than_zstd_and_comes_back) {
  const std::vector<uint8_t> rec = cl::sequel_fixture(30, 7);
  CHECK(cl::is_sequel_recording(rec));
  const Trip t = round_trip(rec);
  CHECK_MSG(t.same, t.error);
  CHECK(t.rep.format == "sequel");
  CHECK(t.rep.records == 302 && t.rep.walked == t.rep.records && t.rep.kept == 0);
  CHECK_MSG(t.packed < t.zstd, std::to_string(t.packed) + " vs zstd " + std::to_string(t.zstd));
  // The same bytes twice: nothing but the recording decides them.
  std::vector<uint8_t> a, b;
  std::string error;
  CHECK(cl::pack_demo(rec, a, error) && cl::pack_demo(rec, b, error) && a == b);
}

TEST(a_block_the_walk_does_not_know_is_kept_and_comes_back) {
  std::vector<uint8_t> rec = cl::sequel_fixture(3, 9, 2, 4);
  std::vector<std::span<const uint8_t>> blocks = cl::sequel_blocks(rec);
  CHECK(blocks.size() == 32);
  // An unknown message id (99) at the end of the fifth block, its length grown by one.
  const size_t at = static_cast<size_t>(blocks[4].data() - rec.data());
  const size_t len = blocks[4].size();
  rec.insert(rec.begin() + static_cast<std::ptrdiff_t>(at + len), 99);
  rec[at - 4] = static_cast<uint8_t>((len + 1) & 0xFF);
  rec[at - 3] = static_cast<uint8_t>((len + 1) >> 8);
  CHECK(cl::is_sequel_recording(rec));
  const Trip t = round_trip(rec);
  CHECK_MSG(t.same, t.error);
  CHECK(t.rep.kept == 1 && t.rep.walked == 31);
}

TEST(the_framing_check_refuses_what_is_not_a_recording) {
  std::vector<uint8_t> rec = cl::sequel_fixture(1, 3, 1, 1);
  CHECK(cl::is_sequel_recording(rec));
  std::vector<uint8_t> no_end(rec.begin(), rec.end() - 4);
  CHECK(!cl::is_sequel_recording(no_end));
  std::vector<uint8_t> tail = rec;
  tail.push_back(0);
  CHECK(!cl::is_sequel_recording(tail));
  std::vector<uint8_t> wrong = rec;
  wrong[4] = 13;  // the first block opens with something other than the server data
  CHECK(!cl::is_sequel_recording(wrong));
  // Not a sequel recording: kept as bytes, still back exactly.
  const Trip t = round_trip(no_end);
  CHECK_MSG(t.same, t.error);
  CHECK(t.rep.format == "bytes");
}

TEST(the_demo_packs_own_recording_comes_back_when_its_pack_is_named) {
  const char* path = std::getenv("GE_SEQUEL_PAK");
  if (path == nullptr || *path == '\0') {
    ge::test::skip("GE_SEQUEL_PAK names no pack (the sequel demo's baseq2/pak0.pak)");
    return;
  }
  std::ifstream in(path, std::ios::binary);
  const std::vector<uint8_t> pak((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());
  ge::vfs::Pak p;
  std::string error;
  CHECK_MSG(ge::vfs::read_pak(pak, &p, &error), error);
  const ge::vfs::PakEntry* e = ge::vfs::pak_find(p, "demos/q2demo1.dm2");
  CHECK(e != nullptr);
  if (e == nullptr) return;
  const std::span<const uint8_t> bytes = ge::vfs::pak_file(pak, *e);
  const std::vector<uint8_t> rec(bytes.begin(), bytes.end());
  CHECK(cl::is_sequel_recording(rec));
  const Trip t = round_trip(rec);
  CHECK_MSG(t.same, t.error);
  CHECK(t.rep.walked == t.rep.records);
  CHECK_MSG(t.packed < t.zstd, std::to_string(t.packed) + " vs zstd " + std::to_string(t.zstd));
}
