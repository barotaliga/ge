// The hub's one stylesheet (`/style.css`): every page's look, light or dark as the reader's system
// asks, in the game's look (`HubSpec::look` on the page's root: the engine's own by default,
// `arena` or `alpine`). A look is only colour tokens; the shapes are the same for every game, so a
// new look is one more block of tokens here. Pages carry no script for it: a phone, a browser
// without script and a search engine read the same page.
//
// ge:summary The hub's stylesheet: tokens per look and theme, header, cards, map banners, tables
// ge:keywords hub stylesheet css look theme dark light arena alpine map-banner live-badge teambar
// ge:keywords phone-width tokens
// ge:skill net
// ge:test tests/hub_pages_test.cpp
#include "hub/pages.hpp"

namespace ge::hub {

const std::string& hub_stylesheet() {
  // Tokens: the engine's look (slate and teal), then each game look; light first, dark when the
  // reader's system asks. A look's own block comes after the engine's, so it wins in both themes.
  static const std::string css =
      R"css(:root{--bg:#f2f4f4;--panel:#fcfdfd;--panel2:#e9eded;--ink:#141a1b;--dim:#5e6b6d;
--line:#d6dddd;--accent:#0e8a74;--accent-ink:#fff;--hot:#a85c0c;--live:#d92f2a;--banner-l:36%;
--glow:color-mix(in srgb,var(--accent) 11%,transparent);
--display:"Bahnschrift","DIN Alternate","DIN Condensed","Barlow Condensed","Roboto Condensed",
"Arial Narrow",system-ui,sans-serif;--text:system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;
--mono:ui-monospace,"SF Mono",Menlo,Consolas,monospace;--radius:14px;
--shadow:0 1px 0 rgba(0,0,0,.04),0 8px 24px -12px rgba(0,0,0,.18);color-scheme:light}
@media (prefers-color-scheme:dark){:root{--bg:#0f1314;--panel:#161c1d;--panel2:#1f2728;--ink:#e6eeee;
--dim:#93a3a5;--line:#283234;--accent:#3fd0b1;--accent-ink:#04201a;--hot:#ffb45c;--live:#ff4d47;
--banner-l:27%;--shadow:0 1px 0 rgba(255,255,255,.03),0 10px 30px -14px rgba(0,0,0,.7);
color-scheme:dark}}
:root[data-look=arena]{--bg:#f3f1ee;--panel:#fffdfa;--panel2:#f0ece6;--ink:#1d1a17;--dim:#6f665d;
--line:#ddd6cc;--accent:#d9561c;--accent-ink:#fff;--hot:#b4480f}
@media (prefers-color-scheme:dark){:root[data-look=arena]{--bg:#121110;--panel:#1c1a18;
--panel2:#262320;--ink:#f1ece6;--dim:#a1968a;--line:#2f2b27;--accent:#ff7a2e;--accent-ink:#1a0d05;
--hot:#ffb067}}
:root[data-look=alpine]{--bg:#eef3f7;--panel:#fbfdff;--panel2:#e6eef5;--ink:#0f1d2a;--dim:#5a6f82;
--line:#d3dee8;--accent:#0b7fc7;--accent-ink:#fff;--hot:#a3520b}
@media (prefers-color-scheme:dark){:root[data-look=alpine]{--bg:#0a121b;--panel:#111d29;
--panel2:#182839;--ink:#e9f3fb;--dim:#8ea4b8;--line:#1f3245;--accent:#5cc8ff;--accent-ink:#04121d;
--hot:#ffc06b}}
*{box-sizing:border-box}
body{margin:0;min-height:100vh;background:var(--bg);color:var(--ink);font:15px/1.45 var(--text);
-webkit-font-smoothing:antialiased;
background-image:radial-gradient(1200px 420px at 8% -12%,var(--glow),transparent 70%)}
a{color:var(--accent);text-decoration:none}a:hover{text-decoration:underline}
h1,h2,h3{font-family:var(--display);letter-spacing:.01em;text-wrap:balance}
main h2{font-size:22px;margin:4px 0 12px}main h3{font-size:15px;text-transform:uppercase;
letter-spacing:.1em;color:var(--dim);margin:26px 0 10px}
small{color:var(--dim)}small.bot,td.bot{color:var(--hot)}.none{color:var(--dim)}.warn{color:var(--hot)}
/* the bar */
header{position:sticky;top:0;z-index:5;display:flex;gap:10px 22px;align-items:center;flex-wrap:wrap;
padding:12px max(16px,calc(50vw - 624px));border-bottom:1px solid var(--line);
background:color-mix(in srgb,var(--bg) 84%,transparent);-webkit-backdrop-filter:blur(10px);
backdrop-filter:blur(10px)}
.brand{display:flex;align-items:center;gap:10px;font:700 21px/1 var(--display);letter-spacing:.02em;
color:var(--ink);text-transform:uppercase;white-space:nowrap}.brand:hover{text-decoration:none}
.brand::before{content:"";width:26px;height:26px;flex:none;border-radius:50%;
background:linear-gradient(135deg,var(--accent),color-mix(in srgb,var(--accent) 45%,#000))}
:root[data-look=arena] .brand::before{border-radius:0;
clip-path:polygon(0 100%,38% 0,62% 0,100% 100%,72% 100%,50% 45%,28% 100%)}
:root[data-look=alpine] .brand::before{border-radius:0;
clip-path:polygon(0 100%,34% 22%,48% 46%,64% 8%,100% 100%)}
nav{display:flex;gap:4px;flex-wrap:wrap}
nav a{color:var(--dim);padding:6px 11px;border-radius:999px;font-weight:600}
nav a:hover{color:var(--ink);background:var(--panel2);text-decoration:none}
nav a.on{color:var(--ink);background:var(--panel2)}
main{max-width:1280px;margin:0 auto;padding:22px 16px}
footer{color:var(--dim);font-size:13px;padding:36px 16px;text-align:center}
/* the filters and the sums */
.filters{display:flex;gap:6px;flex-wrap:wrap;margin-bottom:16px}
.filters a{padding:6px 14px;border-radius:999px;background:var(--panel);border:1px solid var(--line);
color:var(--dim);font:600 13px/1.2 var(--display);text-transform:uppercase;letter-spacing:.08em}
.filters a:hover{color:var(--ink);text-decoration:none}
.filters a.on{background:var(--accent);border-color:var(--accent);color:var(--accent-ink)}
.sum{color:var(--dim);margin:0 0 18px;font-variant-numeric:tabular-nums;display:flex;flex-wrap:wrap;
gap:4px 18px;align-items:baseline}
.sum b{color:var(--ink);font:700 28px/1 var(--display);margin-right:5px}
.sum .live{display:inline-flex;align-items:baseline;gap:6px}
.sum .live::before,.phase.live::before{content:"";width:8px;height:8px;border-radius:50%;flex:none;
background:var(--live);align-self:center;animation:pulse 1.8s infinite}
@keyframes pulse{0%{box-shadow:0 0 0 0 color-mix(in srgb,var(--live) 60%,transparent)}
70%{box-shadow:0 0 0 8px transparent}100%{box-shadow:0 0 0 0 transparent}}
@media (prefers-reduced-motion:reduce){.sum .live::before,.phase.live::before{animation:none}
.card{transition:none}}
/* the servers' cards */
.cards{display:grid;gap:18px;grid-template-columns:repeat(auto-fill,minmax(290px,1fr));align-items:start}
.card{background:var(--panel);border:1px solid var(--line);border-radius:var(--radius);overflow:hidden;
box-shadow:var(--shadow);transition:transform .15s ease}.card:hover{transform:translateY(-2px)}
.map{--h:160;position:relative;display:flex;justify-content:space-between;align-items:flex-end;gap:8px;
min-height:96px;padding:14px;color:#fff;background:
radial-gradient(120% 140% at 85% 0%,hsl(calc(var(--h) + 40) 70% 55% / .5),transparent 60%),
linear-gradient(160deg,hsl(var(--h) 42% var(--banner-l)),hsl(calc(var(--h) + 50) 46% calc(var(--banner-l) - 12%)))}
:root[data-look=arena] .map{background:
repeating-linear-gradient(115deg,rgba(255,255,255,.06) 0 2px,transparent 2px 14px),
radial-gradient(120% 140% at 85% 0%,hsl(calc(var(--h) + 40) 70% 55% / .55),transparent 60%),
linear-gradient(160deg,hsl(var(--h) 45% var(--banner-l)),hsl(calc(var(--h) + 60) 50% calc(var(--banner-l) - 12%)))}
:root[data-look=alpine] .map{background:
repeating-radial-gradient(ellipse at 30% 130%,rgba(255,255,255,.11) 0 1px,transparent 1px 12px),
linear-gradient(160deg,hsl(var(--h) 55% var(--banner-l)),hsl(calc(var(--h) + 25) 45% calc(var(--banner-l) - 14%)))}
.phase-scores .map,.phase-waiting .map{filter:saturate(.55)}
.mapname{font:700 24px/1 var(--display);text-transform:uppercase;letter-spacing:.04em;
text-shadow:0 2px 12px rgba(0,0,0,.35);overflow-wrap:anywhere}
.phase{display:inline-flex;align-items:center;gap:7px;padding:4px 10px;border-radius:999px;
background:rgba(0,0,0,.4);font:600 12px/1 var(--display);text-transform:uppercase;letter-spacing:.08em;
white-space:nowrap;font-variant-numeric:tabular-nums}
.card h2{margin:14px 16px 0;font-size:20px;line-height:1.15}.card h2 small{color:var(--hot);font-size:12px}
.meta{margin:4px 16px 0;color:var(--dim);font-size:14px}
.meta b{color:var(--ink);text-transform:uppercase;font:700 13px var(--display);letter-spacing:.06em}
.teambar{display:flex;height:6px;margin:12px 16px 0;border-radius:3px;overflow:hidden;gap:2px}
.teambar i{display:block;min-width:4px}
table.score{width:calc(100% - 32px);margin:10px 16px 4px;border-collapse:collapse;font-size:14px;
font-variant-numeric:tabular-nums}
table.score td,table.score th{padding:4px;text-align:left}
table.score td+td,table.score th+th{text-align:right}
table.score th,table.num th,table.players th,table.demos th{color:var(--dim);font:600 11px var(--display);
text-transform:uppercase;letter-spacing:.1em;border-bottom:1px solid var(--line);padding-bottom:6px}
table.score tr:nth-child(even):not(.team) td{background:color-mix(in srgb,var(--panel2) 60%,transparent)}
table.score td:first-child{border-radius:6px 0 0 6px}table.score td:last-child{border-radius:0 6px 6px 0}
tr.team td{font:700 17px var(--display);padding:5px 4px}
.dot{display:inline-block;width:10px;height:10px;border-radius:3px;margin-right:8px;vertical-align:-1px}
.watchers{margin:6px 16px;color:var(--dim);font-size:13px}
.act{display:flex;gap:8px;align-items:center;margin:12px 16px;flex-wrap:wrap}
.btn,form.search button,.acct button{display:inline-block;padding:8px 14px;border-radius:10px;
background:var(--panel2);color:var(--ink);border:1px solid var(--line);font:600 13px/1 var(--display);
text-transform:uppercase;letter-spacing:.08em;cursor:pointer;white-space:nowrap}
.btn:hover{text-decoration:none;border-color:var(--accent);color:var(--accent)}
.btn.watch,.btn.play,form.search button,.acct button{background:var(--accent);color:var(--accent-ink);
border-color:var(--accent);box-shadow:0 6px 18px -8px var(--accent)}
.btn.watch:hover,.btn.play:hover,form.search button:hover,.acct button:hover{filter:brightness(1.08);
color:var(--accent-ink)}
.act .small{margin-left:auto;font-size:13px}
.addr{margin:0 16px 14px;color:var(--dim);font:12px var(--mono);overflow-wrap:anywhere}
ul.empty code{font:12px var(--mono);opacity:.8}
ul.empty{padding:0;list-style:none;display:grid;gap:8px;
grid-template-columns:repeat(auto-fill,minmax(260px,1fr))}
ul.empty li{background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:10px 14px;
color:var(--dim)}
.overlay{max-width:420px;background:var(--panel);border:1px solid var(--line);border-radius:var(--radius);
margin:16px;overflow:hidden;box-shadow:var(--shadow)}.overlay table.score{margin-bottom:12px}
/* the lists: players, demos, numbers */
table.players,table.demos{width:100%;border-collapse:separate;border-spacing:0;background:var(--panel);
border:1px solid var(--line);border-radius:var(--radius);overflow:hidden;box-shadow:var(--shadow)}
table.players td,table.players th,table.demos td,table.demos th{padding:9px 12px;text-align:left;
border-bottom:1px solid var(--line)}table.players th,table.demos th{padding-top:12px}
table.players tr:last-child td,table.demos tr:last-child td{border-bottom:0}
table.players td,table.demos td{font-variant-numeric:tabular-nums}
table.players tr:hover td,table.demos tr:hover td{background:color-mix(in srgb,var(--panel2) 55%,transparent)}
td.playing{color:var(--accent);font-weight:600}td.spectating,td.watching{color:var(--dim)}
table.demos td{vertical-align:top}table.demos .act{margin:0;flex-wrap:nowrap}
details{color:var(--dim);font-size:13px}summary{cursor:pointer}
.tag{font:600 11px/1.6 var(--display);text-transform:uppercase;letter-spacing:.08em;padding:1px 8px;
border-radius:999px;background:color-mix(in srgb,var(--hot) 14%,transparent);color:var(--hot)}
.pager{display:flex;gap:16px;margin:14px 0}
form.search{display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin-bottom:14px}
form.search input,form.search select,.acct input{font:inherit;padding:8px 12px;border-radius:10px;
border:1px solid var(--line);background:var(--panel);color:var(--ink);min-width:0}
form.search input:focus,form.search select:focus,.acct input:focus{outline:2px solid
color-mix(in srgb,var(--accent) 55%,transparent);outline-offset:1px;border-color:var(--accent)}
label.without{color:var(--dim);font-size:14px;align-self:center}
main.stats h2,main.watch h2,main.radar h2{font-size:32px;line-height:1.05;margin:6px 0 6px}
main.stats h3{margin:26px 0 10px}main.stats form.search{margin-top:12px}main.acct h2{font-size:30px}main.watch .meta{margin:0 0 12px}.tiles{display:flex;flex-wrap:wrap;gap:10px;margin:12px 0}
.tile{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:12px 16px;
min-width:108px;display:grid;gap:2px;box-shadow:var(--shadow)}
.tile b{font:700 28px/1 var(--display);font-variant-numeric:tabular-nums}
.tile small{text-transform:uppercase;letter-spacing:.08em;font-size:11px}
.wide{overflow-x:auto;max-width:100%}
table.num{border-collapse:separate;border-spacing:0;display:block;width:max-content;max-width:100%;
overflow-x:auto;font-variant-numeric:tabular-nums;background:var(--panel);border:1px solid var(--line);
border-radius:12px;box-shadow:var(--shadow)}.wide table.num{max-width:none}
table.num tr:last-child td{border-bottom:0}table.num th{padding-top:10px}
table.num td,table.num th{padding:6px 12px;text-align:right;border-bottom:1px solid var(--line);white-space:nowrap}
table.num td:first-child,table.num th:first-child{text-align:left}
table.num tr:nth-child(even) td{background:color-mix(in srgb,var(--panel2) 55%,transparent)}
table.rank td:nth-child(2),table.rank th:nth-child(2){text-align:left}
table.rank tr:nth-child(2) td:first-child,table.rank tr:nth-child(3) td:first-child,
table.rank tr:nth-child(4) td:first-child{color:var(--accent);font:700 16px var(--display)}
article.result{margin:14px 0;background:var(--panel);border:1px solid var(--line);
border-radius:var(--radius);padding:4px 16px 12px}article.result h3 small{color:var(--dim)}
tr.unranked td{color:var(--dim)}table.champions td{text-align:left}
/* watching and playing */
main.watch{display:grid;grid-template-columns:minmax(0,3fr) minmax(220px,1fr);gap:22px}
main.watch h3{margin:8px 0}
.viewer{background:#000;aspect-ratio:16/9;max-width:100%;border-radius:var(--radius);display:flex;
align-items:center;justify-content:center;overflow:hidden;position:relative;
box-shadow:0 18px 50px -20px rgba(0,0,0,.6),0 0 0 1px var(--line)}
.viewer canvas{width:100%;height:100%;display:block}
.viewer .none{color:#c8c8c8;padding:16px;text-align:center;max-width:36em}
#viewer-status{position:absolute;left:0;right:0;bottom:0;margin:0;padding:8px 12px;color:#e8e8e8;
background:linear-gradient(transparent,rgba(0,0,0,.75));font-size:13px}#viewer-status:empty{display:none}
#viewer-bar{position:absolute;left:12px;right:12px;bottom:44px;width:calc(100% - 24px);height:6px;
appearance:none;border:0;border-radius:3px;background:#333}
#viewer-bar::-webkit-progress-bar{background:#333;border-radius:3px}
#viewer-bar::-webkit-progress-value{background:var(--accent);border-radius:3px}
#viewer-bar::-moz-progress-bar{background:var(--accent);border-radius:3px}
main.watch aside{background:var(--panel);border:1px solid var(--line);border-radius:var(--radius);
padding:14px 16px;align-self:start;box-shadow:var(--shadow)}
main.watch aside table.score{width:100%;margin:6px 0}
ul.also{list-style:none;padding:0;margin:0;display:grid;gap:2px}
ul.also li{padding:6px 8px;border-radius:8px}ul.also li:hover{background:var(--panel2)}
ul.maps{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:12px;list-style:none;
padding:0}ul.maps a{display:flex;align-items:flex-end;min-height:84px;padding:12px;border-radius:12px;
color:#fff;font:700 18px/1 var(--display);text-transform:uppercase;letter-spacing:.04em;
text-shadow:0 2px 10px rgba(0,0,0,.35);background:linear-gradient(160deg,
color-mix(in srgb,var(--accent) 70%,#000),color-mix(in srgb,var(--accent) 30%,#000))}
ul.maps a:hover{text-decoration:none;filter:brightness(1.1)}
/* the radar */
.radar figure{margin:12px 0}svg.radar{width:100%;max-width:760px;height:auto;display:block;
border-radius:var(--radius);background:var(--panel);border:1px solid var(--line)}
svg.radar .ground{fill:var(--panel)}svg.radar .outline{stroke:var(--dim)}
svg.radar .who{font:600 22px var(--text);fill:var(--ink);paint-order:stroke;stroke:var(--bg);stroke-width:4px}
ul.legend{display:flex;flex-wrap:wrap;gap:6px 16px;list-style:none;padding:0}
.swatch{display:inline-block;width:12px;height:12px;border-radius:50%;margin-right:6px}
/* the account */
main.acct{max-width:640px}.acct form{display:grid;gap:12px;max-width:420px;margin:10px 0 22px;
background:var(--panel);border:1px solid var(--line);border-radius:var(--radius);padding:18px;
box-shadow:var(--shadow)}.acct label{display:grid;gap:5px;color:var(--dim);font-size:14px}
.acct button{justify-self:start}.acct :disabled{opacity:.5;cursor:not-allowed}
.acct .code{font:700 34px var(--mono);letter-spacing:.14em;background:var(--panel);display:inline-block;
padding:10px 18px;border-radius:12px;border:1px dashed var(--accent);color:var(--accent)}
/* a phone */
@media (max-width:700px){main.watch{grid-template-columns:1fr}}
@media (max-width:480px){main{padding:16px}header{gap:8px 14px}.brand{font-size:18px}
nav{flex-wrap:nowrap;overflow-x:auto;max-width:100%;scrollbar-width:none}nav a{padding:5px 9px}
.cards{grid-template-columns:1fr}.sum b{font-size:24px}
table.demos th:nth-child(3),table.demos td:nth-child(3),table.demos th:nth-child(5),
table.demos td:nth-child(5){display:none}table.demos .act{flex-direction:column;align-items:flex-start}
table.players td,table.players th,table.demos td,table.demos th{padding:8px}}
)css";
  return css;
}

}  // namespace ge::hub
