// THE CLIP HULLS AND THE PHS of a read map: the 29 and BSP2 fixtures carry the player's and the
// large box's hulls, each open exactly where its box fits inside the rooms (through a doorway it
// fits, never through a wall; a box as wide as the doorway does not pass); hull 0 is the leaf's
// contents; 38 and 46 keep none; a cluster hears what the clusters it sees can see, so the hall
// hears the vault it cannot see; a clip hull that loops back on itself is refused before anything
// walks it.
//
// ge:summary Test: fixture clip hulls open where the box fits, the PHS, a looping hull refused
// ge:keywords test brush clip-hull hull1 hull2 hull_contents phs build_phs cluster_hears loop
// ge:keywords refuse fixture doorway
// ge:skill brush testing
#include <cstring>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"

namespace {

namespace br = ge::brush;

constexpr int kEmpty = -1, kSolid = -2;

std::vector<uint8_t> written(br::Family f) {
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK(br::write_bsp(br::sample_fixture(), f, &bytes, &error));
  return bytes;
}

br::Map read(br::Family f) {
  br::Map map;
  std::string error;
  const bool ok = br::read_bsp(written(f), &map, &error);
  if (!ok) std::printf("  %s\n", error.c_str());
  CHECK(ok);
  return map;
}

int at(const br::Map& m, int hull, float x, float y, float z) {
  const float p[3] = {x, y, z};
  return br::hull_contents(m, p, 0, hull);
}

}  // namespace

TEST(the_29_and_bsp2_fixtures_carry_both_box_hulls) {
  for (br::Family f : {br::Family::v29, br::Family::bsp2}) {
    const br::Map m = read(f);
    CHECK(!m.clip_nodes.empty());
    CHECK(m.models[0].hulls[0] >= 0 && m.models[0].hulls[1] >= 0);
    CHECK(m.models[0].hulls[0] != m.models[0].hulls[1]);
  }
  for (br::Family f : {br::Family::v38, br::Family::v46}) CHECK(read(f).clip_nodes.empty());
}

TEST(a_box_hull_is_open_where_its_box_fits_in_the_rooms) {
  for (br::Family f : {br::Family::v29, br::Family::bsp2}) {
    const br::Map m = read(f);
    // The player's box (-16, -16, -24)..(16, 16, 32) in the hall, 512 x 384 x 256.
    CHECK(at(m, 1, 256, 192, 100) == kEmpty);
    CHECK(at(m, 1, 256, 192, 24) == kEmpty);   // standing on the floor
    CHECK(at(m, 1, 256, 192, 23) == kSolid);   // its feet under the floor
    CHECK(at(m, 1, 16, 192, 100) == kEmpty);   // its side on the west wall
    CHECK(at(m, 1, 15, 192, 100) == kSolid);   // through it
    CHECK(at(m, 1, 256, 192, 223) == kEmpty);  // its head just under the ceiling
    CHECK(at(m, 1, 256, 192, 224) == kSolid);  // on a plane whose front is solid: solid
    // The east doorway is 64 wide and 128 tall: the player's box stands in it, across both rooms.
    CHECK(at(m, 1, 528, 192, 40) == kEmpty);
    CHECK(at(m, 1, 528, 150, 40) == kSolid);  // beside the doorway: the wall
    CHECK(at(m, 1, 528, 192, 97) == kSolid);  // its head above the lintel
    // The large box (-32, -32, -24)..(32, 32, 64) is the doorway's width: no room to stand in it.
    CHECK(at(m, 2, 528, 192, 30) == kSolid);
    CHECK(at(m, 2, 400, 192, 30) == kEmpty);
    CHECK(at(m, 2, 256, 192, 100) == kEmpty);
    CHECK(at(m, 2, 31, 192, 100) == kSolid);
    // Hull 0 is the point tree: a point is open anywhere inside a room.
    CHECK(at(m, 0, 1, 1, 1) == kEmpty);
    CHECK(at(m, 0, -1, 1, 1) == kSolid);
  }
}

TEST(a_cluster_hears_what_the_clusters_it_sees_can_see) {
  br::Map m = read(br::Family::v29);
  // Rooms in fixture order: hall 0, east door 1, side room 2, north door 3, corridor 4, vault
  // door 5, vault 6. The hall sees two doorways deep, to the corridor, but not the vault.
  CHECK(br::cluster_sees(m, 0, 4));
  CHECK(!br::cluster_sees(m, 0, 6));
  CHECK(br::cluster_hears(m, 0, 6));  // no PHS yet: everything hears everything
  br::build_phs(&m);
  CHECK(m.phs.size() == m.pvs.size());
  CHECK(br::cluster_hears(m, 0, 6));   // the corridor, which the hall sees, sees the vault
  CHECK(!br::cluster_hears(m, 2, 6));  // the side room sees the hall, which does not see it
  for (int32_t a = 0; a < m.clusters; ++a) {
    for (int32_t b = 0; b < m.clusters; ++b) {
      if (br::cluster_sees(m, a, b)) CHECK(br::cluster_hears(m, a, b));
    }
  }
}

TEST(a_clip_hull_that_loops_is_refused) {
  std::vector<uint8_t> b = written(br::Family::v29);
  // Lump 9 (the clip nodes): node 0's front child made node 0.
  uint32_t off = 0;
  std::memcpy(&off, b.data() + 4 + 9 * 8, 4);
  b[off + 4] = 0;
  b[off + 5] = 0;
  br::Map m;
  std::string error;
  CHECK(!br::read_bsp(b, &m, &error));
  CHECK(error.find("loops back") != std::string::npos);
  CHECK(error.find("next:") != std::string::npos);
}
