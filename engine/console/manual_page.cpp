// The reference manual's page, defined: the merge into rows, the JSON, the HTML with its search.
//
// ge:summary The reference manual's page defined: manual_rows, manual_json, manual_html
// ge:keywords manual manual_rows manual_json manual_html merge search filter page html
// ge:skill console
// ge:test tests/console_manual_test.cpp
#include "console/manual_page.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string_view>

namespace ge::console {

namespace {

std::vector<std::string> split_commas(std::string_view s) {
  std::vector<std::string> out;
  while (!s.empty()) {
    const size_t c = s.find(',');
    if (c != 0) out.emplace_back(s.substr(0, c));
    if (c == std::string_view::npos) break;
    s.remove_prefix(c + 1);
  }
  return out;
}

void add_all(std::vector<std::string>& to, const std::vector<std::string>& from) {
  for (const std::string& s : from)
    if (std::find(to.begin(), to.end(), s) == to.end()) to.push_back(s);
}

bool is_game_layer(std::string_view layer) { return layer.starts_with("game:"); }

// What a pass says a name is: compared to find the passes that agree.
struct Def {
  std::string layer, type, fallback, min, max, flags, help, values, overlap, arg;
  bool operator==(const Def&) const = default;
};

Def def_of(const ManualEntry& e) {
  return {e.layer,    e.type,
          e.fallback, e.min,
          e.max,      e.flags,
          e.help,     e.values,
          e.overlap,  e.kind == ManualKind::flag ? e.target : std::string()};
}

struct Group {
  ManualKind kind;
  std::string name;
  std::vector<std::string> programs;
  // Each pass's definition: (program, the pass's games) -> its def.
  std::vector<std::pair<std::vector<std::string>, Def>> passes;
};

std::string json_text(std::string_view s) {
  std::string out = "\"";
  for (const char ch : s) {
    const auto c = static_cast<unsigned char>(ch);
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\t':
        out += "\\t";
        break;
      case '<':
        out += "\\u003c";  // never a closing script tag inside the page
        break;
      default:
        if (c < 0x20) {
          constexpr char kHex[] = "0123456789abcdef";
          out += "\\u00";
          out += kHex[c >> 4];
          out += kHex[c & 15];
        } else {
          out += ch;
        }
    }
  }
  return out + '"';
}

std::string json_list(const std::vector<std::string>& v) {
  std::string out = "[";
  for (size_t i = 0; i < v.size(); ++i) out += (i ? "," : "") + json_text(v[i]);
  return out + ']';
}

// A field written only when it says something.
void field(std::string& out, std::string_view key, std::string_view value) {
  if (value.empty()) return;
  out += ',';
  out += json_text(key);
  out += ':';
  out += json_text(value);
}

std::string html_text(std::string_view s) {
  std::string out;
  for (const char c : s) {
    if (c == '<')
      out += "&lt;";
    else if (c == '>')
      out += "&gt;";
    else if (c == '&')
      out += "&amp;";
    else if (c == '"')
      out += "&quot;";
    else
      out += c;
  }
  return out;
}

}  // namespace

