// engine/hub/watch.hpp: the Watch page (the viewer pointed at the TV node's WebSocket, the match's
// scoreboard, the other broadcasts, the reason when there is no picture), the Demo page (the
// viewer on the demo's file by hash, the packs beside it handed over), the viewer's files and
// packs served and refused, and the TV node's record naming its web port.
//
// ge:summary Test: the hub's Watch, Demo and Play pages, the viewer's files, the TV node's web port
// ge:keywords test hub watch demo web-viewer viewer-files websocket tv web-port play map-content
// ge:skill net testing
#include "hub/watch.hpp"

#include <filesystem>
#include <fstream>
#include <string>

#include "check.hpp"
#include "hub/archive.hpp"
#include "hub/content.hpp"
#include "replay/stream.hpp"
#include "tv/net_link.hpp"
#include "tv/websocket.hpp"
#include "vfs/pak.hpp"

using namespace ge::net;
using namespace ge::hub;
namespace gr = ge::replay;
namespace fs = std::filesystem;

namespace {

bool has(const std::string& s, const std::string& part) {
  return s.find(part) != std::string::npos;
}

HubSpec spec() {
  HubSpec s;
  s.game = "Test Game";
  s.id = "test_game";
  return s;
}

HubNode game_node(uint32_t at, const std::string& name, const Address& tv, uint32_t stream) {
  HubNode n;
  n.address = sim_address(at, 27500);
  n.listed = n.have = true;
  n.status.name = name;
  n.status.mode = "4on4";
  n.status.map = "tower";
  n.status.phase = StatusPhase::playing;
  n.status.max_players = 8;
  for (int i = 0; i < 4; i++) {
    StatusPlayer p;
    p.name = "p<" + std::to_string(i) + ">";
    n.status.players.push_back(p);
  }
  StatusStream st;
  st.node = tv;
  st.stream = stream;
  n.status.streams.push_back(st);
  return n;
}

Poller sample(bool web) {
  Poller p;
  HubNode tv;
  tv.address = ipv4(10, 0, 0, 5, 28000);
  tv.kind = NodeKind::tv;
  tv.listed = tv.have = true;
  tv.status.kind = NodeKind::tv;
  tv.status.keys = {{"viewers", "7"}, {"delay_s", "30"}};
  if (web) tv.status.keys.emplace_back("web", "28080");
  p.nodes.push_back(tv);
  p.nodes.push_back(game_node(1, "Final <A>", tv.address, 2));
  p.nodes.push_back(game_node(2, "Other Match", tv.address, 3));
  return p;
}

fs::path viewer_dir(bool with_viewer) {
  const fs::path d = fs::temp_directory_path() / "ge-hub-watch-viewer";
  fs::remove_all(d);
  fs::create_directories(d);
  if (with_viewer) {
    std::ofstream(d / "test_game_viewer.js") << "// the loader\n";
    std::ofstream(d / "test_game_viewer.wasm", std::ios::binary) << std::string("\0asm", 4);
  }
  return d;
}

std::vector<uint8_t> demo_file() {
  gr::StreamHeader h;
  h.game = "test_game";
  h.meta = {{"map", "tower"}, {"mode", "duel"}, {"title", "Final"}, {"player", "ann\tred"}};
  h.tracks = {{1, gr::TrackKind::meta, gr::TrackCodec::stored, "meta"}};
  std::vector<uint8_t> out;
  gr::StreamWriter w;
  gr::stream_begin(w, h, out);
  const uint8_t c[] = {1};
  gr::stream_add(w, 1, 0, c);
  gr::stream_end_block(w, true);
  gr::stream_finish(w, {{"duration_ms", "2000"}});
  return out;
}

}  // namespace

TEST(the_watch_page_points_the_viewer_at_the_nodes_websocket) {
  const Poller p = sample(true);
  Site site;
  site.viewer_dir = viewer_dir(true).string();
  CHECK(viewer_built(site, spec()));
  Answer a;
  CHECK(watch_answer(p, spec(), "/watch/10.0.0.5:28000/2", 0, site, a));
  CHECK(a.code == 200 && a.type.find("text/html") == 0);
  CHECK(has(a.body, "<canvas id=\"view\""));
  CHECK(has(a.body, "\"--tv=ws://10.0.0.5:28080/\",\"--stream=2\""));
  // The page fetches the wasm itself (the bar follows it, 4 bytes in all), then starts the script.
  CHECK(has(a.body, "fetch(\"/viewer/test_game_viewer.wasm\")") && has(a.body, "total=4,"));
  CHECK(has(a.body, "s.src=\"/viewer/test_game_viewer.js\"") && has(a.body, "Module.wasmBinary"));
  CHECK(has(a.body, "<progress id=\"viewer-bar\""));
  CHECK(has(a.body, "Final &lt;A&gt;") && has(a.body, "p&lt;0&gt;"));  // the match's scoreboard
  CHECK(has(a.body, "/watch/10.0.0.5:28000/3\">Other Match"));         // also live
  CHECK(has(a.body, "test_game://tv/10.0.0.5:28000/2\">Open in the game"));
  CHECK(has(a.body, "7 watching · 30 s behind"));
  CHECK(has(a.body, "id=\"viewer-status\""));
  CHECK(!has(a.body, "--pack="));  // no pack beside the viewer: none handed to it
  fs::remove_all(site.viewer_dir);
}

