// A classic game's downloads from a web address (web_downloads.hpp).
//
// ge:summary The web downloads: the address, the queue, the file lists, the lines said
// ge:keywords web-downloads http-download dlserver filelist github
// ge:skill classic net
// ge:test tests/classic_content_web_downloads_test.cpp
#include "classic/content/web_downloads.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <iterator>
#include <utility>

#include "vfs/unpack.hpp"

namespace ge::classic::content {

namespace gn = ge::net;

namespace {

constexpr size_t kMostUrl = 512;
constexpr size_t kMostList = 1024 * 1024 - 1;  // a file list's bytes

std::string lowered(std::string s) {
  for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return s;
}

std::string gamedir_of(const WebConfig& cfg) {
  return cfg.gamedir.empty() ? cfg.base_folder : cfg.gamedir;
}

constexpr const char* kAcceptEncoding = "gzip, zstd, xz, bzip2";

// `f` asked again at the front of the queue, in its next form (no line said for a form missing).
void ask_again(WebDownloads& h, WebFile f, int packing) {
  f.packing = packing;
  h.queue.insert(h.queue.begin(), std::move(f));
}

// A body's packing peeled: a packed form asked, then the server's own `Content-Encoding`.
bool unpacked(const gn::HttpFetch& f, std::vector<uint8_t> sent, int packing, size_t most,
              std::vector<uint8_t>& out, std::string& why) {
  out = std::move(sent);
  std::vector<uint8_t> inner;
  const std::string& e = f.encoding;
  if (!e.empty() && e != "identity") {
    if (e != "gzip" && e != "x-gzip" && e != "zstd" && e != "xz" && e != "x-xz" && e != "bzip2" &&
        e != "x-bzip2") {
      why = "sent packed as " + e;
      return false;
    }
    if (!ge::vfs::unpack_layer(out, inner, why, most)) return false;
    out.swap(inner);
  }
  if (packing >= 0 && packing < kWebPlain) {
    if (!ge::vfs::unpack_layer(out, inner, why, most)) return false;
    out.swap(inner);
  }
  return true;
}

int hex_digit(char ch) {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
  if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
  return -1;
}

// A certificate digest told as 64 hex digits, as its 32 bytes; empty when it is not one.
std::vector<uint8_t> pin_bytes(const std::string& hex) {
  std::vector<uint8_t> out;
  if (hex.size() != 64) return out;
  for (size_t i = 0; i < 64; i += 2) {
    const int a = hex_digit(hex[i]), b = hex_digit(hex[i + 1]);
    if (a < 0 || b < 0) return {};
    out.push_back(static_cast<uint8_t>(a * 16 + b));
  }
  return out;
}

int remaining(const WebDownloads& h) { return static_cast<int>(h.queue.size() + h.slots.size()); }

bool name_char(char ch) {
  return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_' || ch == '-';
}

// Queues `f` once a connection: a pack after the packs queued, ahead of the rest.
void push(WebDownloads& h, const WebFile& f) {
  if (!h.asked.insert(f.path).second) return;
  if (!f.pak) {
    h.queue.push_back(f);
    return;
  }
  auto at = std::find_if(h.queue.begin(), h.queue.end(), [](const WebFile& q) { return !q.pak; });
  h.queue.insert(at, f);
}

// HTTP off for this connection: every file not fetched yet handed back to the channel.
void turn_off(WebDownloads& h, const WebFile& failed, WebDone& done) {
  h.on = false;
  if (!failed.filelist && !failed.pak) done.back.push_back(failed.path);
  for (WebSlot& s : h.slots) {
    gn::http_cancel(s.fetch);
    if (!s.file.filelist && !s.file.pak) done.back.push_back(s.file.path);
  }
  h.slots.clear();
  for (const WebFile& f : h.queue)
    if (!f.filelist && !f.pak) done.back.push_back(f.path);
  h.queue.clear();
}

// A file list's lines queued: the kinds a server sends, the packs said and put first.
void read_list(WebDownloads& h, const WebConfig& cfg, const std::vector<uint8_t>& body,
               const std::function<bool(const std::string&, bool)>& wanted) {
  std::string text(body.begin(), body.end());
  size_t at = 0;
  while (at < text.size()) {
    size_t end = text.find('\n', at);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(at, end - at);
    at = end + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line.size() >= 64) continue;
    const size_t dot = line.rfind('.');
    if (dot == std::string::npos || dot + 1 >= line.size()) continue;
    const std::string kind = lowered(line.substr(dot + 1));
    const bool pak = kind == "pak" || kind == "pkz";
    if (pak) {
      std::string said = "[HTTP] Filelist is requesting a .";
      said += kind;
      said += " file '";
      said += line;
      said += "'";
      h.said.push_back(std::move(said));
    } else if (std::find(cfg.kinds.begin(), cfg.kinds.end(), kind) == cfg.kinds.end()) {
      h.said.push_back("[HTTP] Illegal file type '" + line + "' in filelist.");
      continue;
    }
    bool game_only = pak;
    if (line[0] == '@') {
      if (pak) {
        h.said.push_back("[HTTP] '@' prefix used on a pak file '" + line + "' in filelist.");
        continue;
      }
      game_only = true;
      line.erase(0, 1);
    }
    for (char& ch : line)
      if (ch == '\\') ch = '/';
    const bool slash = line.find('/') != std::string::npos;
    if (line.empty() || !name_char(line.front()) || !name_char(line.back()) ||
        line.find("..") != std::string::npos || line.find("//") != std::string::npos ||
        slash == pak) {
      h.said.push_back("[HTTP] Illegal path '" + line + "' in filelist.");
      continue;
    }
    line = lowered(line);
    if (line.rfind("save/", 0) == 0 || line.rfind("music/", 0) == 0 || line.rfind("video/", 0) == 0)
      continue;
    if (!wanted(line, game_only)) continue;
    push(h, {.path = line, .filelist = false, .pak = pak});
  }
}

}  // namespace