std::vector<ManualRow> manual_rows(const ManualSource& source) {
  // Every pass of each program, to tell "in every pass" from "in some".
  std::map<std::string, std::set<std::string>> program_passes;
  for (const ManualEntry& e : source.entries) program_passes[e.program].insert(e.games);

  std::map<std::pair<int, std::string>, Group> groups;
  const auto key = [](const ManualEntry& e) {
    // A flag is its program's own: `-game` in two programs may differ.
    const std::string name = e.kind == ManualKind::flag ? e.program + ' ' + e.name : e.name;
    return std::make_pair(static_cast<int>(e.kind), name);
  };
  for (const ManualEntry& e : source.entries) {
    if (e.kind == ManualKind::setting_synonym || e.kind == ManualKind::command_synonym) continue;
    Group& g = groups[key(e)];
    g.kind = e.kind;
    g.name = e.name;
    add_all(g.programs, {e.program});
    g.passes.emplace_back(split_commas(e.games), def_of(e));
  }
  // Which (program, pass) sets each group was seen in: for "every pass".
  std::map<std::pair<int, std::string>, std::map<std::string, std::set<std::string>>> seen;
  for (const ManualEntry& e : source.entries)
    if (e.kind != ManualKind::setting_synonym && e.kind != ManualKind::command_synonym)
      seen[key(e)][e.program].insert(e.games);

  std::vector<ManualRow> rows;
  std::map<std::pair<int, std::string>, size_t> at;
  for (auto& [k, g] : groups) {
    ManualRow r;
    r.kind = g.kind;
    r.name = g.name;
    r.programs = g.programs;
    // The common definition: a pass whose own layer did not meet the name (no overlap, not a
    // game's layer); else the first pass's.
    const Def* common = nullptr;
    for (const auto& [games, d] : g.passes)
      if (d.overlap.empty() && !is_game_layer(d.layer)) {
        common = &d;
        break;
      }
    const bool shared = common != nullptr;
    if (!common) common = &g.passes.front().second;
    r.layer = common->layer;
    r.type = common->type;
    r.fallback = common->fallback;
    r.min = common->min;
    r.max = common->max;
    r.flags = common->flags;
    r.help = common->help;
    r.values = common->values;
    r.arg = common->arg;
    // A game's layer met it: the layer it is from is the one under the game's.
    for (const auto& [games, d] : g.passes)
      if (!shared && !d.overlap.empty()) {
        const size_t colon = d.overlap.find(':');
        if (colon != std::string::npos) r.layer = d.overlap.substr(colon + 1);
      }
    // A name only games' own layers have, alike in each but for the layer (a server's `map` in
    // QuakeWorld's and in Quake II's): one row, its layer "a game's own".
    const auto but_layer = [](Def d, const Def& like) {
      d.layer = like.layer;
      return d == like;
    };
    if (!shared && is_game_layer(common->layer))
      for (const auto& [games, d] : g.passes)
        if (d.layer != common->layer && is_game_layer(d.layer) && but_layer(d, *common))
          r.layer = "game";
    // Every pass whose definition differs from the common one is a game's own.
    for (const auto& [games, d] : g.passes) {
      if (d == *common && shared) continue;
      if (!shared && &d == common && d.overlap.empty()) continue;
      if (!shared && r.layer == "game" && but_layer(d, *common)) continue;
      const std::string how = d.overlap.substr(0, d.overlap.find(':'));
      ManualOwn* own = nullptr;
      for (ManualOwn& o : r.own)
        if (o.overlap == how && o.layer == d.layer && o.fallback == d.fallback &&
            o.help == d.help && o.values == d.values)
          own = &o;
      if (!own) {
        own = &r.own.emplace_back();
        own->overlap = how;
        own->layer = d.layer;
        own->fallback = d.fallback;
        own->min = d.min;
        own->max = d.max;
        own->help = d.help;
        own->values = d.values;
      }
      add_all(own->games, games);
    }
    // The games it is in: none listed when every pass of each of its programs has it.
    bool everywhere = true;
    for (const auto& [program, passes] : seen[k])
      if (passes.size() != program_passes[program].size()) everywhere = false;
    if (!everywhere)
      for (const auto& [games, d] : g.passes) add_all(r.games, games);
    at[k] = rows.size();
    rows.push_back(std::move(r));
  }
  // The spellings: each synonym on its target's row, else a row of its own.
  for (const ManualEntry& e : source.entries) {
    if (e.kind != ManualKind::setting_synonym && e.kind != ManualKind::command_synonym) continue;
    const ManualKind target_kind =
        e.kind == ManualKind::setting_synonym ? ManualKind::setting : ManualKind::command;
    std::string target = e.target, part;
    if (const size_t b = target.find('['); b != std::string::npos) {
      part = target.substr(b + 1, target.size() - b - 2);
      target.resize(b);
    }
    const auto it = at.find({static_cast<int>(target_kind), target});
    if (it == at.end()) {
      ManualRow r;
      r.kind = target_kind;
      r.name = e.name;
      r.programs = {e.program};
      r.games = split_commas(e.games);
      r.layer = e.layer;
      r.help = e.help;
      rows.push_back(std::move(r));
      continue;
    }
    ManualRow& r = rows[it->second];
    add_all(r.programs, {e.program});
    ManualSpelling* sp = nullptr;
    for (ManualSpelling& s : r.spellings)
      if (s.name == e.name && s.part == part) sp = &s;
    if (!sp) {
      sp = &r.spellings.emplace_back();
      sp->name = e.name;
      sp->part = part;
    }
    add_all(sp->games, split_commas(e.games));
  }
  std::sort(rows.begin(), rows.end(), [](const ManualRow& a, const ManualRow& b) {
    if (a.kind != b.kind) return a.kind < b.kind;
    return a.name < b.name;
  });
  return rows;
}

