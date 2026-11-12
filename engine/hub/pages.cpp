// The hub's pages, defined: the routes, the HTML of each page, the stylesheet.
//
// ge:summary The hub's pages defined: routing, servers' cards, players, scoreboard, stylesheet
// ge:keywords hub pages html route card scoreboard players stylesheet escape
// ge:skill net
// ge:test tests/hub_pages_test.cpp
#include "hub/pages.hpp"

#include <algorithm>
#include <cstdio>
#include <utility>
#include <vector>

namespace ge::hub {

namespace gn = ge::net;

namespace {

const char* phase_word(gn::StatusPhase p) {
  switch (p) {
    case gn::StatusPhase::lobby:
      return "waiting";
    case gn::StatusPhase::countdown:
      return "starting";
    case gn::StatusPhase::playing:
      return "playing";
    case gn::StatusPhase::intermission:
      return "scores";
    case gn::StatusPhase::unknown:
      break;
  }
  return "";
}

std::string clock(int32_t seconds) {
  if (seconds < 0) return "";
  char out[16];
  std::snprintf(out, sizeof out, "%d:%02d", seconds / 60, seconds % 60);
  return out;
}

std::string head(const HubSpec& spec, const std::string& title, bool refresh) {
  std::string h =
      "<!doctype html>\n<html lang=\"en\"><head><meta charset=\"utf-8\">"
      "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
  if (refresh)
    h += "<meta http-equiv=\"refresh\" content=\"" + std::to_string(kPageRefreshS) + "\">";
  h += "<title>" + html_escape(title) + " · " + html_escape(spec.game) + "</title>";
  h += "<link rel=\"stylesheet\" href=\"/style.css\"></head>\n<body>";
  return h;
}

std::string nav(const HubSpec& spec, const char* on) {
  auto link = [&](const char* href, const char* word) {
    return std::string("<a href=\"") + href + "\"" +
           (std::string(word) == on ? " class=\"on\"" : "") + ">" + word + "</a>";
  };
  return "<header><a class=\"brand\" href=\"/\">" + html_escape(spec.game) + "</a><nav>" +
         link("/", "Servers") + link("/players", "Players") + link("/v1", "API") +
         "</nav></header>\n";
}

std::string foot() {
  return "<footer>Read-only and public. The same data as JSON: <a href=\"/v1/servers\">/v1/servers"
         "</a>.</footer>\n</body></html>\n";
}

struct Counts {
  int people = 0, bots = 0, spectators = 0, viewers = 0;
};

Counts counts(const gn::Status& s) {
  Counts c;
  for (const gn::StatusPlayer& p : s.players) {
    if (p.flags & gn::kPlayerViewer) {
      c.viewers++;
    } else if (p.flags & gn::kPlayerSpectator) {
      c.spectators++;
    } else {
      c.people++;
      c.bots += (p.flags & gn::kPlayerBot) ? 1 : 0;
    }
  }
  for (const gn::StatusStream& st : s.streams) c.viewers += st.viewers;
  return c;
}

std::vector<const gn::StatusPlayer*> ranked(const HubSpec& spec, const gn::Status& s) {
  std::vector<const gn::StatusPlayer*> v;
  for (const gn::StatusPlayer& p : s.players) {
    if ((p.flags & (gn::kPlayerSpectator | gn::kPlayerViewer)) == 0) v.push_back(&p);
  }
  std::stable_sort(v.begin(), v.end(), [&](const gn::StatusPlayer* a, const gn::StatusPlayer* b) {
    return spec.score_low_first ? a->score < b->score : a->score > b->score;
  });
  return v;
}

std::string colour_dot(uint32_t rgb) {
  char out[64];
  std::snprintf(out, sizeof out, "<i class=\"dot\" style=\"background:#%06x\"></i>",
                rgb & 0xffffffu);
  return out;
}

// The scoreboard: the teams' lines, then the players' rows.
std::string scoreboard(const HubSpec& spec, const gn::Status& s) {
  std::string h = "<table class=\"score\">";
  if (!s.teams.empty()) {
    for (const gn::StatusTeam& t : s.teams) {
      h += "<tr class=\"team\"><td>" + colour_dot(t.colour) + html_escape(t.name) + "</td><td>" +
           std::to_string(t.score) + "</td><td></td></tr>";
    }
  }
  h += "<tr class=\"cols\"><th>name</th><th>" + html_escape(spec.score) + "</th><th>ping</th></tr>";
  for (const gn::StatusPlayer* p : ranked(spec, s)) {
    std::string name = html_escape(p->name);
    if (!p->team.empty()) name += " <small>" + html_escape(p->team) + "</small>";
    if (p->flags & gn::kPlayerBot) name += " <small class=\"bot\">bot</small>";
    h += "<tr><td>" + name + "</td><td>" + std::to_string(p->score) + "</td><td>" +
         (p->flags & gn::kPlayerBot ? std::string("-") : std::to_string(p->ping_ms)) + "</td></tr>";
  }
  return h + "</table>";
}

std::string card(const HubSpec& spec, const HubNode& n) {
  const gn::Status& s = n.status;
  const Counts c = counts(s);
  const std::string addr = gn::to_string(n.address);
  std::string h = "<article class=\"card phase-" + std::string(phase_word(s.phase)) + "\">";
  h += "<div class=\"map\"><span class=\"mapname\">" + html_escape(s.map) +
       "</span><span class=\"phase\">" + phase_word(s.phase) +
       (s.time_left_s >= 0 ? " " + clock(s.time_left_s) : std::string()) + "</span></div>";
  h += "<h2>" + html_escape(s.name) + (s.locked ? " <small>locked</small>" : "") + "</h2>";
  h += "<p class=\"meta\"><b>" + html_escape(s.mode.empty() ? "-" : s.mode) + "</b> · " +
       std::to_string(c.people - c.bots) + " / " + std::to_string(s.max_players) + " playing";
  if (c.bots) h += " · " + std::to_string(c.bots) + " bots";
  if (s.country[0]) h += " · " + html_escape(s.country);
  h += "</p>";
  h += scoreboard(spec, s);
  if (c.spectators || c.viewers) {
    h += "<p class=\"watchers\">" + std::to_string(c.spectators) + " spectating · " +
         std::to_string(c.viewers) + " watching</p>";
  }
  const std::string scheme = spec.id.empty() ? "game" : spec.id;
  h += "<p class=\"act\"><a class=\"btn\" href=\"" + scheme + "://join/" + html_escape(addr) +
       "\">Join</a><a class=\"btn\" href=\"" + scheme + "://spectate/" + html_escape(addr) +
       "\">Spectate</a>";
  for (const gn::StatusStream& st : s.streams) {
    h += "<a class=\"btn watch\" href=\"" + scheme + "://tv/" +
         html_escape(gn::to_string(st.node)) + "/" + std::to_string(st.stream) + "\">Watch</a>";
    break;
  }
  h += "<a class=\"small\" href=\"/scoreboard/" + html_escape(addr) + "\">overlay</a></p>";
  h += "<p class=\"addr\"><code>" + html_escape(addr) + "</code></p></article>\n";
  return h;
}

Answer html(int code, std::string body, int max_age_s) {
  Answer a;
  a.code = code;
  a.type = "text/html";
  a.body = std::move(body);
  a.max_age_s = max_age_s;
  return a;
}

}  // namespace

std::string html_escape(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    switch (c) {
      case '&':
        out += "&amp;";
        break;
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      case '"':
        out += "&quot;";
        break;
      case '\'':
        out += "&#39;";
        break;
      default:
        out += c;
    }
  }
  return out;
}

