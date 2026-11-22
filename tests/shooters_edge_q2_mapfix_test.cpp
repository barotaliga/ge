// shooters_edge/server/q2_mapfix: the visibility patch by map checksum (rows only gain bits, both
// sets, a map it does not know untouched) and the entity file's name by the CRC-16 of the entities;
// a server of Quake II plays a map with the entities of its file under sv_entpath; the dedicated
// server's terminal runs status, listmaps, gamemode and say on it; a movie between maps is a level
// of its own until the client's `nextserver`, or skipped with sv_cinematics 0.
//
// ge:summary Test: Quake II's maps mended at load: the visibility patch, the entity file's name
// ge:keywords test q2 vis-patch q2_vis_patch q2_entity_file_name listmaps gamemode console-say
// ge:keywords nextserver sv_cinematics cinematic-level
// ge:skill shooter testing
#include <cstring>
#include <filesystem>
#include <fstream>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classic/protocol/wire.hpp"
#include "net/sim_network.hpp"
#include "server/server.hpp"
#include "shooter/brush_contents.hpp"
#include "shooters_edge/net/q2_client.hpp"
#include "shooters_edge/net/q2_game.hpp"
#include "shooters_edge/net/q2_net.hpp"
#include "shooters_edge/server/q2_mapfix.hpp"

namespace sv = shooters_edge::server;

namespace {

ge::brush::Map blank(int clusters) {
  ge::brush::Map m;
  m.clusters = clusters;
  m.row_bytes = static_cast<uint32_t>((clusters + 7) / 8);
  m.pvs.assign(size_t(clusters) * m.row_bytes, 0);
  m.phs = m.pvs;
  return m;
}

bool sees(const ge::brush::Map& m, const std::vector<uint8_t>& rows, int from, int to) {
  return (rows[size_t(from) * m.row_bytes + size_t(to) / 8] >> (to % 8)) & 1;
}

int count(const std::vector<uint8_t>& rows) {
  int n = 0;
  for (uint8_t b : rows)
    for (; b != 0; b &= uint8_t(b - 1)) n++;
  return n;
}

}  // namespace

TEST(four_stock_maps_known_by_checksum_gain_the_clusters_their_spots_see) {
  ge::brush::Map dm1 = blank(400);
  CHECK(sv::q2_vis_patch(dm1, 80717714));
  CHECK(sees(dm1, dm1.pvs, 395, 176) && sees(dm1, dm1.pvs, 395, 183));
  CHECK(sees(dm1, dm1.phs, 395, 176) && !sees(dm1, dm1.pvs, 176, 395));
  CHECK(count(dm1.pvs) == 2 && count(dm1.phs) == 2);
  ge::brush::Map dm8 = blank(1000);
  CHECK(sv::q2_vis_patch(dm8, 742046128) && count(dm8.pvs) == 44 && sees(dm8, dm8.pvs, 639, 947));
  ge::brush::Map dm3 = blank(700);
  CHECK(sv::q2_vis_patch(dm3, 509300933) && count(dm3.pvs) == 6 && sees(dm3, dm3.pvs, 384, 692));
  ge::brush::Map gu = blank(300);
  CHECK(sv::q2_vis_patch(gu, 515801089) && count(gu.pvs) == 900 && sees(gu, gu.pvs, 0, 217));
  ge::brush::Map other = blank(400);
  CHECK(!sv::q2_vis_patch(other, 80717715) && count(other.pvs) == 0);
  ge::brush::Map small = blank(100);  // a map with fewer clusters than the patch names
  CHECK(sv::q2_vis_patch(small, 80717714) && count(small.pvs) == 0);
}

TEST(the_entity_file_is_named_by_the_crc_16_of_the_map_entities) {
  const char* nine = "123456789";
  CHECK(ge::classic::protocol::crc16(reinterpret_cast<const uint8_t*>(nine), 9) == 0x29B1);
  CHECK(sv::q2_entity_file_name("q2dm1", std::string("123456789") + '\0') == "q2dm1@29b1.ent");
  CHECK(sv::q2_entity_file_name("q2dm1", "") == "q2dm1@0000.ent");
}

namespace {

namespace br = ge::brush;
namespace fs = std::filesystem;
namespace cn = shooters_edge::net;

void write(const fs::path& p, const std::string& text) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary) << text;
}

// A hall's map at `root`/baseq2/maps/hall.bsp: a floor, four walls, two spawn spots.
std::string write_hall(const fs::path& root) {
  br::BrushWorld w;
  w.models.resize(1);
  const auto box = [](float x0, float y0, float z0, float x1, float y1, float z1) {
    const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
    return br::box_brush(lo, hi, ge::shooter::kSqSolid, "world/wall");
  };
  w.models[0] = {box(-1024, -1024, -16, 1024, 1024, 0), box(-1040, -1024, -16, -1024, 1024, 256),
                 box(1024, -1024, -16, 1040, 1024, 256), box(-1040, 1024, -16, 1040, 1040, 256),
                 box(-1040, -1040, -16, 1040, -1024, 256)};
  for (const char* at : {"-400 -400 24", "400 400 24"}) {
    br::Entity e;
    e.pairs = {{"classname", "info_player_deathmatch"}, {"origin", at}};
    w.entities.push_back(e);
  }
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  fs::create_directories(root / "baseq2" / "maps");
  std::ofstream(root / "baseq2" / "maps" / "hall.bsp", std::ios::binary)
      .write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
  br::Map m;
  CHECK_MSG(br::read_bsp(bytes, &m, &error), error);
  return m.entity_text;
}