std::string manual_json(const ManualSource& source, const std::vector<ManualRow>& rows,
                        const ManualPage& page) {
  std::string out = "{\"title\":" + json_text(page.title);
  field(out, "intro", page.intro);
  field(out, "version", page.version);
  out += ",\"programs\":[";
  for (size_t i = 0; i < source.programs.size(); ++i)
    out += std::string(i ? "," : "") + "{\"id\":" + json_text(source.programs[i].id) +
           ",\"name\":" + json_text(source.programs[i].name) + '}';
  out += "],\"games\":[";
  std::set<std::string> games_seen;
  bool first = true;
  for (const ManualName& g : source.games) {
    if (!games_seen.insert(g.id).second) continue;
    out += std::string(first ? "" : ",") + "{\"id\":" + json_text(g.id) +
           ",\"name\":" + json_text(g.name) + ",\"takes_in\":" + json_list(g.takes_in) + '}';
    first = false;
  }
  out += "],\"rows\":[";
  for (size_t i = 0; i < rows.size(); ++i) {
    const ManualRow& r = rows[i];
    out += i ? ",\n{" : "\n{";
    out += "\"k\":" + json_text(kind_word(r.kind)) + ",\"n\":" + json_text(r.name);
    out += ",\"p\":" + json_list(r.programs);
    if (!r.games.empty()) out += ",\"g\":" + json_list(r.games);
    field(out, "l", r.layer);
    field(out, "t", r.type);
    if (r.kind == ManualKind::setting) {
      out += ",\"d\":" + json_text(r.fallback);
    }
    field(out, "mn", r.min);
    field(out, "mx", r.max);
    field(out, "f", r.flags);
    field(out, "h", r.help);
    field(out, "v", r.values);
    field(out, "arg", r.arg);
    if (!r.own.empty()) {
      out += ",\"o\":[";
      for (size_t j = 0; j < r.own.size(); ++j) {
        const ManualOwn& o = r.own[j];
        out += std::string(j ? "," : "") + "{\"g\":" + json_list(o.games);
        field(out, "ov", o.overlap);
        field(out, "l", o.layer);
        if (r.kind == ManualKind::setting) out += ",\"d\":" + json_text(o.fallback);
        field(out, "mn", o.min);
        field(out, "mx", o.max);
        field(out, "h", o.help);
        field(out, "v", o.values);
        out += '}';
      }
      out += ']';
    }
    if (!r.spellings.empty()) {
      out += ",\"a\":[";
      for (size_t j = 0; j < r.spellings.size(); ++j) {
        const ManualSpelling& s = r.spellings[j];
        out += std::string(j ? "," : "") + "{\"n\":" + json_text(s.name) +
               ",\"g\":" + json_list(s.games);
        field(out, "part", s.part);
        out += '}';
      }
      out += ']';
    }
    out += '}';
  }
  return out + "\n]}\n";
}