std::string servers_page(const Poller& p, const HubSpec& spec, std::string_view mode,
                         int64_t now_us) {
  std::vector<std::pair<int64_t, const HubNode*>> live, empty;
  int people = 0, broadcasts = 0;
  for (const HubNode& n : p.nodes) {
    if (!n.have) continue;
    if (n.kind == gn::NodeKind::tv) broadcasts++;
    if (n.kind != gn::NodeKind::game) continue;
    if (!mode.empty() && n.status.mode != mode) continue;
    const Counts c = counts(n.status);
    people += c.people - c.bots;
    (c.people > 0 || c.spectators > 0 ? live : empty).emplace_back(interest(spec, n, now_us), &n);
  }
  auto order = [](const auto& a, const auto& b) {
    if (a.first != b.first) return a.first > b.first;
    return gn::address_hash(a.second->address) < gn::address_hash(b.second->address);
  };
  std::sort(live.begin(), live.end(), order);
  std::sort(empty.begin(), empty.end(), order);

  std::string h = head(spec, "Servers", true) + nav(spec, "Servers") + "<main>";
  h += "<div class=\"filters\"><a href=\"/\"" + std::string(mode.empty() ? " class=\"on\"" : "") +
       ">All</a>";
  for (const HubMode& m : spec.modes) {
    h += "<a href=\"/?mode=" + html_escape(m.name) + "\"" +
         (m.name == mode ? " class=\"on\"" : "") + ">" + html_escape(m.name) + "</a>";
  }
  h += "</div>";
  h += "<p class=\"sum\">" + std::to_string(live.size() + empty.size()) + " servers · " +
       std::to_string(people) + " playing · " + std::to_string(broadcasts) + " broadcasts</p>";
  h += "<section class=\"cards\">\n";
  for (const auto& r : live) h += card(spec, *r.second);
  h += "</section>";
  if (live.empty()) h += "<p class=\"none\">Nobody is playing right now.</p>";
  if (!empty.empty()) {
    h += "<h3>Empty servers</h3><ul class=\"empty\">";
    for (const auto& r : empty) {
      const gn::Status& s = r.second->status;
      h += "<li><b>" + html_escape(s.name) + "</b> " + html_escape(s.mode) + " · " +
           html_escape(s.map) + " · <code>" + html_escape(gn::to_string(r.second->address)) +
           "</code></li>";
    }
    h += "</ul>";
  }
  return h + "</main>" + foot();
}