// The view of one player seated in the hall served with `sv_entpath ents`, after a second.
std::vector<uint8_t> served(const fs::path& root, std::string* printed = nullptr) {
  ge::server::ServerConfig c;
  c.root = root.string();
  c.map = "hall";
  c.seed = 3;
  c.rules["sv_entpath"] = "ents";
  ge::net::SimNetwork net = ge::net::make_sim_network(3, ge::net::LinkConditions{});
  ge::net::Transport port;
  ge::net::open_sim_transport(port, net, ge::net::sim_address(0, cn::kPortQ2));
  ge::server::Server s;
  std::string error;
  const ge::server::ServerGame& g = cn::q2_game();
  std::vector<uint8_t> view;
  CHECK_MSG(ge::server::server_open(s, g, c, &port, nullptr, error), error);
  if (s.state == nullptr) return view;
  ge::net::Transport near;
  ge::net::open_sim_transport(near, net, ge::net::sim_address(1, 27901));
  cn::Q2ClientEnd client;
  cn::Q2ClientConfig cc;
  cc.name = "walker";
  cn::q2_client_start(client, near, ge::net::sim_address(0, cn::kPortQ2), cc,
                      static_cast<double>(s.now_us) / 1e6);
  for (int f = 0; f < 100; f++) {
    ge::net::sim_advance(net, s.now_us);
    cn::q2_client_tick(client, s.now_us);
    ge::server::server_frame(s);
  }
  CHECK_MSG(client.stage == cn::ClientStage::active, client.refused);
  g.player_view(s.state, 0, view);
  std::vector<std::string> lines;  // what the dedicated server prints and logs
  g.console_lines(s.state, lines);
  for (const std::string& l : lines)
    if (printed != nullptr) *printed += l + "\n";
  return view;
}

}  // namespace

TEST(sv_entpath_plays_a_map_with_the_entities_of_its_file) {
  const fs::path root = fs::temp_directory_path() / "ge_q2_mapfix";
  fs::remove_all(root);
  const std::string own = write_hall(root);
  const std::vector<uint8_t> as_made = served(root);
  // The file's own spawn spot, far from the map's two: the player starts there.
  const std::string first = own.substr(0, own.find('}') + 2);
  write(root / "baseq2" / "ents" / "hall.ent",
        first + "{\n\"classname\" \"info_player_deathmatch\"\n\"origin\" \"0 700 24\"\n}\n");
  std::string printed;
  const std::vector<uint8_t> moved = served(root, &printed);
  CHECK(!as_made.empty() && moved != as_made);
  CHECK_MSG(printed.find("Loaded entity string from ents/hall.ent\n") != std::string::npos,
            printed);
  // The file named for this build of the map wins over the plain one.
  write(root / "baseq2" / "ents" / sv::q2_entity_file_name("hall", own), own);
  CHECK(served(root) == as_made);
  // A file under 2 bytes is refused: the map's own entities.
  write(root / "baseq2" / "ents" / sv::q2_entity_file_name("hall", own), "x");
  write(root / "baseq2" / "ents" / "hall.ent", "x");
  printed.clear();
  CHECK(served(root, &printed) == as_made);
  CHECK_MSG(printed.find("File too small") != std::string::npos, printed);
  fs::remove_all(root);
}

