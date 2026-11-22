// plugins/classic/content/web_downloads.hpp: a classic game's downloads from a web address. The
// size words, a GitHub repository read as its raw files' address, the server's `dlserver` word,
// the address turned on or refused at connect, and against a loopback
// web server: files fetched and said, a file list followed (packs first, bad lines said), a 404
// from the server's address leaving the file lacking, and a failure that turns HTTP off handing
// the files back to the game's channel. A machine with no loopback socket skips the served cases.
//
// ge:summary Test: web downloads: GitHub, dlserver, sizes, file lists, 404, fall back to UDP
// ge:keywords test web-downloads dlserver filelist http-download fallback github
// ge:skill classic net testing
#include "classic/content/web_downloads.hpp"

#include <map>
#include <string>
#include <vector>

#include "check.hpp"
#include "net/tcp_socket.hpp"

using namespace ge::classic::content;
namespace gn = ge::net;

namespace {

// A web server on loopback answering each path from `files` (404 for the rest).
struct Web {
  gn::TcpListener listener;
  std::map<std::string, std::string> files;
  std::vector<gn::TcpStream> streams;
  std::vector<std::string> requests;
  std::vector<std::string> asked;  // the paths, in order
};

void serve(Web& w) {
  for (gn::TcpStream in; gn::tcp_accept(w.listener, in); in = {}) {  // accept closes what it fills
    w.streams.push_back(in);
    w.requests.emplace_back();
  }
  for (size_t i = 0; i < w.streams.size(); ++i) {
    gn::TcpStream& t = w.streams[i];
    if (t.handle == -1) continue;
    gn::tcp_pump(t);
    std::vector<uint8_t> got;
    gn::tcp_take_bytes(t, got);
    w.requests[i].append(got.begin(), got.end());
    const std::string& r = w.requests[i];
    if (!r.empty() && r[0] == '\x16') {  // a TLS hello: this server speaks plain HTTP alone
      const std::string no = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
      gn::tcp_send_bytes(t, reinterpret_cast<const uint8_t*>(no.data()), no.size());
      for (int k = 0; k < 100000 && !t.out.empty(); ++k) gn::tcp_pump(t);
      gn::tcp_close(t);
      continue;
    }
    if (r.find("\r\n\r\n") == std::string::npos) continue;
    const size_t sp = r.find(' ');
    const std::string path = r.substr(sp + 1, r.find(' ', sp + 1) - sp - 1);
    w.asked.push_back(path);
    const auto it = w.files.find(path);
    const std::string reply =
        it == w.files.end()
            ? "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n"
            : "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(it->second.size()) +
                  "\r\n\r\n" + it->second;
    gn::tcp_send_bytes(t, reinterpret_cast<const uint8_t*>(reply.data()), reply.size());
    for (int k = 0; k < 100000 && !t.out.empty(); ++k) gn::tcp_pump(t);
    gn::tcp_close(t);
  }
}

bool open(Web& w) {
  std::string err;
  if (!gn::tcp_listen(w.listener, gn::loopback_v4(0), &err)) {
    ge::test::skip("no loopback TCP socket here: " + err);
    return false;
  }
  return true;
}

std::string base_of(const Web& w) {
  return "http://127.0.0.1:" + std::to_string(w.listener.local.port) + "/q2/";
}

// Steps `h` until it is idle, gathering what it finished.
void run(WebDownloads& h, const WebConfig& cfg, Web& w, WebDone& all,
         const std::function<bool(const std::string&, bool)>& wanted) {
  double now = 0;
  for (int i = 0; i < 400000 && web_busy(h); ++i) {
    serve(w);
    now += 0.0001;
    WebDone d;
    web_step(h, cfg, now, wanted, d);
    for (auto& f : d.files) all.files.push_back(std::move(f));
    for (auto& b : d.back) all.back.push_back(std::move(b));
  }
}

bool has(const std::vector<std::string>& lines, const std::string& prefix) {
  for (const std::string& l : lines)
    if (l.rfind(prefix, 0) == 0) return true;
  return false;
}

}  // namespace

TEST(web_size_words) {
  CHECK(web_size_text(-1) == "unknown size");
  CHECK(web_size_text(0) == "0 bytes");
  CHECK(web_size_text(1) == "1 byte");
  CHECK(web_size_text(999) == "999 bytes");
  CHECK(web_size_text(1000) == "1 kB");
  CHECK(web_size_text(340500) == "340 kB");
  CHECK(web_size_text(1500000) == "1.5 MB");
  CHECK(web_size_text(34900000) == "34 MB");
  CHECK(web_size_text(1200000000) == "1.2 GB");
}

