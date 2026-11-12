// The hub's Watch and Demo pages and the viewer's files, defined (watch.hpp).
//
// ge:summary The Watch and Demo pages around the web viewer, and the viewer's files, defined
// ge:keywords hub watch demo web-viewer canvas loader viewer-files mime
// ge:skill net
// ge:test tests/hub_watch_test.cpp
#include "hub/watch.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <utility>
#include <vector>

#include "core/json_write.hpp"
#include "hub/archive.hpp"
#include "hub/link.hpp"
#include "hub/page_parts.hpp"

namespace ge::hub {

namespace gn = ge::net;
namespace fs = std::filesystem;

namespace {

constexpr size_t kOtherBroadcasts = 6;

std::string viewer_name(const HubSpec& spec) {
  return (spec.id.empty() ? std::string("game") : spec.id) + "_viewer";
}

// A JSON string safe inside a <script>: '<' written as its escape, so no text closes the tag.
std::string script_str(std::string_view s) {
  const std::string j = core::json_str(s);
  std::string out;
  out.reserve(j.size());
  for (const char c : j) {
    if (c == '<') {
      out += "\\u003c";
    } else {
      out += c;
    }
  }
  return out;
}

// The address's host alone: "1.2.3.4", "[::1]".
std::string host_of(const gn::Address& a) {
  std::string t = gn::to_string(a);
  const size_t colon = t.rfind(':');
  if (colon != std::string::npos) t.resize(colon);
  return t;
}

std::string status_key(const gn::Status& s, std::string_view key) {
  for (const auto& [k, v] : s.keys)
    if (k == key) return v;
  return {};
}

// The picture's place: the viewer started with `args`, or the reason there is none.
std::string viewer_slot(const Site& site, const HubSpec& spec, const std::vector<std::string>& args,
                        const std::string& no_viewer) {
  std::string h = "<div class=\"viewer\">";
  if (!no_viewer.empty()) return h + "<p class=\"none\">" + no_viewer + "</p></div>";
  if (!viewer_built(site, spec)) {
    return h +
           "<p class=\"none\">This hub does not serve the game's viewer yet, so the picture is "
           "not here. Open it in the game with the button below.</p></div>";
  }
  std::string list;
  for (const std::string& a : args) list += (list.empty() ? "" : ",") + script_str(a);
  h += "<canvas id=\"view\" width=\"1280\" height=\"720\"></canvas>";
  h += "<noscript><p class=\"none\">The viewer needs script; open it in the game "
       "instead.</p></noscript>";
  h += "<script>var Module={canvas:document.getElementById(\"view\"),arguments:[" + list +
       "]};</script>";
  h += "<script src=\"/viewer/" + viewer_name(spec) + ".js\" async></script></div>";
  return h;
}

std::string scheme_of(const HubSpec& spec) { return spec.id.empty() ? "game" : spec.id; }

Answer not_found(const HubSpec& spec, const char* title, const std::string& why, const char* on) {
  return html(404,
              head(spec, title, false) + nav(spec, on) + "<main><h2>" + title + "</h2><p>" + why +
                  "</p></main>" + foot(),
              0);
}

Answer watch_page(const Poller& p, const HubSpec& spec, std::string_view rest, int64_t now_us,
                  const Site& site) {
  // {host:port}/{stream}
  const size_t slash = rest.rfind('/');
  gn::Address node;
  uint32_t stream = 0;
  bool ok = slash != std::string_view::npos && slash + 1 < rest.size() &&
            gn::parse_address(rest.substr(0, slash), node);
  if (ok) {
    for (const char c : rest.substr(slash + 1)) {
      if (c < '0' || c > '9' || stream > 100000000) ok = false;
      stream = stream * 10 + static_cast<uint32_t>(c - '0');
    }
  }
  if (!ok) {
    return not_found(spec, "No broadcast",
                     "A Watch page is /watch/&lt;node address&gt;/&lt;stream&gt;. The live "
                     "broadcasts are on <a href=\"/\">the front page</a>.",
                     "");
  }
  // The game server whose record lists this stream, and the other live broadcasts.
  const HubNode* source = nullptr;
  std::vector<std::pair<int64_t, std::pair<const HubNode*, const gn::StatusStream*>>> others;
  for (const HubNode& n : p.nodes) {
    if (!n.have) continue;
    for (const gn::StatusStream& st : n.status.streams) {
      if (st.node == node && st.stream == stream) {
        source = &n;
      } else {
        others.push_back({interest(spec, n, now_us), {&n, &st}});
      }
      break;  // a server's first stream is the one its card shows
    }
  }
  std::stable_sort(others.begin(), others.end(),
                   [](const auto& a, const auto& b) { return a.first > b.first; });
  const HubNode* tv = poller_find(p, node);
  const std::string web = tv && tv->have ? status_key(tv->status, kWebPortKey) : std::string();
  std::string no_viewer;
  if (!tv || !tv->have) {
    no_viewer =
        "The hub has not heard from this broadcast's node, so it cannot show the "
        "picture here.";
  } else if (web.empty()) {
    no_viewer = "This broadcast takes no viewers in a browser; open it in the game below.";
  }
  const std::string title = source ? source->status.name : "Broadcast";
  std::string h = head(spec, title, false) + nav(spec, "") + "<main class=\"watch\">";
  h += "<section class=\"stage\"><h2>" + html_escape(title) + "</h2>";
  if (source) {
    const gn::Status& s = source->status;
    h += "<p class=\"meta\"><b>" + html_escape(s.mode.empty() ? "-" : s.mode) + "</b> · " +
         html_escape(s.map) + "</p>";
  }
  h += viewer_slot(
      site, spec,
      {"--tv=ws://" + host_of(node) + ":" + web + "/", "--stream=" + std::to_string(stream)},
      no_viewer);
  h += "<p class=\"act\"><a class=\"btn watch\" href=\"" + scheme_of(spec) + "://tv/" +
       html_escape(gn::to_string(node)) + "/" + std::to_string(stream) + "\">Open in the game</a>";
  if (tv && tv->have) {
    h += "<span class=\"small\">" + status_key(tv->status, "viewers") + " watching · " +
         status_key(tv->status, "delay_s") + " s behind</span>";
  }
  h += "</p></section><aside>";
  if (source) h += "<h3>Scoreboard</h3>" + scoreboard_html(spec, source->status);
  if (!others.empty()) {
    h += "<h3>Also live</h3><ul class=\"also\">";
    for (size_t i = 0; i < others.size() && i < kOtherBroadcasts; i++) {
      const auto& [n, st] = others[i].second;
      h += "<li><a href=\"" + watch_path(st->node, st->stream) + "\">" +
           html_escape(n->status.name) + "</a> <small>" + html_escape(n->status.mode) + " · " +
           html_escape(n->status.map) + "</small></li>";
    }
    h += "</ul>";
  }
  h += "</aside></main>" + foot();
  return html(200, std::move(h), kPageRefreshS);
}

Answer demo_page(const HubSpec& spec, std::string_view sha, const Site& site) {
  const DemoRow* r = site.archive ? archive_find(*site.archive, std::string(sha)) : nullptr;
  if (!r) {
    return not_found(spec, "No demo",
                     "The hub keeps no demo with that hash. The demos are on <a "
                     "href=\"/demos\">the Demos page</a>.",
                     "Demos");
  }
  const std::string title = r->title.empty() ? r->map : r->title;
  std::string h = head(spec, title, false) + nav(spec, "Demos") + "<main class=\"watch\">";
  h += "<section class=\"stage\"><h2>" + html_escape(title) + "</h2>";
  h += "<p class=\"meta\"><b>" + html_escape(r->mode.empty() ? "-" : r->mode) + "</b> · " +
       html_escape(r->map) + " · " + utc_text(r->start_s);
  if (!r->server.empty()) h += " · " + html_escape(r->server);
  h += "</p>";
  h += viewer_slot(site, spec, {"--demo=/" + archive_path(r->sha)}, "");
  h += "<p class=\"act\"><a class=\"btn watch\" href=\"" + scheme_of(spec) + "://demo/" + r->sha;
  if (link_host(site.host)) h += "?hub=" + site.host;
  h += "\">Play in the game</a><a class=\"small\" href=\"/" + archive_path(r->sha) +
       "\" download>Download</a>";
  if (r->radar) h += "<a class=\"small\" href=\"/radar/" + r->sha + "\">Map</a>";
  h += "</p></section><aside><h3>Players</h3><ul class=\"also\">";
  for (const DemoPlayer& pl : r->players) {
    h += "<li>" + html_escape(pl.name);
    if (!pl.team.empty()) h += " <small>" + html_escape(pl.team) + "</small>";
    h += "</li>";
  }
  h += "</ul></aside></main>" + foot();
  return html(200, std::move(h), 3600);
}

Answer viewer_file(const HubSpec& spec, std::string_view name, const Site& site) {
  const auto refuse = [&](const std::string& why) {
    Answer a;
    a.code = 404;
    a.type = "text/plain; charset=utf-8";
    a.body = why + "\n";
    return a;
  };
  const size_t dot = name.rfind('.');
  const std::string ext = dot == std::string_view::npos ? "" : std::string(name.substr(dot));
  const bool plain = !name.empty() && name.find("..") == std::string_view::npos &&
                     std::all_of(name.begin(), name.end(), [](char c) {
                       return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
                     });
  const char* type = ext == ".js"     ? "text/javascript"
                     : ext == ".wasm" ? "application/wasm"
                     : ext == ".data" ? "application/octet-stream"
                     : ext == ".json" ? "application/json"
                                      : nullptr;
  if (!plain || type == nullptr) {
    return refuse(
        "the viewer's files are names of letters, digits, '_', '-' and '.', ending .js, "
        ".wasm, .data or .json");
  }
  if (site.viewer_dir.empty()) {
    return refuse("this hub serves no viewer: start it with --viewer=<dir>, the build's " +
                  viewer_name(spec) + " folder");
  }
  std::ifstream f(fs::path(site.viewer_dir) / std::string(name), std::ios::binary);
  if (!f) return refuse("no " + std::string(name) + " in the hub's viewer folder");
  Answer a;
  a.type = type;
  a.body.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  a.max_age_s = kViewerMaxAgeS;
  return a;
}

}  // namespace

std::string watch_path(const gn::Address& node, uint32_t stream) {
  return "/watch/" + html_escape(gn::to_string(node)) + "/" + std::to_string(stream);
}

bool viewer_built(const Site& site, const HubSpec& spec) {
  if (site.viewer_dir.empty()) return false;
  std::error_code ec;
  return fs::is_regular_file(fs::path(site.viewer_dir) / (viewer_name(spec) + ".js"), ec);
}

bool watch_answer(const Poller& p, const HubSpec& spec, std::string_view target, int64_t now_us,
                  const Site& site, Answer& out) {
  std::string path = target_path(target);
  while (path.size() > 1 && path.back() == '/') path.pop_back();
  const std::pair<std::string_view, int> kinds[] = {{"/watch/", 0}, {"/demo/", 1}, {"/viewer/", 2}};
  for (const auto& [prefix, kind] : kinds) {
    if (path.rfind(prefix, 0) != 0) continue;
    const std::string_view rest = std::string_view(path).substr(prefix.size());
    out = kind == 0   ? watch_page(p, spec, rest, now_us, site)
          : kind == 1 ? demo_page(spec, rest, site)
                      : viewer_file(spec, rest, site);
    return true;
  }
  return false;
}

}  // namespace ge::hub
