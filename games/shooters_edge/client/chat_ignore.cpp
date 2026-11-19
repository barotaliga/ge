// The classic games' chat lines read for the ignore lists (chat_ignore.hpp).
//
// ge:summary The classic games' chat lines read for the engine's ignore lists on a server joined
// ge:keywords chat-ignore classic-chat drop_ignored chat_line_of roster_of
// ge:skill net shooter
// ge:test tests/shooters_edge_chat_ignore_test.cpp
#include "shooters_edge/client/chat_ignore.hpp"

#include <algorithm>

#include "console/cvar.hpp"
#include "shooters_edge/client/cvar_layers.hpp"

namespace shooters_edge::client {

namespace con = ge::console;

namespace {

constexpr int kChatLevel = 3, kHighLevel = 2;

std::string info(const DemoState& s, const char* key) {
  for (const auto& [k, v] : s.serverinfo)
    if (k == key) return v;
  return {};
}

bool lower_is(std::string a, const char* b) {
  for (char& c : a)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return a == b;
}

// `#<digits>:` at the start: a TV viewer's chat.
bool tv_line(const std::string& t) {
  if (t.size() < 3 || t[0] != '#') return false;
  size_t i = 1;
  while (i < t.size() && t[i] >= '0' && t[i] <= '9') ++i;
  return i > 1 && i < t.size() && t[i] == ':';
}

}  // namespace

void add_classic_ignore(con::Console& c, ClassicIgnore& g, const DemoState& s) {
  {
    con::CvarLayer engine(c.cvars(), kLayerEngine);
    con::add_chat_ignore(c, g.lists);
  }
  con::CvarLayer shooter(c.cvars(), kLayerShooter);
  ge::shooter::add_weapon_line_cvars(c.cvars(), g.lines);
  g.levels = s.levels;
  const DemoState* st = &s;
  g.lists.roster = [st] { return roster_of(*st); };
}

con::ChatRoster roster_of(const DemoState& s) {
  con::ChatRoster r;
  for (size_t k = 0; k < r.names.size(); ++k) {
    r.names[k] = s.names[k];
    r.teams[k] = s.teams[k];
    r.spectator[k] = s.spectator[k];
  }
  r.me = s.player;
  const std::string tp = info(s, "teamplay");
  r.teams_play = !tp.empty() && tp != "0";
  r.match_running = !lower_is(info(s, "status"), "standby");
  return r;
}

con::ChatLine chat_line_of(const DemoState& s, const DemoPrint& p) {
  con::ChatLine line;
  line.text = p.text;
  if (p.level != kChatLevel) return line;
  if (tv_line(p.text)) {
    line.kind = con::ChatKind::tv;
    return line;
  }
  const con::ChatRoster r = roster_of(s);
  const bool watching =
      r.me >= 0 && r.me < con::kChatPlaces && r.spectator[static_cast<size_t>(r.me)];
  size_t best = 0;
  for (int k = 0; k < con::kChatPlaces; ++k) {
    const std::string& n = r.names[static_cast<size_t>(k)];
    if (n.empty() || n.size() < best) continue;
    const bool spec = r.spectator[static_cast<size_t>(k)];
    con::ChatKind kind = con::ChatKind::none;
    if (p.text.starts_with(n + ": ")) {
      kind = spec ? con::ChatKind::spectator : con::ChatKind::everyone;
    } else if (p.text.starts_with("(" + n + "): ")) {
      const bool mine =
          r.me >= 0 && r.teams[static_cast<size_t>(k)] == r.teams[static_cast<size_t>(r.me)];
      if (k == r.me || (r.teams_play && mine)) kind = con::ChatKind::team;
    } else if (p.text.starts_with("[SPEC] " + n + ": ") && watching && spec) {
      kind = con::ChatKind::spectator_team;
    }
    if (kind == con::ChatKind::none) continue;
    line.kind = kind;
    line.place = k;
    best = n.size();
  }
  return line;
}

void drop_ignored(ClassicIgnore& g, const DemoState& s, std::vector<DemoPrint>& prints,
                  double now_s) {
  if (g.levels != s.levels) {  // a new map: the players' flags and the flood's memory go
    g.levels = s.levels;
    con::chat_new_level(g.lists);
  }
  if (prints.empty()) return;
  const con::ChatRoster r = roster_of(s);
  std::erase_if(prints, [&](const DemoPrint& p) {
    if (p.level == kHighLevel) return ge::shooter::weapon_line_hidden(g.lines, p.text);
    return con::chat_ignored(g.lists, r, chat_line_of(s, p), now_s);
  });
}

}  // namespace shooters_edge::client
