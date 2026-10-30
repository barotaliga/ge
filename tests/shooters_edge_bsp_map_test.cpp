// A MAP FILE UNDER THE MOD SERVER: the 29 fixture map read into a MapSpec (its world's hulls, its
// entity lump) and loaded by the test mod; a player spawned on a deathmatch spot in the hall falls
// onto its floor and stands there; the server's visibility hook answers from the map's PVS and
// hearing from its PHS (the hall hears the vault it cannot see); a 38 map is refused with its
// family named.
//
// ge:summary Test: a fixture map file loaded by the mod server: a player stands, the PVS and PHS
// ge:keywords test shooters-edge bsp map read_bsp_map use_map_sets pvs phs vis load_map fixture
// ge:skill shooter progs brush testing
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "check.hpp"
#include "shooters_edge/server/bsp_map.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;

std::vector<uint8_t> written(br::Family f) {
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK(br::write_bsp(br::sample_fixture(), f, &bytes, &error));
  return bytes;
}

}  // namespace

TEST(a_player_spawned_in_the_fixture_hall_stands_on_its_floor) {
  ge::progs::Progs progs;
  std::string error;
  CHECK_MSG(sv::build_test_mod(progs, error), error);
  sv::QcServerOptions o;
  o.loop.max_edicts = 1024;
  std::unique_ptr<sv::QcServer> s = sv::make_qc_server(progs, o, error);
  CHECK_MSG(s != nullptr, error);
  if (!s) return;

  sv::BspMap map;
  sv::MapSpec spec;
  CHECK_MSG(sv::read_bsp_map(written(br::Family::v29), "halls", &map, &spec, error), error);
  CHECK(spec.models.size() == 1);
  CHECK(spec.entities.find("worldspawn") != std::string::npos);
  spec.entities += "{\n\"classname\" \"info_player_deathmatch\"\n\"origin\" \"256 192 64\"\n}\n";
  CHECK_MSG(sv::load_map(*s, spec, error), error);
  sv::use_map_sets(*s, map);
  CHECK(s->models[1] == "maps/halls.bsp");

  sv::connect(*s, 0, "one", false, true);
  sh::Packet p;
  p.client = 0;
  for (sh::MoveCmd& c : p.cmds) c.msec = 13;
  for (int frame = 0; frame < 60; frame++) {
    p.sequence = frame + 1;
    sv::qc_frame(*s, &p, 1);
  }
  const sh::Vec3f at = s->sv.edicts[1].origin;
  CHECK_MSG(std::fabs(at.z - 24) < 0.1f, "z " + std::to_string(at.z));
  CHECK(at.x > 0 && at.x < 512 && at.y > 0 && at.y < 384);
}

TEST(the_server_s_sets_are_the_map_s_pvs_and_phs) {
  sv::BspMap map;
  sv::MapSpec spec;
  std::string error;
  CHECK_MSG(sv::read_bsp_map(written(br::Family::bsp2), "halls", &map, &spec, error), error);
  CHECK(!map.map.phs.empty());
  ge::progs::Progs progs;
  CHECK_MSG(sv::build_test_mod(progs, error), error);
  std::unique_ptr<sv::QcServer> s = sv::make_qc_server(progs, {}, error);
  CHECK_MSG(s != nullptr, error);
  if (!s) return;
  sv::use_map_sets(*s, map);
  const sh::Vec3f hall{256, 192, 64}, corridor{224, 600, 64}, vault{544, 672, 64};
  CHECK(s->vis(s->vis_ctx, hall, corridor, true));
  CHECK(!s->vis(s->vis_ctx, hall, vault, true));
  CHECK(s->vis(s->vis_ctx, hall, vault, false));
  // Outside the map: a leaf with no cluster sees and hears nothing.
  CHECK(!s->vis(s->vis_ctx, {-100, -100, -100}, hall, true));
}

TEST(a_map_that_sweeps_through_brushes_is_refused) {
  sv::BspMap map;
  sv::MapSpec spec;
  std::string error;
  CHECK(!sv::read_bsp_map(written(br::Family::v38), "halls", &map, &spec, error));
  CHECK(error.find("maps/halls.bsp") != std::string::npos);
  CHECK(error.find("next:") != std::string::npos);
}