namespace {

// The page's look: light and dark by the reader's system, one column on a phone.
constexpr std::string_view kStyle = R"CSS(
:root{--bg:#fbfaf7;--fg:#1d1c1a;--muted:#6b675f;--line:#e4e0d6;--card:#ffffff;--accent:#9a3412;
--chip:#f1eee6;--chip-on:#1d1c1a;--chip-on-fg:#fbfaf7;--code:#f4f1ea;--own:#7c2d12;--same:#14532d;
--mono:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;
--sans:system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
@media (prefers-color-scheme:dark){:root{--bg:#141311;--fg:#ece8df;--muted:#9c968a;--line:#2c2a26;
--card:#1b1a17;--accent:#fb923c;--chip:#24221e;--chip-on:#ece8df;--chip-on-fg:#141311;--code:#24221e;
--own:#fdba74;--same:#86efac}}
*{box-sizing:border-box}html{background:var(--bg)}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.5 var(--sans)}
header{position:sticky;top:0;z-index:2;background:var(--bg);border-bottom:1px solid var(--line);
padding:16px 16px 10px}
.wrap{max-width:1100px;margin:0 auto}
h1{font-size:22px;margin:0 0 2px;letter-spacing:-.01em}
.intro{color:var(--muted);margin:0 0 10px}
#q{width:100%;font:16px var(--sans);padding:10px 12px;border:1px solid var(--line);border-radius:8px;
background:var(--card);color:var(--fg)}
#q:focus{outline:2px solid var(--accent);outline-offset:-1px}
.filters{display:flex;flex-wrap:wrap;gap:6px 14px;margin-top:10px;align-items:center}
.group{display:flex;flex-wrap:wrap;gap:4px;align-items:center}
.group b{font-size:12px;color:var(--muted);font-weight:600;text-transform:uppercase;
letter-spacing:.04em;margin-right:2px}
.chip{border:1px solid var(--line);background:var(--chip);color:var(--fg);border-radius:999px;
padding:3px 10px;font:13px var(--sans);cursor:pointer}
.chip[aria-pressed=true]{background:var(--chip-on);color:var(--chip-on-fg);border-color:var(--chip-on)}
.count{color:var(--muted);font-size:13px;margin-left:auto}
main{padding:8px 16px 40px}
.row{border-bottom:1px solid var(--line);padding:12px 2px;scroll-margin-top:190px}
.row:target{background:var(--code);border-radius:8px;padding:12px 10px}
.head{display:flex;flex-wrap:wrap;gap:6px 10px;align-items:baseline}
.name{font:600 15px var(--mono);color:var(--fg);text-decoration:none}
.name:hover{color:var(--accent)}
.arg{font:14px var(--mono);color:var(--muted)}
.tag{font-size:12px;color:var(--muted);border:1px solid var(--line);border-radius:4px;padding:0 6px}
.tag.k-command{color:var(--accent)}
.def{font:13px var(--mono);background:var(--code);border-radius:4px;padding:1px 6px}
.help{margin:4px 0 0}
.more{margin:6px 0 0;font-size:13px;color:var(--muted)}
.more code{font:13px var(--mono);background:var(--code);border-radius:4px;padding:0 4px;color:var(--fg)}
.ov-own{color:var(--own)}.ov-same{color:var(--same)}
.empty{color:var(--muted);padding:40px 0;text-align:center}
footer{color:var(--muted);font-size:12px;padding:0 16px 24px}
@media (max-width:640px){header{position:static}.count{width:100%;margin-left:0}}
)CSS";

// The page's behaviour: the search, the filters, the rows drawn from the JSON.
constexpr std::string_view kScript = R"JS(
const M=JSON.parse(document.getElementById('data').textContent);
const KINDS=[['setting','Settings'],['command','Commands'],['flag','Launch flags']];
const LAYERS={engine:'Engine',shooter:'Shooter',classic:'Classic games',shell:'Shell',game:"A game's own"};
const st={q:'',game:'',kind:'',layer:'',program:''};
try{const h=new URLSearchParams(location.search);for(const k in st)if(h.get(k))st[k]=h.get(k)}catch(e){}
const gameName=id=>(M.games.find(g=>g.id===id)||{name:id}).name;
const progName=id=>(M.programs.find(p=>p.id===id)||{name:id}).name;
const layerName=l=>l&&l.startsWith('game:')?gameName(l.slice(5))+"'s own":(LAYERS[l]||l||'');
const layerKey=l=>l&&l.startsWith('game:')?'game':l;
function inGames(gs){if(!st.game)return true;const want=[st.game,...((M.games.find(g=>g.id===st.game)||{}).takes_in||[])];return gs.some(g=>want.includes(g))}
function view(r){if(!st.game||!r.o)return r;for(const o of r.o)if(inGames(o.g))return Object.assign({},r,{d:o.d!==undefined?o.d:r.d,h:o.h||r.h,v:o.v||r.v,mn:o.mn||r.mn,mx:o.mx||r.mx,ov:o.ov,ol:o.l});return r}
function spellings(r){return (r.a||[]).filter(a=>inGames(a.g))}
function el(t,c,x){const e=document.createElement(t);if(c)e.className=c;if(x!==undefined)e.textContent=x;return e}
function chips(box,label,items,key){const g=el('div','group');g.append(el('b',null,label));
 for(const [v,t] of [['','All'],...items]){const b=el('button','chip',t);b.type='button';b.setAttribute('aria-pressed',st[key]===v);
  b.onclick=()=>{st[key]=v;draw()};g.append(b)}box.append(g)}
const ROWS=M.rows.map(r=>Object.assign(r,{hay:[r.n,r.h||'',r.v||'',...(r.a||[]).map(a=>a.n),...(r.o||[]).map(o=>o.h||'')].join(' ').toLowerCase()}));
function matches(r){
 if(st.kind&&r.k!==st.kind)return false;
 if(st.program&&!r.p.includes(st.program))return false;
 if(st.layer&&layerKey(r.l)!==st.layer&&!(st.layer==='game'&&(r.o||[]).length))return false;
 if(st.game&&r.g&&!inGames(r.g))return false;
 if(st.q){for(const w of st.q.toLowerCase().split(/\s+/))if(w&&!r.hay.includes(w))return false}
 return true}
function row(r0){const r=view(r0);const d=el('div','row');d.id=r.k+'-'+r.n;
 const h=el('div','head');const a=el('a','name',r.n);a.href='#'+d.id;h.append(a);
 if(r.arg)h.append(el('span','arg',r.arg));
 if(r.k==='setting'&&r.d!==undefined){const s=el('span','def',r.d===''?'""':r.d);s.title='default';h.append(s)}
 h.append(el('span','tag k-'+r.k,r.k==='flag'?'flag':r.k));
 if(r.l)h.append(el('span','tag',layerName(r.ol||r.l)));
 if(r.g)h.append(el('span','tag',r.g.map(gameName).join(', ')+' only'));
 if(M.programs.length>1)h.append(el('span','tag',r.p.map(progName).join(', ')));
 d.append(h);if(r.h)d.append(el('p','help',r.h));
 const more=[];
 if(r.t&&r.t!=='text'){let t=r.t;if(r.mn||r.mx)t+=' '+(r.mn||'-∞')+' to '+(r.mx||'∞');more.push(['Type',t])}
 if(r.v)more.push(['Values',r.v]);
 const sp=spellings(r0);if(sp.length)more.push(['Also',sp.map(s=>s.n+(s.part?' (word '+(+s.part+1)+')':'')+' in '+s.g.map(gameName).join(', ')).join('; ')]);
 if(!st.game&&r0.o)for(const o of r0.o)more.push([o.g.map(gameName).join(', '),(o.ov==='own'?'its own meaning':o.ov==='same'?'its own default':'its own')+(o.d!==undefined?' '+(o.d===''?'""':o.d):'')+(o.h&&o.h!==r0.h?': '+o.h:'')]);
 if(st.game&&r.ov)more.push([gameName(st.game),r.ov==='own'?"this game's own meaning of the name":"this game's own default"]);
 for(const [k,v] of more){const p=el('p','more');p.append(el('span',null,k+': '));p.append(el('span',null,v));d.append(p)}
 return d}
let shown=0;
function draw(){const f=document.getElementById('filters');f.textContent='';
 if(M.games.length)chips(f,'Game',M.games.map(g=>[g.id,g.name]),'game');
 chips(f,'Kind',KINDS,'kind');
 const ls=[...new Set(M.rows.map(r=>layerKey(r.l)).filter(Boolean))];
 chips(f,'Layer',ls.map(l=>[l,l==='game'?"A game's own":(LAYERS[l]||l)]),'layer');
 if(M.programs.length>1)chips(f,'Program',M.programs.map(p=>[p.id,p.name]),'program');
 const list=ROWS.filter(matches);const box=document.getElementById('rows');box.textContent='';
 const frag=document.createDocumentFragment();for(const r of list)frag.append(row(r));box.append(frag);
 if(!list.length)box.append(el('p','empty','Nothing matches. Clear the search or pick All.'));
 const c=el('span','count',list.length+' of '+ROWS.length);f.append(c);
 try{const u=new URL(location.href);for(const k in st){if(st[k])u.searchParams.set(k,st[k]);else u.searchParams.delete(k)}history.replaceState(null,'',u)}catch(e){}}
const q=document.getElementById('q');q.value=st.q;q.oninput=()=>{st.q=q.value;draw()};
document.addEventListener('keydown',e=>{if(e.key==='/'&&document.activeElement!==q){e.preventDefault();q.focus()}});
draw();if(location.hash){const t=document.getElementById(location.hash.slice(1));if(t)t.scrollIntoView()}
)JS";

}  // namespace

std::string manual_html(const ManualSource& source, const std::vector<ManualRow>& rows,
                        const ManualPage& page) {
  std::string out =
      "<!doctype html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n"
      "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n<title>" +
      html_text(page.title) + "</title>\n<style>" + std::string(kStyle) +
      "</style>\n</head>\n<body>\n<header><div class=\"wrap\"><h1>" + html_text(page.title) +
      "</h1>";
  if (!page.intro.empty()) out += "<p class=\"intro\">" + html_text(page.intro) + "</p>";
  out +=
      "<input id=\"q\" type=\"search\" placeholder=\"Search names and descriptions  ( / )\" "
      "aria-label=\"Search\" autocomplete=\"off\"><div id=\"filters\" class=\"filters\"></div>"
      "</div></header>\n<main><div class=\"wrap\" id=\"rows\"></div></main>\n<footer><div "
      "class=\"wrap\">Generated from the programs' own registrations";
  if (!page.version.empty()) out += " · " + html_text(page.version);
  out += "</div></footer>\n<script type=\"application/json\" id=\"data\">" +
         manual_json(source, rows, page) + "</script>\n<script>" + std::string(kScript) +
         "</script>\n</body>\n</html>\n";
  return out;
}

}  // namespace ge::console