TEST(the_watch_page_says_why_there_is_no_picture) {
  Site none;
  Answer a;
  CHECK(watch_answer(sample(true), spec(), "/watch/10.0.0.5:28000/2", 0, none, a));
  CHECK(a.code == 200 && !has(a.body, "<canvas") && !has(a.body, "<script"));
  CHECK(has(a.body, "does not serve the game's viewer"));
  Site site;
  site.viewer_dir = viewer_dir(true).string();
  CHECK(watch_answer(sample(false), spec(), "/watch/10.0.0.5:28000/2", 0, site, a));
  CHECK(!has(a.body, "<canvas") && has(a.body, "takes no viewers in a browser"));
  CHECK(watch_answer(sample(true), spec(), "/watch/10.0.0.9:28000/2", 0, site, a));
  CHECK(!has(a.body, "<canvas") && has(a.body, "has not heard from this broadcast"));
  CHECK(watch_answer(sample(true), spec(), "/watch/nonsense", 0, site, a) && a.code == 404);
  CHECK(watch_answer(sample(true), spec(), "/watch/10.0.0.5:28000/x", 0, site, a) && a.code == 404);
  CHECK(!watch_answer(sample(true), spec(), "/demos", 0, site, a));
  fs::remove_all(site.viewer_dir);
}

TEST(the_demo_page_plays_the_file_by_hash) {
  const fs::path dir = fs::temp_directory_path() / "ge-hub-watch-archive";
  fs::remove_all(dir);
  Archive archive;
  std::string error;
  CHECK(archive_open(archive, dir.string(), error));
  DemoRow row;
  CHECK(archive_add(archive, demo_file(), 1, {}, error, &row) == AddResult::added);
  Site site;
  site.archive = &archive;
  site.viewer_dir = viewer_dir(true).string();
  Answer a;
  std::ofstream(fs::path(site.viewer_dir) / "pak1.pak") << "PACK";
  std::ofstream(fs::path(site.viewer_dir) / "pak0.pak") << "PACK";
  CHECK(watch_answer(Poller{}, spec(), "/demo/" + row.sha, 0, site, a) && a.code == 200);
  CHECK(has(a.body, "\"--demo=/" + archive_path(row.sha) +
                        "\",\"--pack=/viewer/pak0.pak\",\"--pack=/viewer/pak1.pak\""));
  CHECK(has(a.body, "test_game://demo/" + row.sha) && has(a.body, "Download"));
  CHECK(has(a.body, "Final") && has(a.body, "ann"));
  CHECK(!has(a.body, "/radar/"));  // no radar track, no Map link
  CHECK(watch_answer(Poller{}, spec(), "/demo/" + std::string(64, 'f'), 0, site, a));
  CHECK(a.code == 404 && has(a.body, "no demo with that hash"));
  fs::remove_all(site.viewer_dir);
  fs::remove_all(dir);
}

TEST(the_viewer_files_are_served_and_names_refused) {
  Site site;
  site.viewer_dir = viewer_dir(true).string();
  Answer a;
  CHECK(watch_answer(Poller{}, spec(), "/viewer/test_game_viewer.wasm", 0, site, a));
  CHECK(a.code == 200 && a.type == "application/wasm" && a.body.size() == 4 && a.encoding.empty());
  // The build's gzip copy goes to a reader that takes gzip, the file itself to one that does not.
  std::ofstream(fs::path(site.viewer_dir) / "test_game_viewer.wasm.gz") << "GZIPPED";
  site.gzip = true;
  CHECK(watch_answer(Poller{}, spec(), "/viewer/test_game_viewer.wasm", 0, site, a));
  CHECK(a.code == 200 && a.type == "application/wasm" && a.body == "GZIPPED" &&
        a.encoding == "gzip");
  CHECK(watch_answer(Poller{}, spec(), "/viewer/test_game_viewer.js", 0, site, a));
  CHECK(a.code == 200 && a.encoding.empty());  // no copy of this one: the file
  site.gzip = false;
  CHECK(watch_answer(Poller{}, spec(), "/viewer/test_game_viewer.js", 0, site, a));
  CHECK(a.code == 200 && a.type == "text/javascript" && a.max_age_s == kViewerMaxAgeS);
  std::ofstream(fs::path(site.viewer_dir) / "pak0.pak") << "PACK";
  CHECK(watch_answer(Poller{}, spec(), "/viewer/pak0.pak", 0, site, a));
  CHECK(a.code == 200 && a.type == "application/octet-stream" && a.body == "PACK");
  for (const char* bad :
       {"/viewer/../secret.js", "/viewer/a/b.js", "/viewer/x.html", "/viewer/missing.js"}) {
    CHECK(watch_answer(Poller{}, spec(), bad, 0, site, a) && a.code == 404);
  }
  Site none;
  CHECK(watch_answer(Poller{}, spec(), "/viewer/test_game_viewer.js", 0, none, a));
  CHECK(a.code == 404 && has(a.body, "--viewer=<dir>"));
  fs::remove_all(site.viewer_dir);
}