std::string web_files_left(int n) {
  return "[" + std::to_string(n) + " remaining file" + (n == 1 ? "]" : "s]");
}

std::vector<std::string> quake_web_kinds() {
  return {"bsp", "ent", "jpg", "lit", "loc", "mdl", "ogg",
          "pcx", "png", "spr", "tga", "txt", "wad", "wav"};
}

std::string web_base(const std::string& text) {
  // `github:<owner>/<repo>[@<ref>][/<folder>]` or `https://github.com/<owner>/<repo>[/tree/<ref>
  // [/<folder>]]`: the raw files' address, the default branch (`HEAD`) when no ref is named.
  std::string rest, ref = "HEAD";
  const std::string low = lowered(text.substr(0, 19));
  if (low.rfind("github:", 0) == 0) {
    rest = text.substr(7);
  } else if (low.rfind("https://github.com/", 0) == 0 || low.rfind("http://github.com/", 0) == 0) {
    rest = text.substr(text.find("github.com/") + 11);
  } else {
    return text;
  }
  while (!rest.empty() && rest.back() == '/') rest.pop_back();
  const size_t a = rest.find('/');
  if (a == std::string::npos || a == 0) return text;
  const std::string owner = rest.substr(0, a);
  std::string tail = rest.substr(a + 1);
  size_t b = tail.find('/');
  std::string repo = tail.substr(0, b);
  std::string folder = b == std::string::npos ? std::string() : tail.substr(b + 1);
  if (const size_t at = repo.find('@'); at != std::string::npos) {  // github:o/r@ref/folder
    ref = repo.substr(at + 1);
    repo.resize(at);
  } else if (folder.rfind("tree/", 0) == 0) {  // the page of a branch or a folder in it
    folder.erase(0, 5);
    const size_t c = folder.find('/');
    ref = folder.substr(0, c);
    folder = c == std::string::npos ? std::string() : folder.substr(c + 1);
  }
  if (repo.size() > 4 && repo.substr(repo.size() - 4) == ".git") repo.resize(repo.size() - 4);
  if (repo.empty() || ref.empty()) return text;
  std::string url = "https://raw.githubusercontent.com/" + owner + "/" + repo + "/" + ref + "/";
  if (!folder.empty()) url += folder + "/";
  return url;
}

