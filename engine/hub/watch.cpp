// The hub's Watch and Demo pages and the viewer's files, defined (watch.hpp).
//
// ge:summary The Watch, Demo and Play pages around the web viewer, and the viewer's files, defined
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
#include "forward/forward.hpp"
#include "hub/archive.hpp"
#include "hub/content.hpp"
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

// The player: the viewer that also runs a match in the page.
std::string player_name(const HubSpec& spec) {
  return (spec.id.empty() ? std::string("game") : spec.id) + "_play";
}

bool built(const Site& site, const std::string& name) {
  if (site.viewer_dir.empty()) return false;
  std::error_code ec;
  return fs::is_regular_file(fs::path(site.viewer_dir) / (name + ".js"), ec);
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

bool is_pack(std::string_view name) {
  const size_t dot = name.rfind('.');
  const std::string_view ext = dot == std::string_view::npos ? "" : name.substr(dot);
  return ext == ".pak" || ext == ".pk3";
}

// The content packs beside the viewer, by name: the order the viewer mounts them (a later one
// searched first, as a game folder's numbered packs).
std::vector<std::string> viewer_packs(const Site& site) {
  std::vector<std::string> out;
  std::error_code ec;
  for (fs::directory_iterator it(site.viewer_dir, ec), end; !ec && it != end; it.increment(ec)) {
    const std::string name = it->path().filename().string();
    if (it->is_regular_file(ec) && is_pack(name)) out.push_back(name);
  }
  std::sort(out.begin(), out.end());
  return out;
}

// The picture's place: the viewer started with `args`, or the reason there is none.
std::string viewer_slot(const Site& site, const HubSpec& spec, const std::vector<std::string>& args,
                        const std::string& no_viewer, const std::string& content = "",
                        bool play = false) {
  std::string h = "<div class=\"viewer\">";
  if (!no_viewer.empty()) return h + "<p class=\"none\">" + no_viewer + "</p></div>";
  const std::string name = play ? player_name(spec) : viewer_name(spec);
  if (!built(site, name)) {
    return h +
           "<p class=\"none\">This hub does not serve the game's viewer yet, so the picture is "
           "not here. Open it in the game with the button below.</p></div>";
  }
  std::string list;
  for (const std::string& a : args) list += (list.empty() ? "" : ",") + script_str(a);
  if (!content.empty()) {
    list += "," + script_str("--pack=/" + content);  // only what the demo needs (content.hpp)
  } else {
    for (const std::string& pack : viewer_packs(site))
      list += "," + script_str("--pack=/viewer/" + pack);
  }
  // The wasm (the most of a first visit) fetched by the page itself, so the bar follows it; the
  // viewer's script then starts on those bytes. Its size is the file's, before any gzip: what
  // the page counts as the bytes come.
  std::error_code ec;
  const auto wasm_size = fs::file_size(fs::path(site.viewer_dir) / (name + ".wasm"), ec);
  h += "<canvas id=\"view\" width=\"1280\" height=\"720\"></canvas>";
  h += "<progress id=\"viewer-bar\" max=\"1\" value=\"0\"></progress>";
  h += "<p class=\"small\" id=\"viewer-status\">Loading the viewer...</p>";
  h += "<noscript><p class=\"none\">The viewer needs script; open it in the game "
       "instead.</p></noscript>";
  h += "<script>var Module={canvas:document.getElementById(\"view\"),arguments:[" + list +
       "]};</script>";
  h += "<script>(function(){var bar=document.getElementById(\"viewer-bar\"),"
       "line=document.getElementById(\"viewer-status\"),total=" +
       std::to_string(ec ? 0 : wasm_size) +
       ",got=0,parts=[];function mb(n){return (n/1048576).toFixed(1);}"
       "function start(){var s=document.createElement(\"script\");s.src=\"/viewer/" +
       name +
       ".js\";document.body.appendChild(s);}"
       "fetch(\"/viewer/" +
       name +
       ".wasm\").then(function(r){if(!r.ok||!r.body)throw 0;var rd=r.body.getReader();"
       "function step(){return rd.read().then(function(d){if(d.done)return;parts.push(d.value);"
       "got+=d.value.length;if(total){bar.max=total;bar.value=Math.min(got,total);}"
       "line.textContent=\"Loading the viewer... \"+mb(got)+(total?\" of \"+mb(total):\"\")+"
       "\" MB\";return step();});}return step();}).then(function(){var b=new Uint8Array(got),"
       "at=0;parts.forEach(function(p){b.set(p,at);at+=p.length;});Module.wasmBinary=b;"
       "bar.hidden=true;start();},function(){bar.hidden=true;start();});})();</script></div>";
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
  ContentPack pack;
  std::string why;
  const std::string content =
      viewer_built(site, spec) && demo_content(site, spec, r->sha, pack, why)
          ? content_path(pack.sha)
          : "";
  h += viewer_slot(site, spec, {"--demo=/" + archive_path(r->sha)}, "", content);
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

// The maps to play: those the packs beside the viewer hold that the game shows.
std::vector<std::string> play_maps(const HubSpec& spec, const Site& site) {
  std::vector<std::string> maps = viewer_maps(site);
  if (spec.map_shown) std::erase_if(maps, [&](const std::string& m) { return !spec.map_shown(m); });
  return maps;
}

// `/play`: the maps the packs beside the viewer hold, each a Play page.
Answer play_list(const HubSpec& spec, const Site& site) {
  std::string h = head(spec, "Play", false) + nav(spec, "Play") + "<main><h2>Play</h2>";
  const std::vector<std::string> maps = play_maps(spec, site);
  if (!built(site, player_name(spec)) || maps.empty()) {
    h += "<p class=\"none\">This hub does not serve the game to play in a browser yet. Join a "
         "server from <a href=\"/\">the front page</a> in the game instead.</p>";
  } else {
    h += "<p>Play a match against bots here in the page, on any map this hub holds. The game "
         "runs in your browser; only the map's own files are downloaded.</p><ul class=\"maps\">";
    for (const std::string& m : maps)
      h += "<li><a href=\"/play/" + html_escape(m) + "\">" + html_escape(m) + "</a></li>";
    h += "</ul>";
  }
  h += "</main>" + foot();
  return html(200, std::move(h), 3600);
}

// `/play/{map}`: a match on the map in the page, its files in one pack.
Answer play_page(const HubSpec& spec, std::string_view map, const Site& site) {
  const std::vector<std::string> maps = play_maps(spec, site);
  if (!map_name_ok(map) || std::find(maps.begin(), maps.end(), map) == maps.end()) {
    return not_found(spec, "No such map",
                     "The hub holds no map by that name. The maps are on <a "
                     "href=\"/play\">the Play page</a>.",
                     "Play");
  }
  const std::string title = std::string(map);
  std::string h = head(spec, title, false) + nav(spec, "Play") + "<main class=\"watch\">";
  h += "<section class=\"stage\"><h2>" + html_escape(title) + "</h2>";
  h += "<p class=\"meta\"><b>deathmatch</b> · " + html_escape(title) + " · you and " +
       std::to_string(kPlayBots) + " bots</p>";
  ContentPack pack;
  std::string why;
  const std::string content =
      built(site, player_name(spec)) && map_content(site, spec, map, pack, why)
          ? content_path(pack.sha)
          : "";
  h +=
      viewer_slot(site, spec, {"--play=" + std::string(map), "--bots=" + std::to_string(kPlayBots)},
                  "", content, true);
  h += "<p class=\"small\">Click the picture to play: W A S D move, the mouse looks, the left "
       "button fires, SPACE jumps, 1 to 8 or the wheel pick a weapon, TAB shows the scores, "
       "Escape lets the mouse go. On a phone or a tablet the left thumb walks, the right one "
       "looks, and the round buttons at the right fire, jump and change weapon.</p>";
  h += "</section><aside><h3>Other maps</h3><ul class=\"also\">";
  for (const std::string& m : maps)
    if (m != map)
      h += "<li><a href=\"/play/" + html_escape(m) + "\">" + html_escape(m) + "</a></li>";
  h += "</ul></aside></main>" + foot();
  return html(200, std::move(h), 3600);
}

// `/join/{host:port}`: the game server played in the page, through a forwarder that takes pages
// (its WebTransport endpoint, the certificate trusted by the hash its record names), with the
// pack of the map the server is on.
Answer join_page(const Poller& p, const HubSpec& spec, std::string_view rest, const Site& site) {
  gn::Address server;
  const HubNode* n = gn::parse_address(rest, server) ? poller_find(p, server) : nullptr;
  if (n == nullptr || !n->have || n->kind != gn::NodeKind::game) {
    return not_found(spec, "No such server",
                     "The hub has not heard from a server at that address. The servers are on "
                     "<a href=\"/\">the front page</a>.",
                     "");
  }
  const gn::Status& s = n->status;
  const HubNode* fwd = poller_page_forward(p, server);
  std::string no_viewer;
  std::vector<std::string> args;
  if (fwd == nullptr) {
    no_viewer =
        "No forwarder this hub knows takes players from a browser to this server, so it "
        "cannot be played here. Join it in the game below.";
  } else {
    args = {"--join=https://" + host_of(fwd->address) + ":" +
                status_key(fwd->status, ge::forward::kWebTransportKey) +
                "/play?to=" + gn::to_string(server),
            "--cert=" + ge::forward::status_cert(fwd->status)};
  }
  ContentPack pack;
  std::string why;
  const std::string content =
      spec.map_files && built(site, player_name(spec)) && map_content(site, spec, s.map, pack, why)
          ? content_path(pack.sha)
          : "";
  std::string h = head(spec, s.name, false) + nav(spec, "") + "<main class=\"watch\">";
  h += "<section class=\"stage\"><h2>" + html_escape(s.name) + "</h2>";
  h += "<p class=\"meta\"><b>" + html_escape(s.mode.empty() ? "-" : s.mode) + "</b> · " +
       html_escape(s.map) + "</p>";
  h += viewer_slot(site, spec, args, no_viewer, content, true);
  h += "<p class=\"small\">Click the picture to play: W A S D move, the mouse looks, the left "
       "button fires, SPACE jumps, 1 to 8 or the wheel pick a weapon, TAB shows the scores, "
       "Escape lets the mouse go. On a phone or a tablet the left thumb walks, the right one "
       "looks, and the round buttons at the right fire, jump and change weapon.</p>";
  h += "<p class=\"act\"><a class=\"btn\" href=\"" + scheme_of(spec) + "://join/" +
       html_escape(gn::to_string(server)) + "\">Join in the game</a></p>";
  h += "</section><aside><h3>Scoreboard</h3>" + scoreboard_html(spec, s) + "</aside></main>" +
       foot();
  return html(200, std::move(h), kPageRefreshS);
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
                     : is_pack(name)  ? "application/octet-stream"
                                      : nullptr;
  if (!plain || type == nullptr) {
    return refuse(
        "the viewer's files are names of letters, digits, '_', '-' and '.', ending .js, "
        ".wasm, .data, .json, .pak or .pk3");
  }
  if (site.viewer_dir.empty()) {
    return refuse("this hub serves no viewer: start it with --viewer=<dir>, the build's " +
                  viewer_name(spec) + " folder");
  }
  // The build's gzip copy beside the file when the reader takes it (the wasm is a third the size).
  const fs::path file = fs::path(site.viewer_dir) / std::string(name);
  Answer a;
  std::ifstream f;
  if (site.gzip) {
    f.open(file.string() + ".gz", std::ios::binary);
    if (f.is_open()) a.encoding = "gzip";
  }
  if (!f.is_open()) f.open(file, std::ios::binary);
  if (!f.is_open()) return refuse("no " + std::string(name) + " in the hub's viewer folder");
  a.type = type;
  a.body.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  a.max_age_s = kViewerMaxAgeS;
  return a;
}

}  // namespace

std::string watch_path(const gn::Address& node, uint32_t stream) {
  return "/watch/" + html_escape(gn::to_string(node)) + "/" + std::to_string(stream);
}

bool viewer_built(const Site& site, const HubSpec& spec) { return built(site, viewer_name(spec)); }

bool player_built(const Site& site, const HubSpec& spec) { return built(site, player_name(spec)); }

bool watch_answer(const Poller& p, const HubSpec& spec, std::string_view target, int64_t now_us,
                  const Site& site, Answer& out) {
  std::string path = target_path(target);
  while (path.size() > 1 && path.back() == '/') path.pop_back();
  if (content_answer(path, site, out, &spec)) return true;
  if (spec.map_files && path == "/play") {
    out = play_list(spec, site);
    return true;
  }
  const std::pair<std::string_view, int> kinds[] = {
      {"/watch/", 0}, {"/demo/", 1}, {"/viewer/", 2}, {"/play/", 3}, {"/join/", 4}};
  for (const auto& [prefix, kind] : kinds) {
    if (path.rfind(prefix, 0) != 0) continue;
    if (kind == 3 && !spec.map_files) continue;
    const std::string_view rest = std::string_view(path).substr(prefix.size());
    out = kind == 0   ? watch_page(p, spec, rest, now_us, site)
          : kind == 1 ? demo_page(spec, rest, site)
          : kind == 2 ? viewer_file(spec, rest, site)
          : kind == 3 ? play_page(spec, rest, site)
                      : join_page(p, spec, rest, site);
    return true;
  }
  return false;
}

}  // namespace ge::hub
