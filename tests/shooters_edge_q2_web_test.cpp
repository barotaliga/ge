// games/shooters_edge/net/q2_client.hpp and q2_net_server.hpp: Quake II's downloads from a web
// address. The server names one (sv_downloadserver) and the client fetches there, falling back to
// the channel; the server's own HTTPS files (sv_http_port) are trusted by the certificate digest
// its answer to `connect` names (` dlpin=`).
//
// ge:summary Test: Quake II's web downloads, a server's address and its own HTTPS files by pin
// ge:keywords test q2 web-downloads dlserver dlpin https sv_http_port fallback channel
// ge:skill net shooter testing
#include <algorithm>
#include <string>
#include <vector>

#include "check.hpp"
#include "net/tcp_socket.hpp"
#include "server/web_files.hpp"
#include "shooters_edge_q2_rig.hpp"

namespace {

namespace gn = ge::net;
namespace cn = shooters_edge::net;
using q2_rig::join;
using q2_rig::rig;
using q2_rig::run;

TEST(a_client_fetches_from_the_server_web_address_and_falls_back_to_the_channel) {
  // The server names a web address (sv_downloadserver). The web holds the map: the client fetches
  // it there, says so, and tells the server `download http`; a skin the web lacks (404) is left
  // lacking. A second client's own repository (cl_http_default_url) lacks the map: HTTP turns off
  // and the map comes over the game's channel. Both play on.
  gn::TcpListener web;
  std::string error;
  if (!gn::tcp_listen(web, gn::loopback_v4(0), &error)) {
    ge::test::skip("no loopback TCP socket here: " + error);
    return;
  }
  const std::string base = "http://127.0.0.1:" + std::to_string(web.local.port) + "/";
  const std::vector<uint8_t> map(5000, 1);
  cn::Q2NetConfig cfg;
  cfg.download_server = base;
  cfg.read_file = [&map](const std::string& path, std::vector<uint8_t>* out) {
    if (path != "maps/hall.bsp") return false;
    *out = map;
    return true;
  };
  auto r = rig(cfg);
  cn::Q2ClientConfig web_one;
  web_one.name = "web";
  web_one.fetch = true;
  web_one.want = {"players/male/red.pcx"};
  cn::Q2ClientConfig own = web_one;
  own.name = "own";
  own.http.default_url = base + "mine/";
  cn::Q2ClientEnd& a = join(*r, web_one);
  std::vector<gn::TcpStream> streams;
  std::vector<std::string> asked;
  for (int i = 0; i < 400; i++) {
    if (i == 30) {  // the second joins once the server names no address: its own is asked
      r->server.cfg.download_server.clear();
      join(*r, own);
    }
    for (gn::TcpStream in; gn::tcp_accept(web, in); in = {}) streams.push_back(in);
    for (gn::TcpStream& t : streams) {
      if (t.handle == -1) continue;
      gn::tcp_pump(t);
      std::vector<uint8_t> got;
      gn::tcp_take_bytes(t, got);
      const std::string req(got.begin(), got.end());
      if (req.find("\r\n\r\n") == std::string::npos) continue;
      const std::string path = req.substr(4, req.find(' ', 4) - 4);
      asked.push_back(path);
      const bool has = path == "/baseq2/maps/hall.bsp";
      std::string reply = has ? "HTTP/1.1 200 OK\r\nContent-Length: 5000\r\n\r\n"
                              : "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
      if (has) reply.append(map.begin(), map.end());
      gn::tcp_send_bytes(t, reinterpret_cast<const uint8_t*>(reply.data()), reply.size());
      for (int k = 0; k < 100000 && !t.out.empty(); ++k) gn::tcp_pump(t);
      gn::tcp_close(t);
    }
    run(*r, 1);
  }
  cn::Q2ClientEnd& b = *r->clients[1];
  CHECK_MSG(a.stage == cn::ClientStage::active, a.refused);
  CHECK_MSG(b.stage == cn::ClientStage::active, b.refused);
  CHECK(a.downloaded.size() == 1 && a.downloaded["maps/hall.bsp"] == map);
  CHECK(std::find(asked.begin(), asked.end(), "/baseq2/players/male/red.pcx") != asked.end());
  CHECK(std::find(asked.begin(), asked.end(), "/baseq2.filelist") != asked.end());
  CHECK(std::find(asked.begin(), asked.end(), "/baseq2/maps/hall.filelist") != asked.end());
  CHECK(b.downloaded.size() == 1 && b.downloaded["maps/hall.bsp"] == map);
  CHECK(!b.http.on);
  CHECK(std::find(b.http.said.begin(), b.http.said.end(),
                  "[UDP] maps/hall.bsp [DONE] [1 remaining file]") != b.http.said.end());
  CHECK(std::find(a.http.said.begin(), a.http.said.end(), "[HTTP] Download server at " + base) !=
        a.http.said.end());
  CHECK(std::find(asked.begin(), asked.end(), "/mine/baseq2/maps/hall.bsp") != asked.end());
  for (gn::TcpStream& t : streams) gn::tcp_close(t);
  gn::tcp_close_listener(web);
}

TEST(a_client_fetches_the_map_from_the_servers_own_https_files_by_their_pin) {
  // sv_http_port on a Quake II server: its own web files over HTTPS under a certificate it made
  // itself. The answer to `connect` names the address and the certificate's digest (` dlpin=`);
  // the client trusts that certificate alone, takes the map from there, checked by its SHA-256,
  // and plays on. The channel sends no piece of it.
  const std::vector<uint8_t> map(150000, 0x3c);
  std::vector<std::string> served;
  ge::server::WebFiles wf;
  ge::server::WebFilesConfig wc;
  wc.port = 47615;
  std::string error;
  if (!ge::server::web_files_open(
          wf, wc, gn::loopback_v4(0),
          [&](const std::string& path, std::vector<uint8_t>& out) {
            served.push_back(path);
            if (path != "baseq2/maps/hall.bsp") return false;
            out = map;
            return true;
          },
          error)) {
    ge::test::skip("no loopback TCP port 47615 here: " + error);
    return;
  }
  CHECK(wf.pin.size() == 64 && wf.url == "https://{server}:47615/");
  cn::Q2NetConfig cfg;
  // The simulated server's host is no real one: the address told names loopback instead.
  cfg.web_url = ge::server::web_files_at(wf.url, "127.0.0.1");
  cfg.web_pin = wf.pin;
  cfg.read_file = [&map](const std::string& path, std::vector<uint8_t>* out) {
    if (path != "maps/hall.bsp") return false;
    *out = map;
    return true;
  };
  auto r = rig(cfg);
  cn::Q2ClientConfig cc;
  cc.name = "fast";
  cc.fetch = true;
  cn::Q2ClientEnd& c = join(*r, cc);
  for (int i = 0; i < 600 && c.stage != cn::ClientStage::active; i++) {
    ge::server::web_files_frame(wf, static_cast<double>(r->now) / 1e6);
    run(*r, 1);
  }
  std::string said;
  for (const std::string& l : c.http.said) said += l + "\n";
  for (const std::string& l : served) said += "served " + l + "\n";
  CHECK_MSG(c.stage == cn::ClientStage::active, c.refused + "\n" + said);
  CHECK_MSG(c.downloaded.size() == 1 && c.downloaded["maps/hall.bsp"] == map, said);
  CHECK(c.http.pin.size() == 32);
  CHECK_MSG(std::find(served.begin(), served.end(), "baseq2/maps/hall.bsp") != served.end(), said);
  CHECK(std::find(c.http.said.begin(), c.http.said.end(),
                  "[HTTP] Download server at " + cfg.web_url) != c.http.said.end());
  ge::server::web_files_close(wf);
}

}  // namespace