std::string players_page(const Poller& p, const HubSpec& spec, int64_t now_us) {
  struct Row {
    const gn::StatusPlayer* player;
    const HubNode* node;
  };
  std::vector<Row> rows;
  for (const HubNode& n : p.nodes) {
    if (!n.have || n.kind != gn::NodeKind::game) continue;
    for (const gn::StatusPlayer& pl : n.status.players) rows.push_back({&pl, &n});
  }
  std::stable_sort(rows.begin(), rows.end(), [&](const Row& a, const Row& b) {
    const bool ab = a.player->flags & gn::kPlayerBot, bb = b.player->flags & gn::kPlayerBot;
    if (ab != bb) return bb;  // people first
    const int64_t ia = interest(spec, *a.node, now_us), ib = interest(spec, *b.node, now_us);
    if (ia != ib) return ia > ib;
    return a.player->score > b.player->score;
  });
  std::string h = head(spec, "Players", true) + nav(spec, "Players") + "<main>";
  h += "<p class=\"sum\">" + std::to_string(rows.size()) + " on " +
       std::to_string(poller_count(p, gn::kind_bit(gn::NodeKind::game))) + " servers</p>";
  h += "<table class=\"players\"><tr><th>name</th><th>doing</th><th>server</th><th>mode</th><th>" +
       html_escape(spec.score) + "</th></tr>";
  for (const Row& r : rows) {
    const uint8_t f = r.player->flags;
    const char* doing = (f & gn::kPlayerBot)         ? "bot"
                        : (f & gn::kPlayerViewer)    ? "watching"
                        : (f & gn::kPlayerSpectator) ? "spectating"
                                                     : "playing";
    h += "<tr><td>" + html_escape(r.player->name) + "</td><td class=\"" + doing + "\">" + doing +
         "</td><td><a href=\"/scoreboard/" + html_escape(gn::to_string(r.node->address)) + "\">" +
         html_escape(r.node->status.name) + "</a></td><td>" + html_escape(r.node->status.mode) +
         "</td><td>" + std::to_string(r.player->score) + "</td></tr>";
  }
  h += "</table>";
  if (rows.empty()) h += "<p class=\"none\">Nobody is on a server right now.</p>";
  return h + "</main>" + foot();
}

std::string scoreboard_page(const HubSpec& spec, const HubNode& n) {
  const gn::Status& s = n.status;
  std::string h = head(spec, s.name, true) + "<main class=\"overlay\">";
  h += "<h2>" + html_escape(s.name) + "</h2><p class=\"meta\">" + html_escape(s.mode) + " · " +
       html_escape(s.map) + " · " + phase_word(s.phase) +
       (s.time_left_s >= 0 ? " " + clock(s.time_left_s) : std::string()) + "</p>";
  h += scoreboard(spec, s);
  return h + "</main></body></html>\n";
}

