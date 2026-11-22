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

void web_connect(WebDownloads& h, const WebConfig& cfg, const std::string& dlserver, int all,
                 const std::string& server) {
  web_stop(h);
  h = {};
  if (all < 0 || !cfg.downloads) return;
  std::string url = dlserver;
  h.default_repo = url.empty();
  if (url.empty()) url = cfg.default_url;
  if (url.empty()) return;
  url = web_base(url);
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
    if (f.state == gn::HttpState::done && f.status == 200) {
      const double took = std::max(now_s - ended.started_s, 1e-3);
      const auto size = static_cast<int64_t>(f.body.size());
      h.said.push_back("[HTTP] " + path + " [" + web_size_text(size) + ", " +
                       web_size_text(static_cast<int64_t>(static_cast<double>(size) / took)) +
                       "/sec] " + web_files_left(remaining(h)));
      if (ended.file.filelist) {
        if (cfg.filelists) read_list(h, cfg, f.body, wanted);
      } else {
        done.files.emplace_back(path, f.body);
      }
      continue;
    }
    const bool status = f.state == gn::HttpState::done;
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
    WebSlot s;
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
    o.verify = !cfg.insecure;
    if (s.file.filelist) o.most_bytes = kMostList;
    gn::http_start(s.fetch, url, o, now_s);
    h.slots.push_back(std::move(s));
  }
}

void web_stop(WebDownloads& h) {
  for (WebSlot& s : h.slots) gn::http_cancel(s.fetch);
  h.slots.clear();
  h.queue.clear();
}

}  // namespace ge::classic::content