// The dedicated server's own console reaches a Quake II server: the endpoint's words (`status`),
// `listmaps`, `gamemode` for the next map, and `say` as "console:" to every player.
TEST(the_terminal_runs_status_listmaps_gamemode_and_say_on_a_quake_ii_server) {
  const fs::path root = fs::temp_directory_path() / "ge_q2_console";
  fs::remove_all(root);
  write_hall(root);
  ge::server::ServerConfig c;
  c.root = root.string();
  c.map = "hall";
  c.seed = 3;
  ge::net::SimNetwork net = ge::net::make_sim_network(3, ge::net::LinkConditions{});
  ge::net::Transport port;
  ge::net::open_sim_transport(port, net, ge::net::sim_address(0, cn::kPortQ2));
  ge::server::Server s;
  std::string error;
  const ge::server::ServerGame& g = cn::q2_game();
  CHECK_MSG(ge::server::server_open(s, g, c, &port, nullptr, error), error);
  if (s.state == nullptr) return;
  ge::net::Transport near;
  ge::net::open_sim_transport(near, net, ge::net::sim_address(1, 27901));
  cn::Q2ClientEnd client;
  cn::Q2ClientConfig cc;
  cc.name = "walker";
  cn::q2_client_start(client, near, ge::net::sim_address(0, cn::kPortQ2), cc,
                      static_cast<double>(s.now_us) / 1e6);
  const auto frames = [&](int n) {
    for (int f = 0; f < n; f++) {
      ge::net::sim_advance(net, s.now_us);
      cn::q2_client_tick(client, s.now_us);
      ge::server::server_frame(s);
    }
  };
  frames(100);
  CHECK_MSG(client.stage == cn::ClientStage::active, client.refused);
  const auto printed = [&](const std::string& line) {
    std::vector<std::string> lines;
    g.console_lines(s.state, lines);  // what came before
    CHECK_MSG(g.console(s.state, line), line);
    lines.clear();
    g.console_lines(s.state, lines);
    std::string all;
    for (const std::string& l : lines) all += l + "\n";
    return all;
  };
  const std::string status = printed("status");
  CHECK_MSG(status.find("walker") != std::string::npos, status);
  const std::string maps = printed("listmaps");
  CHECK_MSG(maps.find("hall") != std::string::npos, maps);
  CHECK_MSG(printed("gamemode coop").find("next map is played in coop") != std::string::npos,
            "gamemode coop");
  CHECK_MSG(printed("gamemode ?").find("gamemode: dm, next map coop") != std::string::npos,
            "gamemode ?");
  CHECK_MSG(printed("say the match starts soon").find("console: the match starts soon") !=
                std::string::npos,
            "say");
  frames(20);
  bool heard = false;
  for (const std::string& p : client.prints)
    heard = heard || p.find("console: the match starts soon") != std::string::npos;
  CHECK(heard);
  ge::server::server_close(s);
  fs::remove_all(root);
}

// A MOVIE BETWEEN MAPS: `gamemap "intro.cin+hall"` puts the server on the movie: the client is
// told the level changed and gets server data with no player slot, the movie as the level, and
// nothing more; a `nextserver` of another level is ignored, the right one runs the setting
// (`gamemap "!hall"`, emptied) and the map follows; with `sv_cinematics 0` the movie is skipped.
TEST(a_movie_between_maps_is_a_level_of_its_own_until_nextserver) {
  const fs::path root = fs::temp_directory_path() / "ge_q2_cinematic";
  fs::remove_all(root);
  write_hall(root);
  for (const char* cinematics : {"1", "0"}) {
    ge::server::ServerConfig c;
    c.root = root.string();
    c.map = "hall";
    c.seed = 3;
    c.rules["sv_cinematics"] = cinematics;
    ge::net::SimNetwork net = ge::net::make_sim_network(3, ge::net::LinkConditions{});
    ge::net::Transport port;
    ge::net::open_sim_transport(port, net, ge::net::sim_address(0, cn::kPortQ2));
    ge::server::Server s;
    std::string error;
    const ge::server::ServerGame& g = cn::q2_game();
    CHECK_MSG(ge::server::server_open(s, g, c, &port, nullptr, error), error);
    if (s.state == nullptr) return;
    ge::net::Transport near;
    ge::net::open_sim_transport(near, net, ge::net::sim_address(1, 27901));
    cn::Q2ClientEnd client;
    cn::Q2ClientConfig cc;
    cc.name = "watcher";
    cn::q2_client_start(client, near, ge::net::sim_address(0, cn::kPortQ2), cc,
                        static_cast<double>(s.now_us) / 1e6);
    const auto frames = [&](int n) {
      for (int f = 0; f < n; f++) {
        ge::net::sim_advance(net, s.now_us);
        cn::q2_client_tick(client, s.now_us);
        ge::server::server_frame(s);
      }
    };
    frames(100);
    CHECK_MSG(client.stage == cn::ClientStage::active, client.refused);
    const int32_t first = cn::q2_demo(client).server_count;
    std::vector<std::string> lines;
    CHECK(g.console(s.state, "gamemap \"intro.cin+hall\""));
    frames(100);
    const int32_t count = cn::q2_demo(client).server_count;
    if (std::string(cinematics) == "0") {  // skipped: the map at once, the client back in it
      CHECK(count != first);
      CHECK_MSG(client.stage == cn::ClientStage::active, client.refused);
      lines.clear();
      g.console_lines(s.state, lines);
      bool movie = false;
      for (const std::string& l : lines) movie = movie || l == "SpawnServer: intro.cin";
      CHECK(!movie);
    } else {
      CHECK(count != first);
      CHECK(cn::q2_demo(client).state.player == 0);  // no slot (read as -1, kept at 0)
      g.console_lines(s.state, lines);
      bool movie = false;
      for (const std::string& l : lines) movie = movie || l == "SpawnServer: intro.cin";
      CHECK(movie);
      CHECK(g.console(s.state, "nextserver"));  // the setting as it stands
      cn::q2_client_command(client, "nextserver " + std::to_string(count + 1));  // not this one
      frames(30);
      CHECK(cn::q2_demo(client).server_count == count);
      cn::q2_client_command(client, "nextserver " + std::to_string(count));
      frames(150);
      CHECK(cn::q2_demo(client).server_count != count);
      CHECK_MSG(client.stage == cn::ClientStage::active, client.refused);
      CHECK_MSG(cn::q2_demo(client).state.map == "hall", cn::q2_demo(client).state.map);
    }
    ge::server::server_close(s);
  }
  fs::remove_all(root);
}