const std::string& hub_stylesheet() {
  static const std::string css =
      R"css(:root{--bg:#101513;--panel:#18201d;--panel2:#1f2925;--ink:#e8f1ec;--dim:#8fa59c;
--accent:#3fc79a;--hot:#ffb44d;--line:#2b3833;color-scheme:dark}
@media (prefers-color-scheme:light){:root{--bg:#f4f6f5;--panel:#fff;--panel2:#eef2f0;--ink:#16201c;
--dim:#5d6e67;--accent:#138a63;--hot:#b8690a;--line:#d5ddd9;color-scheme:light}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);
font:15px/1.4 system-ui,-apple-system,"Segoe UI",sans-serif}
a{color:var(--accent);text-decoration:none}header{display:flex;gap:16px;align-items:center;
padding:12px 16px;border-bottom:1px solid var(--line);flex-wrap:wrap}
.brand{font-weight:700;font-size:18px;color:var(--ink)}nav{display:flex;gap:12px}
nav a{color:var(--dim)}nav a.on{color:var(--ink);font-weight:600}
main{max-width:1280px;margin:0 auto;padding:16px}
.filters{display:flex;gap:8px;flex-wrap:wrap}.filters a{padding:4px 12px;border-radius:999px;
background:var(--panel2);color:var(--dim)}.filters a.on{background:var(--accent);color:var(--bg)}
.sum{color:var(--dim)}.cards{display:grid;gap:16px;grid-template-columns:repeat(auto-fill,minmax(300px,1fr))}
.card{background:var(--panel);border:1px solid var(--line);border-radius:12px;overflow:hidden}
.map{display:flex;justify-content:space-between;padding:28px 14px 10px;
background:linear-gradient(135deg,#2c4a3f,#1b2a40);color:#fff;font-weight:600}
.phase-scores .map{background:linear-gradient(135deg,#4a3f2c,#2a1b30)}
.phase-waiting .map,.phase-starting .map{background:linear-gradient(135deg,#2c3f4a,#20283a)}
.card h2{margin:10px 14px 0;font-size:18px}.card h2 small{color:var(--hot);font-size:12px}
.meta{margin:4px 14px;color:var(--dim)}.meta b{color:var(--ink)}
table.score{width:calc(100% - 28px);margin:8px 14px;border-collapse:collapse;font-size:14px}
table.score td,table.score th{padding:3px 4px;text-align:left}table.score td+td,table.score th+th{text-align:right}
table.score th{color:var(--dim);font-weight:500;border-bottom:1px solid var(--line)}
tr.team td{font-weight:700}small{color:var(--dim)}small.bot{color:var(--hot)}
.dot{display:inline-block;width:9px;height:9px;border-radius:50%;margin-right:6px}
.watchers{margin:4px 14px;color:var(--dim);font-size:13px}
.act{display:flex;gap:8px;align-items:center;margin:10px 14px;flex-wrap:wrap}
.btn{padding:6px 12px;border-radius:8px;background:var(--panel2);color:var(--ink);border:1px solid var(--line)}
.btn.watch{background:var(--accent);color:var(--bg);border-color:var(--accent)}.act .small{margin-left:auto;font-size:13px}
.addr{margin:0 14px 12px;color:var(--dim);font-size:13px}ul.empty{color:var(--dim);padding-left:18px}
table.players{width:100%;border-collapse:collapse}table.players td,table.players th{padding:6px 8px;
text-align:left;border-bottom:1px solid var(--line)}table.players th{color:var(--dim);font-weight:500}
td.bot{color:var(--hot)}td.spectating,td.watching{color:var(--dim)}
.overlay{max-width:420px;background:var(--panel);border-radius:12px;margin:16px}
footer{color:var(--dim);font-size:13px;padding:24px 16px;text-align:center}.none{color:var(--dim)}
@media (max-width:480px){main{padding:12px}.cards{grid-template-columns:1fr}}
)css";
  return css;
}

Answer hub_answer(const Poller& p, const HubSpec& spec, std::string_view target, int64_t now_us) {
  std::string path = target_path(target);
  if (path == "/v1" || path.rfind("/v1/", 0) == 0) return api_answer(p, spec, target, now_us);
  while (path.size() > 1 && path.back() == '/') path.pop_back();
  if (path == "/" || path.empty()) {
    return html(200, servers_page(p, spec, query_param(target, "mode"), now_us), kPageRefreshS);
  }
  if (path == "/players") return html(200, players_page(p, spec, now_us), kPageRefreshS);
  if (path == "/style.css") {
    Answer a = html(200, hub_stylesheet(), 3600);
    a.type = "text/css";
    return a;
  }
  const std::string kBoard = "/scoreboard/";
  if (path.rfind(kBoard, 0) == 0) {
    gn::Address a;
    const HubNode* n =
        gn::parse_address(path.substr(kBoard.size()), a) ? poller_find(p, a) : nullptr;
    if (n && n->have) return html(200, scoreboard_page(spec, *n), kPageRefreshS);
  }
  return html(404,
              head(spec, "Not found", false) + nav(spec, "") +
                  "<main><h2>Not found</h2><p>Nothing is at " + html_escape(path) +
                  ". The servers are on <a href=\"/\">the front page</a>.</p></main>" + foot(),
              0);
}

}  // namespace ge::hub
