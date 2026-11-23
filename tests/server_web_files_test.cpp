// engine/server/web_files.hpp: the sv_http_ settings read (the caps' defaults, a value out of
// range, a certificate without its key, an address that is not one), the address told (the
// server's own, `{server}` until sv_publicaddr names the host, sv_http_url as set) and, on
// loopback, the server's own certificate trusted by the digest it tells and its file fetched.
//
// ge:summary Test: a dedicated server's web files: settings, the address told, its own certificate
// ge:keywords test web-files sv_http_port sv_http_rate certificate-pin
// ge:skill net testing
#include "server/web_files.hpp"

#include <string>
#include <vector>

#include "check.hpp"
#include "net/http_fetch.hpp"

using namespace ge::server;
namespace gn = ge::net;

TEST(web_files_settings_and_their_defaults) {
  ServerConfig c;
  WebFilesConfig w;
  std::string err;
  CHECK(web_files_config(c, w, err));
  CHECK(w.port == 0 && w.rate_kb == 20000 && w.rate_player_kb == 5000 && !w.plain);
  c.rules["sv_http_port"] = "27600";
  c.rules["sv_http_rate"] = "0";
  c.rules["sv_http_url"] = "https://files.example.org/quake";
  c.public_addr = "games.example.org:27500";
  CHECK(web_files_config(c, w, err));
  CHECK(w.port == 27600 && w.rate_kb == 0 && w.url == "https://files.example.org/quake/");
  CHECK(w.public_host == "games.example.org");
  c.rules["sv_http_port"] = "70000";
  CHECK(!web_files_config(c, w, err) && err.find("sv_http_port") != std::string::npos);
  c.rules["sv_http_port"] = "27600";
  c.rules["sv_http_cert"] = "/etc/ge/cert.pem";
  CHECK(!web_files_config(c, w, err) && err.find("sv_http_key") != std::string::npos);
  c.rules.erase("sv_http_cert");
  c.rules["sv_http_url"] = "ftp://x/";
  CHECK(!web_files_config(c, w, err) && err.find("set it to https") != std::string::npos);
  CHECK(web_files_told("https://a.example/") && !web_files_told("https://{server}:1/"));
  CHECK(web_files_at("https://{server}:27600/", "10.0.0.2") == "https://10.0.0.2:27600/");
}

TEST(web_files_trusted_by_the_digest_told) {
  WebFiles w;
  WebFilesConfig cfg;
  cfg.port = 47613;
  std::string err;
  const std::vector<uint8_t> file(5000, 7);
  if (!web_files_open(
          w, cfg, gn::loopback_v4(0),
          [&file](const std::string& path, std::vector<uint8_t>& out) {
            out = file;
            return path == "qw/maps/a.bsp";
          },
          err)) {
    ge::test::skip("no loopback TCP port 47613 here: " + err);
    return;
  }
  CHECK(w.url == "https://{server}:47613/" && w.pin.size() == 64);
  gn::HttpOptions o;
  for (size_t i = 0; i < 64; i += 2)
    o.pin.push_back(static_cast<uint8_t>(std::stoi(w.pin.substr(i, 2), nullptr, 16)));
  gn::HttpFetch f;
  double now = 0;
  gn::http_start(f, web_files_at(w.url, "127.0.0.1") + "qw/maps/a.bsp", o, now);
  for (int i = 0; i < 200000 && !gn::http_step(f, now); ++i) {
    web_files_frame(w, now);
    now += 0.001;
  }
  CHECK_MSG(f.state == gn::HttpState::done, f.error);
  CHECK(f.body == file && gn::http_digest_ok(f, f.body));
  web_files_close(w);
  CHECK(w.url.empty());
}