TEST(a_tv_nodes_record_names_its_web_port) {
  ge::tv::TvHost h;
  bool named = false;
  for (const auto& kv : ge::tv::tv_host_status(h).keys) named = named || kv.first == "web";
  CHECK(!named);  // no WebSocket listener: no port
  ge::tv::TvWeb web;
  std::string error;
  if (!ge::tv::tv_web_listen(web, loopback_v4(0), error)) {
    std::printf("SKIP: no loopback TCP here: %s\n", error.c_str());
    return;
  }
  h.web = &web;
  std::string port;
  for (const auto& [k, v] : ge::tv::tv_host_status(h).keys)
    if (k == "web") port = v;
  CHECK(port == std::to_string(web.listener.local.port) && port != "0");
  ge::tv::tv_web_close(web);
}

TEST(the_play_pages_list_the_maps_and_start_the_player_on_one) {
  const fs::path dir = fs::temp_directory_path() / "ge-hub-watch-play";
  fs::remove_all(dir);
  Archive archive;
  std::string error;
  CHECK(archive_open(archive, (dir / "archive").string(), error));
  Site site;
  site.archive = &archive;
  site.viewer_dir = (dir / "viewer").string();
  fs::create_directories(site.viewer_dir);
  std::ofstream(fs::path(site.viewer_dir) / "test_game_play.js") << "// the player\n";
  std::vector<uint8_t> pak;
  const std::vector<uint8_t> map = {'B', 'S', 'P'}, pic = {1, 2, 3};
  CHECK(ge::vfs::write_pak(
      {{"maps/tower.bsp", map}, {"maps/Yard.bsp", map}, {"gfx/a.lmp", pic}, {"sound/b.wav", pic}},
      &pak, &error));
  std::ofstream(fs::path(site.viewer_dir) / "pak0.pak", std::ios::binary)
      .write(reinterpret_cast<const char*>(pak.data()), static_cast<std::streamsize>(pak.size()));
  HubSpec s = spec();
  Answer a;
  CHECK(!watch_answer(Poller{}, s, "/play", 0, site, a));  // a game that names no map's files
  s.map_files = [](std::string_view m, const ge::vfs::SearchPath&, std::vector<std::string>& names,
                   std::string&) {
    names = {"maps/" + std::string(m) + ".bsp", "gfx/"};
    return true;
  };
  CHECK(viewer_maps(site) == std::vector<std::string>({"tower", "yard"}));
  CHECK(watch_answer(Poller{}, s, "/play", 0, site, a) && a.code == 200);
  CHECK(has(a.body, "href=\"/play/tower\"") && has(a.body, "href=\"/play/yard\""));
  CHECK(has(a.body, ">Play</a>"));  // the nav's Play
  CHECK(watch_answer(Poller{}, s, "/play/tower", 0, site, a) && a.code == 200);
  ContentPack pack;
  CHECK_MSG(map_content(site, s, "tower", pack, error), error);
  CHECK(pack.files == 2 && pack.missing == 0);  // the map and the folder's one picture
  CHECK(has(a.body, "\"--play=tower\",\"--bots=3\",\"--pack=/" + content_path(pack.sha) + "\""));
  CHECK(has(a.body, "/viewer/test_game_play.js") && has(a.body, "/play/yard"));
  CHECK(watch_answer(Poller{}, s, "/play/nowhere", 0, site, a) && a.code == 404);
  CHECK(watch_answer(Poller{}, s, "/play/..%2fx", 0, site, a) && a.code == 404);
  CHECK(!map_content(site, s, "Tower", pack, error) && has(error, "not a map's name"));
  fs::remove_all(dir);
}
