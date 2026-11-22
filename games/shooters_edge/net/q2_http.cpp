// Quake II's downloads over HTTP (q2_http.hpp).
//
// ge:summary Quake II's HTTP downloads: the address, the queue, the file lists, the lines said
// ge:keywords q2-http http-download dlserver filelist
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_http_test.cpp
#include "shooters_edge/net/q2_http.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <iterator>
#include <utility>

namespace shooters_edge::net {

namespace gn = ge::net;

namespace {

constexpr size_t kMostUrl = 512;
constexpr size_t kMostList = 1024 * 1024 - 1;  // a file list's bytes

std::string lowered(std::string s) {
  for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return s;
}

std::string gamedir_of(const Q2HttpConfig& cfg) {
  return cfg.gamedir.empty() ? std::string("baseq2") : cfg.gamedir;
}

std::string files_left(int n) {
  return "[" + std::to_string(n) + " remaining file" + (n == 1 ? "]" : "s]");
}

int remaining(const Q2Http& h) { return static_cast<int>(h.queue.size() + h.slots.size()); }

bool name_char(char ch) {
  return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_' || ch == '-';
}

// Queues `f` once a connection: a pack after the packs queued, ahead of the rest.
void push(Q2Http& h, const Q2HttpFile& f) {
  if (!h.asked.insert(f.path).second) return;
  if (!f.pak) {
    h.queue.push_back(f);
    return;
  }
  auto at =
      std::find_if(h.queue.begin(), h.queue.end(), [](const Q2HttpFile& q) { return !q.pak; });
  h.queue.insert(at, f);
}

// HTTP off for this connection: every file not fetched yet handed back to the channel.
void turn_off(Q2Http& h, const Q2HttpFile& failed, Q2HttpDone& done) {
  h.on = false;
  if (!failed.filelist && !failed.pak) done.back.push_back(failed.path);
  for (Q2HttpSlot& s : h.slots) {
    gn::http_cancel(s.fetch);
    if (!s.file.filelist && !s.file.pak) done.back.push_back(s.file.path);
  }
  h.slots.clear();
  for (const Q2HttpFile& f : h.queue)
    if (!f.filelist && !f.pak) done.back.push_back(f.path);
  h.queue.clear();
}

// A file list's lines queued: the kinds a server sends, the packs said and put first.
void read_list(Q2Http& h, const std::vector<uint8_t>& body,
               const std::function<bool(const std::string&, bool)>& wanted) {
  static constexpr const char* kKinds[] = {"bsp", "dm2", "ent", "jpg", "loc", "md2", "md3", "ogg",
                                           "pcx", "png", "sp2", "tga", "txt", "wal", "wav"};
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
      h.said.push_back("[HTTP] Filelist is requesting a ." + kind + " file '" + line + "'");
    } else if (std::none_of(std::begin(kKinds), std::end(kKinds),
                            [&kind](const char* k) { return kind == k; })) {
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

std::string q2_size_text(int64_t bytes) {
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

std::string q2_http_dlserver(const std::vector<std::string>& words) {
  for (const std::string& w : words)
    if (w.rfind("dlserver=", 0) == 0) return w.substr(9);
  return {};
}

void q2_http_connect(Q2Http& h, const Q2HttpConfig& cfg, const std::string& dlserver, int all,
                     const std::string& server) {
  q2_http_stop(h);
  h = {};
  if (all < 0 || !cfg.downloads) return;
  std::string url = dlserver;
  h.default_repo = url.empty();
  if (url.empty()) url = cfg.default_url;
  if (url.empty()) return;
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
  h.base = url;
  h.referer = "quake2://" + server;
  h.said.push_back("[HTTP] Download server at " + url);
}

bool q2_http_queue(Q2Http& h, const Q2HttpConfig& cfg, const std::string& path) {
  if (!h.on) return false;
  bool tell = false;
  if (!h.told_server) {
    h.told_server = true;
    tell = !h.default_repo;
  }
  const std::string dir = gamedir_of(cfg);
  if (cfg.filelists && !h.asked_list) {
    h.asked_list = true;
    push(h, {.path = dir + ".filelist", .filelist = true, .pak = false});
  }
  push(h, {.path = path, .filelist = false, .pak = false});
  if (cfg.filelists && path.size() > 4 && lowered(path.substr(path.size() - 4)) == ".bsp")
    push(h, {.path = dir + "/" + path.substr(0, path.size() - 4) + ".filelist",
             .filelist = true,
             .pak = false});
  return tell;
}

bool q2_http_busy(const Q2Http& h) { return !h.queue.empty() || !h.slots.empty(); }

void q2_http_step(Q2Http& h, const Q2HttpConfig& cfg, double now_s,
                  const std::function<bool(const std::string& path, bool game_only)>& wanted,
                  Q2HttpDone& done) {
  // The fetches that ended.
  for (size_t i = 0; i < h.slots.size() && h.on;) {
    Q2HttpSlot& s = h.slots[i];
    if (!gn::http_step(s.fetch, now_s)) {
      ++i;
      continue;
    }
    Q2HttpSlot ended = std::move(s);
    h.slots.erase(h.slots.begin() + static_cast<ptrdiff_t>(i));
    const gn::HttpFetch& f = ended.fetch;
    const std::string& path = ended.file.path;
    if (f.state == gn::HttpState::done && f.status == 200) {
      const double took = std::max(now_s - ended.started_s, 1e-3);
      const auto size = static_cast<int64_t>(f.body.size());
      h.said.push_back("[HTTP] " + path + " [" + q2_size_text(size) + ", " +
                       q2_size_text(static_cast<int64_t>(static_cast<double>(size) / took)) +
                       "/sec] " + files_left(remaining(h)));
      if (ended.file.filelist) {
        if (cfg.filelists) read_list(h, f.body, wanted);
      } else {
        done.files.emplace_back(path, f.body);
      }
      continue;
    }
    const bool status = f.state == gn::HttpState::done;
    const std::string why = status ? gn::http_status_text(f.status) : f.error;
    h.said.push_back("[HTTP] " + path + " [" + why + "] " + files_left(remaining(h)));
    const bool missing = status && f.status == 404;
    const bool fatal =
        status ? !missing || (h.default_repo && !ended.file.filelist)
               : why == "Couldn't resolve host name" || why == "Couldn't connect to server" ||
                     why == "Couldn't resolve proxy name" || why == "Unsupported protocol";
    if (fatal) turn_off(h, ended.file, done);
  }
  if (!h.on) return;
  // The queued started into free slots; nothing passes a pack still coming.
  const int most = std::clamp(cfg.max_connections, 1, 4);
  while (!h.queue.empty() && static_cast<int>(h.slots.size()) < most) {
    const bool pak_coming =
        std::any_of(h.slots.begin(), h.slots.end(), [](const Q2HttpSlot& s) { return s.file.pak; });
    if (pak_coming || (h.queue.front().pak && !h.slots.empty())) break;
    Q2HttpSlot s;
    s.file = h.queue.front();
    h.queue.erase(h.queue.begin());
    s.started_s = now_s;
    const std::string url =
        h.base +
        gn::url_escape(s.file.filelist ? s.file.path : gamedir_of(cfg) + "/" + s.file.path);
    if (cfg.debug) h.said.push_back("[HTTP] Fetching " + url + "...");
    gn::HttpOptions o;
    o.user_agent = cfg.user_agent;
    o.referer = h.referer;
    o.proxy = cfg.proxy;
    if (s.file.filelist) o.most_bytes = kMostList;
    gn::http_start(s.fetch, url, o, now_s);
    h.slots.push_back(std::move(s));
  }
}

void q2_http_stop(Q2Http& h) {
  for (Q2HttpSlot& s : h.slots) gn::http_cancel(s.fetch);
  h.slots.clear();
  h.queue.clear();
}

}  // namespace shooters_edge::net
