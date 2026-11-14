// engine/save/qoi.hpp and picture.hpp: QOI's bytes for a tiny picture as its specification lays
// them out, every op round-tripped to the pixel, damage refused; the box shrink; and a store
// that shows each slot's picture off the file's front by the index, and out of the whole file
// when the index was rebuilt.
//
// ge:summary Test: QOI each way, the PICT chunk, and a store's pictures read by the index
// ge:keywords test qoi picture pict shrink store index thumbnail
// ge:skill save testing
#include <string>
#include <vector>

#include "check.hpp"
#include "save/disk.hpp"
#include "save/picture.hpp"
#include "save/qoi.hpp"
#include "save/store.hpp"

namespace {

using namespace ge::save;

std::vector<uint8_t> scene(int w, int h) {
  std::vector<uint8_t> px(static_cast<size_t>(w * h) * 4);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      uint8_t* p = &px[static_cast<size_t>(y * w + x) * 4];
      p[0] = static_cast<uint8_t>(x * 3);                       // small steps: diff and luma
      p[1] = static_cast<uint8_t>(y < h / 2 ? 200 : x * 37);    // a flat half: runs
      p[2] = static_cast<uint8_t>((x * y) % 7 == 0 ? 9 : 250);  // two colours: the index
      p[3] = static_cast<uint8_t>(x % 11 == 0 ? 128 : 255);     // alpha changes: rgba
    }
  return px;
}

std::vector<uint8_t> save_with_picture(const std::string& title, int w, int h) {
  Meta m;
  m.game = "demo";
  m.title = title;
  std::vector<uint8_t> out;
  FileWriter f(out, m);
  put_picture(f, w, h, scene(w, h), 64);
  f.put(tag("STAT"), 0, 1, std::vector<uint8_t>(60000, 3));
  CHECK(f.finish());
  return out;
}

}  // namespace

TEST(qoi_lays_out_a_tiny_picture_as_its_specification_does) {
  const std::vector<uint8_t> px{255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255};
  std::vector<uint8_t> q;
  qoi_encode(3, 1, px, q);
  // The header; red is (0, 0, 0, 255) with r one less, wrapped: a diff op; then a run of two;
  // then the end marker.
  const std::vector<uint8_t> want{'q', 'o', 'i',  'f',  0, 0, 0, 3, 0, 0, 0, 1,
                                  4,   0,   0x5a, 0xc1, 0, 0, 0, 0, 0, 0, 0, 1};
  CHECK(q == want);
}

TEST(every_op_round_trips_and_damage_is_refused) {
  const int w = 97, h = 61;
  const std::vector<uint8_t> px = scene(w, h);
  std::vector<uint8_t> q, back;
  qoi_encode(w, h, px, q);
  int rw = 0, rh = 0;
  std::string problem;
  CHECK_MSG(qoi_decode(q, rw, rh, back, problem), problem);
  CHECK(rw == w && rh == h && back == px);
  CHECK(q.size() < px.size());
  std::vector<uint8_t> cut(q.begin(), q.begin() + static_cast<std::ptrdiff_t>(q.size() / 2));
  CHECK(!qoi_decode(cut, rw, rh, back, problem));
  std::vector<uint8_t> bad = q;
  bad[0] = 'x';
  CHECK(!qoi_decode(bad, rw, rh, back, problem) && problem.find("qoif") != std::string::npos);
}

TEST(a_picture_is_shrunk_to_its_side_and_kept_in_its_chunk) {
  std::vector<uint8_t> small;
  int w = 0, h = 0;
  shrink(640, 360, std::vector<uint8_t>(640 * 360 * 4, 77), 320, small, w, h);
  CHECK(w == 320 && h == 180 && small[0] == 77);
  shrink(100, 50, std::vector<uint8_t>(100 * 50 * 4, 9), 320, small, w, h);
  CHECK(w == 100 && h == 50);  // never grown
  const std::vector<uint8_t> file = save_with_picture("one", 200, 100);
  FileReader r;
  CHECK(r.open(file));
  CHECK(r.chunks()[1].tag == kPictTag);  // first after the meta
  std::vector<uint8_t> rgba;
  std::string problem;
  CHECK_MSG(read_picture(r, w, h, rgba, problem), problem);
  CHECK(w == 50 && h == 25 && rgba.size() == 50u * 25 * 4);
}

TEST(a_store_shows_each_slot_s_picture_off_the_file_s_front) {
  SimDisk disk;
  StoreSpec spec;
  spec.game = "demo";
  {
    Store s(disk, spec);
    CHECK(s.open());
    for (int i = 1; i <= 3; i++)
      CHECK(s.write("slot-" + std::to_string(i), save_with_picture("s", 160 + i * 16, 90)));
    CHECK(s.find("slot-2")->picture.width == 64 && s.find("slot-2")->picture.bytes > 0);
  }
  SimDisk again = disk.restarted();
  Store s(again, spec);
  CHECK(s.open());
  const int64_t before = again.bytes_read;
  int w = 0, h = 0;
  std::vector<uint8_t> rgba;
  CHECK_MSG(s.picture("slot-2", w, h, rgba), s.problem());
  CHECK(w == 64 && rgba.size() == static_cast<size_t>(w * h) * 4);
  const int64_t read = again.bytes_read - before;
  CHECK_MSG(read < static_cast<int64_t>(s.find("slot-2")->bytes / 2),
            "the picture cost " + std::to_string(read) + " bytes of a " +
                std::to_string(s.find("slot-2")->bytes) + "-byte file");
  // The index gone: the slots are peeked, and the picture comes out of the whole file.
  SimDisk lost = disk.restarted();
  lost.files.erase(kIndexName);
  Store t(lost, spec);
  CHECK(t.open());
  CHECK_MSG(t.picture("slot-3", w, h, rgba), t.problem());
  CHECK(w == 52);  // 208 wide shrunk by whole boxes of 4
  CHECK(!t.picture("slot-9", w, h, rgba));
}