TEST(web_github_repositories_read_as_raw_files) {
  CHECK(web_base("github:quakers/maps") == "https://raw.githubusercontent.com/quakers/maps/HEAD/");
  CHECK(web_base("github:quakers/maps@v2/q2") ==
        "https://raw.githubusercontent.com/quakers/maps/v2/q2/");
  CHECK(web_base("https://github.com/quakers/maps") ==
        "https://raw.githubusercontent.com/quakers/maps/HEAD/");
  CHECK(web_base("https://github.com/quakers/maps.git/") ==
        "https://raw.githubusercontent.com/quakers/maps/HEAD/");
  CHECK(web_base("https://github.com/quakers/maps/tree/main/files/q2") ==
        "https://raw.githubusercontent.com/quakers/maps/main/files/q2/");
  CHECK(web_base("http://dl.example/q2/") == "http://dl.example/q2/");
  CHECK(web_base("github:alone") == "github:alone");
  {
    // The game folder is added to every path; a folder that names it is read as its parent.
    WebConfig qw;
    qw.gamedir = "qw";
    WebDownloads h;
    web_connect(h, qw, "github:quakers/maps/quakeworld", 1, "s");
    CHECK(h.base == "https://raw.githubusercontent.com/quakers/maps/HEAD/quakeworld/");
    web_connect(h, qw, "github:quakers/maps/quakeworld/QW", 1, "s");
    CHECK(h.base == "https://raw.githubusercontent.com/quakers/maps/HEAD/quakeworld/");
    web_connect(h, qw, "github:quakers/maps/qw", 1, "s");
    CHECK(h.base == "https://raw.githubusercontent.com/quakers/maps/HEAD/");
    web_connect(h, qw, "github:quakers/maps@qw", 1, "s");  // a branch named qw stays
    CHECK(h.base == "https://raw.githubusercontent.com/quakers/maps/qw/");
    web_connect(h, qw, "http://dl.example/qw/", 1, "s");  // a server's own address stays as named
    CHECK(h.base == "http://dl.example/qw/");
    CHECK(!h.game_in_base);
    // Set by hand, for a repository laid out otherwise: the game folder not added again.
    web_connect(h, qw, "github:quakers/maps/{game}/files", 1, "s");
    CHECK(h.base == "https://raw.githubusercontent.com/quakers/maps/HEAD/qw/files/");
    CHECK(h.game_in_base);
    web_connect(h, qw, "github:quakers/maps/odd/place#", 1, "s");
    CHECK(h.base == "https://raw.githubusercontent.com/quakers/maps/HEAD/odd/place/");
    CHECK(h.game_in_base);
  }
  WebDownloads h;
  web_connect(h, {}, "github:quakers/maps@main", 1, "s");
  CHECK(h.on && h.base == "https://raw.githubusercontent.com/quakers/maps/main/");
  CHECK(h.said.size() == 1 &&
        h.said[0] ==
            "[HTTP] Download server at https://raw.githubusercontent.com/quakers/maps/main/");
}

TEST(web_address_at_connect) {
  CHECK(web_dlserver({"client_connect", "dlserver=http://a/", "dlserver=http://b/", "map=q2dm1"}) ==
        "http://a/");
  CHECK(web_dlserver({"client_connect", "map=q2dm1"}).empty());
  WebDownloads h;
  WebConfig cfg;
  web_connect(h, cfg, "http://dl.example/q2/", 1, "10.0.0.1:27910");
  CHECK(h.on && !h.default_repo && h.base == "http://dl.example/q2/");
  CHECK(h.referer == "quake2://10.0.0.1:27910");
  CHECK(h.said.size() == 1 && h.said[0] == "[HTTP] Download server at http://dl.example/q2/");
  web_connect(h, cfg, "ftp://x/", 1, "s");
  CHECK(!h.on && h.said.size() == 1 &&
        h.said[0] == "[HTTP] Ignoring download server URL with non-HTTP schema.");
  web_connect(h, cfg, "http://" + std::string(600, 'a'), 1, "s");
  CHECK(!h.on && h.said[0] == "[HTTP] Ignoring oversize download server URL.");
  web_connect(h, cfg, "http://dl.example/", -1, "s");  // allow_download -1
  CHECK(!h.on && h.said.empty());
  web_connect(h, cfg, "", 1, "s");  // none named and no repository of your own
  CHECK(!h.on && h.said.empty());
  cfg.default_url = "http://mine.example/";
  web_connect(h, cfg, "", 1, "s");
  CHECK(h.on && h.default_repo && h.base == "http://mine.example/");
  CHECK(!web_queue(h, cfg, "maps/a.bsp"));  // your own: the server is not told
  cfg.downloads = false;
  web_connect(h, cfg, "http://dl.example/", 1, "s");
  CHECK(!h.on);
}

