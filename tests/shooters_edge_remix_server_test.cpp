// Classic remix as the server's (M18u13c): a match the window hosts tells its remix in the server
// info, and a client reading that info back plays the same remix, whatever its own cr_ settings.
//
// ge:summary Tests the server's remix: the host's server info lines and our client reading them
// ge:keywords test remix server-remix remix_info_lines remix_told serverinfo cr_enabled
// ge:skill shooter testing
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "brush/write.hpp"
#include "check.hpp"
#include "shooter/hull.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/remix.hpp"
#include "shooters_edge/net/qw_game.hpp"
#include "shooters_edge/profiles/remix_arsenal.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "vfs/pak.hpp"

namespace cl = shooters_edge::client;
namespace pf = shooters_edge::profiles;
namespace br = ge::brush;
namespace sh = ge::shooter;

namespace {

std::vector<uint8_t> hall() {
  br::BrushWorld world;
  world.models.resize(1);
  const float lo[3] = {-512, -512, -16}, hi[3] = {512, 512, 0};
  world.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  br::Entity start;
  start.pairs = {{"classname", "info_player_deathmatch"}, {"origin", "0 0 24"}};
  world.entities.push_back(start);
  std::vector<uint8_t> bsp;
  std::string error;
  CHECK_MSG(br::write_brush_world(world, &bsp, &error), error);
  return bsp;
}

// Quake played with Quake II's weapons, figures and sounds, its own movement and monsters.
cl::RemixFiles worn() {
  cl::RemixFiles r;
  r.played = "q1";
  r.state.on = true;
  r.state.other = "q2";
  r.worn = pf::remix_arsenal("q1", "q2");
  r.figure = pf::remix_figure("q1", "q2");
  r.heard = "q2";
  r.monsters = "q1";
  return r;
}

const std::string* info_value(const std::vector<std::pair<std::string, std::string>>& info,
                              const std::string& key) {
  for (const auto& [k, v] : info)
    if (k == key) return &v;
  return nullptr;
}

}  // namespace

// The lines name each part another game plays, and nothing the played game keeps; off, none.
TEST(the_host_tells_the_parts_another_game_plays) {
  const cl::RemixFiles r = worn();
  CHECK(r.worn != nullptr && r.figure != nullptr);
  const std::vector<std::string> lines = cl::remix_info_lines(r);
  const std::vector<std::string> want = {"serverinfo cr_enabled 1", "serverinfo cr_items q2",
                                         "serverinfo cr_models q2", "serverinfo cr_sounds q2"};
  CHECK(lines == want);
  cl::RemixFiles off;
  off.played = "q1";
  CHECK(cl::remix_info_lines(off).empty());
}

// What a client reads back is the remix the host plays; a server that tells none is played as
// itself.
TEST(our_client_reads_the_server_s_remix_back) {
  const std::vector<std::pair<std::string, std::string>> info = {{"hostname", "a match"},
                                                                 {"cr_enabled", "1"},
                                                                 {"cr_items", "q2"},
                                                                 {"cr_sounds", "q2"},
                                                                 {"cr_physics", "q2"}};
  const cl::RemixSettings s = cl::remix_told(info);
  CHECK(s.enabled && s.items == "q2" && s.sounds == "q2" && s.physics == "q2");
  CHECK(s.models.empty() && s.monsters.empty());
  CHECK(!cl::remix_told({}).enabled);
  // Read back and checked as the window checks its own: the same parts worn.
  cl::RemixState state;
  state.on = true;
  state.other = "q2";
  CHECK(cl::worn_arsenal(s, state, "q1") == pf::remix_arsenal("q1", "q2"));
  CHECK(cl::heard_game(s, state, "q1") == "q2");
}

// The window's own match carries the lines into its rules' server info, which the network server
// sends every client that joins.
TEST(the_window_s_match_puts_its_remix_in_the_server_info) {
  cl::RemixFiles remix = worn();
  std::string error;
  std::vector<uint8_t> pak;
  CHECK_MSG(ge::vfs::write_pak({{"maps/hall.bsp", hall()}}, &pak, &error), error);
  CHECK(cl::add_archive(remix.files, "pak0.pak", pak, error));
  cl::SearchPath mine;  // a map the played game lacks: the remix's, on the played game's rules
  std::unique_ptr<cl::LoadedMap> map = cl::load_remixed_map(mine, "hall", &remix, error);
  CHECK_MSG(map != nullptr, error);
  if (!map) return;
  static const ge::progs::Progs progs = [] {
    ge::progs::Progs out;
    std::string why;
    CHECK_MSG(cl::baseline_progs(out, why), why);
    return out;
  }();
  cl::Listen l;
  cl::ListenOptions opt = shooters_edge::net::listen_options();
  opt.remix_info = cl::remix_info_lines(remix);
  CHECK_MSG(cl::listen_start(l, progs, std::move(map), opt, error), error);
  CHECK(l.rules != nullptr);
  if (l.rules == nullptr) return;
  const shooters_edge::server::QcServer* qc = shooters_edge::server::qc_of(*l.rules);
  CHECK(qc != nullptr);
  if (qc == nullptr) return;
  const std::string* on = info_value(qc->serverinfo, "cr_enabled");
  const std::string* items = info_value(qc->serverinfo, "cr_items");
  CHECK(on != nullptr && *on == "1");
  CHECK(items != nullptr && *items == "q2");
  CHECK(cl::remix_told(qc->serverinfo).sounds == "q2");
}
