// engine/hub/link.hpp: the hub's links. Each kind is read and written back; a link that would
// carry anything but a host, a port, a stream number or a hash onto a command line is refused;
// the registration plans name the right files and commands on each system; a demo is fetched
// from a real hub over loopback and kept only when its bytes match its name.
//
// ge:summary Test: the hub's links (read, written, refused, the plans, a demo fetched over HTTP)
// ge:keywords test hub link parse refuse injection register plan fetch demo loopback
// ge:skill net testing
#include "hub/link.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include "check.hpp"
#include "hub/archive.hpp"
#include "hub/http.hpp"
#include "mod/sha256.hpp"
#include "net/address.hpp"
#include "replay/stream.hpp"

using namespace ge::hub;

namespace {

const std::string kSha(64, 'a');

bool has(const std::string& s, const std::string& part) {
  return s.find(part) != std::string::npos;
}

}  // namespace

TEST(each_kind_reads_and_writes_back) {
  HubLink l;
  std::string err;
  CHECK(parse_link("se://join/10.0.0.5:27500", "se", l, err));
  CHECK(l.kind == LinkKind::join && l.address == "10.0.0.5:27500");
  CHECK(link_url("se", l) == "se://join/10.0.0.5:27500");
  CHECK(parse_link("se://spectate/arena.example.org/", "se", l, err));  // a browser's slash
  CHECK(l.kind == LinkKind::spectate && l.address == "arena.example.org");
  CHECK(parse_link("se://tv/tv.example.org:28000/3", "se", l, err));
  CHECK(l.kind == LinkKind::tv && l.address == "tv.example.org:28000" && l.stream == 3);
  CHECK(link_url("se", l) == "se://tv/tv.example.org:28000/3");
  CHECK(parse_link("se://demo/" + kSha + "?hub=hub.example:28080", "se", l, err));
  CHECK(l.kind == LinkKind::demo && l.sha == kSha && l.hub == "hub.example:28080");
  CHECK(link_url("se", l) == "se://demo/" + kSha + "?hub=hub.example:28080");
}

TEST(anything_else_is_refused) {
  HubLink l;
  std::string err;
  const char* bad[] = {
      "other://join/10.0.0.5",          // another game's scheme
      "se://join/",                     // no server
      "se://join/10.0.0.5;quit",        // a command after the address
      "se://join/10.0.0.5 +exec evil",  // words after it
      "se://join/-config",              // a flag
      "se://join/10.0.0.5:99999",       // no such port
      "se://join/10.0.0.5:0",
      "se://spectate/host\"name",
      "se://tv/tv.example.org:28000/x",
      "se://tv/tv.example.org:28000",
      "se://demo/ABC",
      "se://demo/aaaa?hub=hub.example",  // a hash too short
      "se://quit/now",
  };
  for (const char* b : bad) {
    CHECK(!parse_link(b, "se", l, err));
    CHECK(!err.empty());
  }
  CHECK(!parse_link("se://demo/" + kSha, "se", l, err));  // no hub to fetch it from
  CHECK(!parse_link("se://demo/" + kSha + "?hub=a b", "se", l, err));
  CHECK(!parse_link("se://demo/" + std::string(64, 'A') + "?hub=h", "se", l, err));
  CHECK(link_host("hub.example") && link_host("1.2.3.4:28080") && !link_host("") &&
        !link_host("a:b") && !link_host(".x"));
}

TEST(plans_per_system) {
  const LinkPlan linux_plan =
      link_plan("se", "Shooter's Edge", "/opt/se/se_cli", LinkOs::linux_desktop, "/home/ann");
  CHECK(linux_plan.files.size() == 1);
  CHECK(linux_plan.files[0].path == "/home/ann/.local/share/applications/se-link.desktop");
  CHECK(has(linux_plan.files[0].text, "Exec=\"/opt/se/se_cli\" link open %u"));
  CHECK(has(linux_plan.files[0].text, "MimeType=x-scheme-handler/se;"));
  CHECK(linux_plan.commands.size() == 1 && linux_plan.commands[0][0] == "xdg-mime");
  CHECK(linux_plan.commands[0].back() == "x-scheme-handler/se");

  const LinkPlan mac =
      link_plan("se", "Shooter's Edge", "/Users/ann/se/se_cli", LinkOs::macos, "/Users/ann");
  CHECK(mac.handler == "/Users/ann/Applications/Shooter's Edge Links.app");
  CHECK(mac.files.size() == 1 && has(mac.files[0].text, "on open location this_link"));
  CHECK(has(mac.files[0].text, "quoted form of this_link"));
  CHECK(mac.commands.size() == 3 && mac.commands[0][0] == "osacompile");
  CHECK(mac.commands[1][0] == "plutil" &&
        has(mac.commands[1][4], "\"CFBundleURLSchemes\":[\"se\"]"));
  CHECK(has(mac.commands[2][0], "lsregister"));

  const LinkPlan win =
      link_plan("se", "Shooter's Edge", "C:\\Games\\se\\se_cli.exe", LinkOs::windows, "");
  CHECK(win.files.empty() && win.commands.size() == 3);
  CHECK(win.commands[0][2] == "HKCU\\Software\\Classes\\se");
  CHECK(win.commands[1][4] == "URL Protocol");
  CHECK(win.commands[2][5] == "\"C:\\Games\\se\\se_cli.exe\" link open \"%1\"");
}

TEST(demo_fetched_from_a_real_hub) {
  // A one-demo archive behind the hub's HTTP on loopback, served from a thread while the
  // fetch blocks.
  ge::replay::StreamHeader h;
  h.game = "test_game";
  h.meta = {{"map", "tower"}, {"mode", "duel"}};
  h.tracks = {{1, ge::replay::TrackKind::meta, ge::replay::TrackCodec::stored, "meta"}};
  std::vector<uint8_t> file;
  ge::replay::StreamWriter w;
  ge::replay::stream_begin(w, h, file);
  const uint8_t chunk[] = {9, 8, 7};
  ge::replay::stream_add(w, 1, 0, chunk);
  ge::replay::stream_end_block(w, true);
  ge::replay::stream_finish(w, {{"duration_ms", "1000"}});
  Archive a;
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "ge-hub-link";
  std::filesystem::remove_all(dir);
  std::string err;
  CHECK(archive_open(a, dir.string(), err));
  DemoRow row;
  CHECK(archive_add(a, file, 1, {}, err, &row) == AddResult::added);

  HttpServer s;
  CHECK(http_open(s, ge::net::loopback_v4(0), err));
  s.archive = &a;
  const std::string hub = "127.0.0.1:" + std::to_string(s.listener.local.port);
  std::atomic<bool> stop{false};
  std::thread serve([&] {
    Poller p;
    HubSpec spec;
    int64_t now = 0;
    while (!stop) {
      http_update(s, p, spec, now += 1000);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  });
  std::vector<uint8_t> got;
  const bool fetched = fetch_demo(hub, row.sha, got, err, 5000);
  std::string missing_err;
  std::vector<uint8_t> none;
  const bool missing = fetch_demo(hub, std::string(64, 'b'), none, missing_err, 5000);
  stop = true;
  serve.join();
  http_close(s);
  CHECK(fetched && got == file);
  CHECK(!missing && has(missing_err, "does not have that demo"));
  std::vector<uint8_t> nowhere;
  CHECK(!fetch_demo("hub.invalid;rm", row.sha, nowhere, err));
  std::filesystem::remove_all(dir);
}