TEST(web_files_and_file_lists) {
  Web w;
  if (!open(w)) return;
  w.files["/q2/baseq2/maps/q2dm1.bsp"] = std::string(1500, 'm');
  w.files["/q2/baseq2.filelist"] =
      "pics/extra.pcx\r\nmaps/notes.exe\n@sound/own.wav\nbig.pak\n@other.pak\n../up.pcx\n";
  w.files["/q2/baseq2/maps/q2dm1.filelist"] = "textures/e1/wall.wal\n";
  w.files["/q2/baseq2/pics/extra.pcx"] = "pcx";
  w.files["/q2/baseq2/sound/own.wav"] = "wav";
  w.files["/q2/baseq2/big.pak"] = "PACK";
  w.files["/q2/baseq2/textures/e1/wall.wal"] = "wal";
  WebDownloads h;
  WebConfig cfg;
  cfg.user_agent = "test-agent";
  web_connect(h, cfg, base_of(w), 1, "127.0.0.1:27910");
  CHECK(web_queue(h, cfg, "maps/q2dm1.bsp"));  // the first file tells the server
  CHECK(!web_queue(h, cfg, "players/male/tris.md2"));
  std::vector<std::pair<std::string, bool>> checked;
  WebDone all;
  run(h, cfg, w, all, [&checked](const std::string& path, bool game_only) {
    checked.emplace_back(path, game_only);
    return true;
  });
  std::map<std::string, std::string> got;
  for (auto& [path, bytes] : all.files) got[path] = std::string(bytes.begin(), bytes.end());
  CHECK(got.size() == 5);
  CHECK(got["maps/q2dm1.bsp"].size() == 1500 && got["big.pak"] == "PACK");
  CHECK(got["pics/extra.pcx"] == "pcx" && got["sound/own.wav"] == "wav");
  CHECK(got["textures/e1/wall.wal"] == "wal");
  CHECK(all.back.empty());  // a 404 from the server's address hands nothing back
  CHECK(has(h.said, "[HTTP] maps/q2dm1.bsp [1 kB, "));
  CHECK(has(h.said, "[HTTP] players/male/tris.md2 [404 Not Found] ["));
  CHECK(has(h.said, "[HTTP] Filelist is requesting a .pak file 'big.pak'"));
  CHECK(has(h.said, "[HTTP] Illegal file type 'maps/notes.exe' in filelist."));
  CHECK(has(h.said, "[HTTP] '@' prefix used on a pak file '@other.pak' in filelist."));
  CHECK(has(h.said, "[HTTP] Illegal path '../up.pcx' in filelist."));
  CHECK(has(h.said, "[HTTP] baseq2.filelist ["));
  // `@` asks the game folder alone; a pack is always looked for there.
  bool own = false, pak = false;
  for (auto& [path, game_only] : checked) {
    if (path == "sound/own.wav") own = game_only;
    if (path == "big.pak") pak = game_only;
  }
  CHECK(own && pak);
  // The pack came alone, before the files after it in the queue.
  size_t pak_at = 0, wal_at = 0;
  for (size_t i = 0; i < w.asked.size(); ++i) {
    if (w.asked[i] == "/q2/baseq2/big.pak") pak_at = i;
    if (w.asked[i] == "/q2/baseq2/textures/e1/wall.wal") wal_at = i;
  }
  CHECK(pak_at != 0 && pak_at < wal_at);
  CHECK(w.requests[0].find("\r\nUser-Agent: test-agent\r\n") != std::string::npos);
  CHECK(w.requests[0].find("\r\nReferer: quake2://127.0.0.1:27910\r\n") != std::string::npos);
  for (gn::TcpStream& t : w.streams) gn::tcp_close(t);
  gn::tcp_close_listener(w.listener);
}

TEST(web_falls_back_to_the_channel) {
  Web w;
  if (!open(w)) return;
  // The player's own repository lacks the map: a 404 there turns HTTP off, every file handed back.
  WebDownloads h;
  WebConfig cfg;
  cfg.filelists = false;
  cfg.max_connections = 1;
  cfg.default_url = base_of(w);
  web_connect(h, cfg, "", 1, "s");
  CHECK(h.on && h.default_repo);
  CHECK(!web_queue(h, cfg, "maps/q2dm9.bsp"));
  web_queue(h, cfg, "sound/a.wav");
  web_queue(h, cfg, "sound/b.wav");
  WebDone all;
  run(h, cfg, w, all, [](const std::string&, bool) { return true; });
  CHECK(!h.on && all.files.empty());
  CHECK(all.back == (std::vector<std::string>{"maps/q2dm9.bsp", "sound/a.wav", "sound/b.wav"}));
  CHECK(has(h.said, "[HTTP] maps/q2dm9.bsp [404 Not Found] [2 remaining files]"));
  // A secure address whose server speaks no TLS: off at the first file, the file handed back.
  WebDownloads s;
  web_connect(s, cfg, "https://127.0.0.1:" + std::to_string(w.listener.local.port) + "/q2/", 1,
              "s");
  web_queue(s, cfg, "maps/q2dm1.bsp");
  WebDone back;
  run(s, cfg, w, back, [](const std::string&, bool) { return true; });
  CHECK(!s.on && back.back == std::vector<std::string>{"maps/q2dm1.bsp"});
  CHECK(has(s.said, "[HTTP] maps/q2dm1.bsp [SSL connect error]"));
  for (gn::TcpStream& t : w.streams) gn::tcp_close(t);
  gn::tcp_close_listener(w.listener);
}