std::string web_size_text(int64_t bytes) {
  char buf[48];
  if (bytes < 0) return "unknown size";
  const double d = static_cast<double>(bytes);
  if (bytes >= 1000000000) {
    std::snprintf(buf, sizeof buf, "%.1f GB", d / 1e9);
  } else if (bytes >= 10000000) {
    std::snprintf(buf, sizeof buf, "%lld MB", static_cast<long long>(bytes / 1000000));
  } else if (bytes >= 1000000) {
    std::snprintf(buf, sizeof buf, "%.1f MB", d / 1e6);
  } else if (bytes >= 1000) {
    std::snprintf(buf, sizeof buf, "%lld kB", static_cast<long long>(bytes / 1000));
  } else {
    std::snprintf(buf, sizeof buf, "%lld byte%s", static_cast<long long>(bytes),
                  bytes == 1 ? "" : "s");
  }
  return buf;
}

std::string web_dlserver(const std::vector<std::string>& words) {
  for (const std::string& w : words)
    if (w.rfind("dlserver=", 0) == 0) return w.substr(9);
  return {};
}

std::string web_dlpin(const std::vector<std::string>& words) {
  for (const std::string& w : words)
    if (w.rfind("dlpin=", 0) == 0) return w.substr(6);
  return {};
}

void web_connect(WebDownloads& h, const WebConfig& cfg, const std::string& dlserver, int all,
                 const std::string& server, const std::string& pin) {
  web_stop(h);
  h = {};
  if (all < 0 || !cfg.downloads) return;
  std::string url = dlserver;
  // The server's own web files: `{server}` the host joined (its port dropped), the certificate
  // trusted by the digest it told.
  if (const size_t at = url.find("{server}"); at != std::string::npos) {
    std::string host = server;
    const size_t colon = host.rfind(':');
    if (colon != std::string::npos && host.find(':') == colon) host.resize(colon);
    url.replace(at, 8, host);
  }
  if (!dlserver.empty()) h.pin = pin_bytes(pin);
  // The server's own web files keep every file as it is: no packed form asked first.
  if (!h.pin.empty()) h.packing = kWebPlain;
  h.default_repo = url.empty();
  if (url.empty()) url = cfg.default_url;
  if (url.empty()) return;
  // Set by hand: `{game}` where the game folder goes, or `#` at the end for an address that is the
  // game folder's files already; neither added again to each path.
  bool set = false;
  if (!url.empty() && url.back() == '#') {
    url.pop_back();
    set = true;
  }
  const std::string named = url;
  url = web_base(url);
  if (const size_t at = lowered(url).find("{game}"); at != std::string::npos) {
    url.replace(at, 6, gamedir_of(cfg));
    set = true;
  }
  if (!url.empty() && url.back() != '/') url += '/';
  // A repository's folder that names the game folder itself (`github:o/r/quakeworld/qw` while
  // `qw` is played) is read as its parent, as the game folder is added to every path anyway. Only
  // a folder: never the owner, the repository or the branch (three names after the host).
  const std::string tail = "/" + lowered(gamedir_of(cfg)) + "/";
  const size_t host = std::string("https://raw.githubusercontent.com/").size();
  if (!set && url != named && url.size() > host + tail.size() &&
      lowered(url).compare(url.size() - tail.size(), tail.size(), tail) == 0) {
    const size_t cut = url.size() - tail.size() + 1;
    if (std::count(url.begin() + static_cast<ptrdiff_t>(host),
                   url.begin() + static_cast<ptrdiff_t>(cut), '/') >= 3)
      url.resize(cut);
  }
  const std::string scheme = lowered(url.substr(0, 8));
  if (scheme.rfind("http://", 0) != 0 && scheme.rfind("https://", 0) != 0) {
    h.said.push_back("[HTTP] Ignoring download server URL with non-HTTP schema.");
    return;
  }
  if (url.size() >= kMostUrl) {
    h.said.push_back("[HTTP] Ignoring oversize download server URL.");
    return;
  }
  h.on = true;
  h.game_in_base = set;
  h.base = url;
  h.referer = cfg.referer_scheme + "://" + server;
  h.said.push_back("[HTTP] Download server at " + url);
}

bool web_queue(WebDownloads& h, const WebConfig& cfg, const std::string& path) {
  if (!h.on) return false;
  bool tell = false;
  if (!h.told_server) {
    h.told_server = true;
    tell = !h.default_repo;
  }
  // The address set to the game folder's own files: its maps' lists under it, no list beside it.
  const std::string dir = h.game_in_base ? std::string() : gamedir_of(cfg);
  if (cfg.filelists && !h.asked_list && !h.game_in_base) {
    h.asked_list = true;
    push(h, {.path = dir + ".filelist", .filelist = true, .pak = false});
  }
  push(h, {.path = path, .filelist = false, .pak = false});
  if (cfg.filelists && path.size() > 4 && lowered(path.substr(path.size() - 4)) == ".bsp")
    push(h,
         {.path = (dir.empty() ? dir : dir + "/") + path.substr(0, path.size() - 4) + ".filelist",
          .filelist = true,
          .pak = false});
  return tell;
}

bool web_busy(const WebDownloads& h) { return !h.queue.empty() || !h.slots.empty(); }

void web_step(WebDownloads& h, const WebConfig& cfg, double now_s,
              const std::function<bool(const std::string& path, bool game_only)>& wanted,
              WebDone& done) {
  // The fetches that ended.
  for (size_t i = 0; i < h.slots.size() && h.on;) {
    WebSlot& s = h.slots[i];
    if (!gn::http_step(s.fetch, now_s)) {
      ++i;
      continue;
    }
    WebSlot ended = std::move(s);
    h.slots.erase(h.slots.begin() + static_cast<ptrdiff_t>(i));
    const gn::HttpFetch& f = ended.fetch;
    const std::string& path = ended.file.path;
    const int packing = ended.file.packing;
    const bool packed = packing >= 0 && packing < kWebPlain;
    if (f.state == gn::HttpState::done && f.status == 404 && packed) {
      // This packed form is missing: the next form while the address's is not known, else the
      // plain file.
      ask_again(h, ended.file, h.packing < 0 ? packing + 1 : kWebPlain);
      continue;
    }
    if (f.state == gn::HttpState::done && f.status == 200) {
      if (h.packing < 0 && !ended.file.filelist) h.packing = packing;
      // A resume's bytes before, then the rest; the whole checked against the digest told.
      std::vector<uint8_t> sent;
      if (f.offset > 0 && static_cast<size_t>(f.offset) == ended.held.size()) sent = ended.held;
      sent.insert(sent.end(), f.body.begin(), f.body.end());
      std::vector<uint8_t> body;
      std::string bad;
      if (!gn::http_digest_ok(f, sent)) {
        h.said.push_back("[HTTP] " + path + " [checksum mismatch] " + web_files_left(remaining(h)));
        if (!ended.file.filelist && !ended.file.pak) done.back.push_back(path);
        continue;
      }
      if (!unpacked(f, std::move(sent), packing,
                    ended.file.filelist ? kMostList : ge::vfs::kUnpackLimit, body, bad)) {
        std::string said = "[HTTP] " + path;
        said += " [";
        said += bad;
        said += "] ";
        said += web_files_left(remaining(h));
        h.said.push_back(std::move(said));
        continue;
      }
      const double took = std::max(now_s - ended.started_s, 1e-3);
      const auto size = static_cast<int64_t>(body.size());
      const auto moved = static_cast<double>(f.body.size() + static_cast<size_t>(f.offset));
      std::string said = "[HTTP] " + path + " [" + web_size_text(size);
      if (packed) said += std::string(" from ") + kWebPackings[packing].name;
      said += ", " + web_size_text(static_cast<int64_t>(moved / took)) + "/sec] ";
      said += web_files_left(remaining(h));
      h.said.push_back(std::move(said));
      if (ended.file.filelist) {
        if (cfg.filelists) read_list(h, cfg, body, wanted);
      } else {
        done.files.emplace_back(path, std::move(body));
      }
      continue;
    }
    const bool status = f.state == gn::HttpState::done;
    // Cut short: asked again for the rest from what came, a few times.
    if (!status && (f.error == "Transferred a partial file" || f.error == "Timeout was reached") &&
        ended.tries + 1 < kWebTries) {
      std::vector<uint8_t> more;
      if (gn::http_partial(f, more) && f.offset == static_cast<int64_t>(ended.held.size())) {
        WebSlot again;
        again.file = ended.file;
        again.held = std::move(ended.held);
        again.held.insert(again.held.end(), more.begin(), more.end());
        again.tries = ended.tries + 1;
        again.started_s = ended.started_s;
        gn::HttpOptions o = f.options;
        o.from = static_cast<int64_t>(again.held.size());
        o.accept_encoding.clear();  // the rest of the bytes as they lie
        h.said.push_back("[HTTP] " + path + " [" + f.error + ", resuming at " +
                         web_size_text(o.from) + "]");
        gn::http_start(again.fetch, f.url, o, now_s);
        h.slots.push_back(std::move(again));
        continue;
      }
    }
    const std::string why = status ? gn::http_status_text(f.status) : f.error;
    std::string said = "[HTTP] " + path;
    said += " [";
    said += why;
    said += "] ";
    said += web_files_left(remaining(h));
    h.said.push_back(std::move(said));
    const bool missing = status && f.status == 404;
    const bool fatal =
        status ? !missing || (h.default_repo && !ended.file.filelist)
               : why == "Couldn't resolve host name" || why == "Couldn't connect to server" ||
                     why == "Couldn't resolve proxy name" || why.rfind("SSL ", 0) == 0 ||
                     why.rfind("Problem with the SSL", 0) == 0;
    if (fatal) turn_off(h, ended.file, done);
  }
  if (!h.on) return;
  // The queued started into free slots; nothing passes a pack still coming.
  const int most = std::clamp(cfg.max_connections, 1, 4);
  while (!h.queue.empty() && static_cast<int>(h.slots.size()) < most) {
    const bool pak_coming =
        std::any_of(h.slots.begin(), h.slots.end(), [](const WebSlot& s) { return s.file.pak; });
    if (pak_coming || (h.queue.front().pak && !h.slots.empty())) break;
    // While the address's packed form is not known, one file at a time finds it.
    const bool finding = h.packing < 0 && !h.queue.front().filelist;
    if (finding && std::any_of(h.slots.begin(), h.slots.end(),
                               [](const WebSlot& s) { return !s.file.filelist; }))
      break;
    WebSlot s;
    s.file = h.queue.front();
    h.queue.erase(h.queue.begin());
    s.started_s = now_s;
    if (s.file.filelist)
      s.file.packing = kWebPlain;
    else if (s.file.packing < 0)
      s.file.packing = h.packing < 0 ? 0 : h.packing;
    std::string url = h.base + gn::url_escape(s.file.filelist || h.game_in_base
                                                  ? s.file.path
                                                  : gamedir_of(cfg) + "/" + s.file.path);
    if (s.file.packing < kWebPlain) url += kWebPackings[s.file.packing].suffix;
    if (cfg.debug) h.said.push_back("[HTTP] Fetching " + url + "...");
    gn::HttpOptions o;
    o.user_agent = cfg.user_agent;
    o.referer = h.referer;
    o.proxy = cfg.proxy;
    o.verify = !cfg.insecure;
    o.pin = h.pin;
    o.accept_encoding = kAcceptEncoding;
    if (cfg.max_size_mb > 0) o.most_bytes = static_cast<size_t>(cfg.max_size_mb) * 1000000;
    if (s.file.filelist) o.most_bytes = kMostList;
    gn::http_start(s.fetch, url, o, now_s);
    h.slots.push_back(std::move(s));
  }
  // The speed cap shared evenly by the fetches running.
  if (cfg.max_rate_kb > 0 && !h.slots.empty())
    for (WebSlot& s : h.slots)
      s.fetch.options.rate = cfg.max_rate_kb * 1000 / static_cast<int64_t>(h.slots.size());
}

bool web_progress(const WebDownloads& h, WebProgress& out) {
  for (const WebSlot& s : h.slots) {
    if (s.file.filelist) continue;
    out.path = s.file.path;
    // A resume's bytes before count once the server took the range (its 206 said so).
    const size_t before = s.fetch.offset > 0 ? s.held.size() : 0;
    out.got = static_cast<int64_t>(before) + static_cast<int64_t>(gn::http_received(s.fetch));
    out.total = s.fetch.total;
    out.files_left = remaining(h);
    return true;
  }
  return false;
}

void web_stop(WebDownloads& h) {
  for (WebSlot& s : h.slots) gn::http_cancel(s.fetch);
  h.slots.clear();
  h.queue.clear();
}

}  // namespace ge::classic::content
