// Our Quake II server's votes and settings sheet (q2_votes.hpp).
//
// ge:summary Our Quake II server's votes and the settings sheet it sends on svc 120
// ge:keywords q2-votes vote ge_sheet settings-sheet
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_votes_test.cpp
#include "shooters_edge/net/q2_votes.hpp"

#include <cstdlib>
#include <string_view>

#include "classic/protocol/info.hpp"
#include "shooters_edge/net/qw_ext.hpp"
#include "shooters_edge/net/qw_sheet.hpp"

namespace shooters_edge::net {

namespace gs = ge::server;

namespace {

constexpr size_t kRowBytes = 600;  // a row's text at most
// The channel's reliable bytes (waiting and in flight) the sheet's parts may bring it to: a
// packet's reliable part and the frame after it stay under the 1400 bytes a client reads.
constexpr size_t kSheetBacklog = 640;
constexpr int kSlots = 32;

// The rules the rules read only as a map starts: a vote on one restarts the map.
bool restarts(std::string_view name) {
  return name == "k_midair" || name == "sv_fps" || name == "sv_wear" || name == "sv_physics";
}

// The vote's lists with the owner's rules kept out, as the vote and the sheet both judge them.
gs::VoteSettings judged(const gs::VoteSettings& set) {
  gs::VoteSettings out = set;
  out.deny += (out.deny.empty() ? "" : " ") + std::string(kQ2OwnerRules);
  return out;
}

bool held(const Q2NetClient& c) { return c.stage == Q2Slot::active || c.stage == Q2Slot::signon; }

bool agreed(const Q2NetClient& c) {
  const int ext = std::atoi(ge::classic::protocol::info_get(c.userinfo, "*ge_ext").c_str());
  return (ext & static_cast<int>(kGeSettings)) != 0;
}

void tell(Q2Votes& v, int to, const std::string& line) {
  Q2NetServer& s = *v.net;
  for (int i = 0; i < kSlots && i < sh::kMaxClients; i++)
    if ((to < 0 || to == i) && s.clients[i].stage == Q2Slot::active) q2_net_print(s, i, line);
}

// The slot of the player `who` names: his user id, else his name; -1 when nobody.
int seat_of(const Q2NetServer& s, std::string_view who) {
  char* end = nullptr;
  const std::string text(who);
  const long id = std::strtol(text.c_str(), &end, 10);
  const bool number = end != text.c_str() && *end == '\0';
  for (int i = 0; i < kSlots && i < sh::kMaxClients; i++) {
    const Q2NetClient& c = s.clients[i];
    if (c.stage != Q2Slot::active) continue;
    if (number ? c.userid == id : c.name == text) return i;
  }
  return -1;
}

bool check(void* ctx, std::string_view name, std::string_view value, gs::VoteCheck& out) {
  const Q2Votes& v = *static_cast<const Q2Votes*>(ctx);
  const std::string text(value);
  if (name == "map") {
    if (v.has_map && !v.has_map(text)) {
      out.error = "This server has no map called " + text + ".";
      return false;
    }
    out.act = true;
    out.before = v.map ? v.map() : std::string();
    return true;
  }
  if (name == "kick") {
    const int slot = seat_of(*v.net, text);
    if (slot < 0) {
      out.error = "Nobody called " + text + " is playing here.";
      return false;
    }
    out.act = true;
    out.key = "kick:" + std::to_string(v.net->clients[slot].userid);
    out.value = v.net->clients[slot].name;
    return true;
  }
  const ge::console::Cvar* c = (*v.rules)->cvars.find(name);
  if (c == nullptr) {
    out.error = std::string(name) +
                " is not a setting of this server. Type \"vote rules\" to see what may be voted "
                "on (map <name>, kick <player>, fraglimit, timelimit, dmflags, sv_gravity...).";
    return false;
  }
  if (c->kind != ge::console::CvarKind::text) {
    char* end = nullptr;
    std::strtod(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0') {
      out.error = std::string(name) + " \"" + text + "\" is not a number.";
      return false;
    }
  }
  if (name == "timelimit" || name == "fraglimit") {  // a match that never ends refused
    const ge::console::Cvar* frags = (*v.rules)->cvars.find("fraglimit");
    const ge::console::Cvar* time = (*v.rules)->cvars.find("timelimit");
    out.error = gs::vote_limit_refusal(name, std::strtod(text.c_str(), nullptr),
                                       frags ? frags->number : 0, time ? time->number : 0, 0);
    if (!out.error.empty()) return false;
  }
  out.before = c->value;
  return true;
}

void send_part(Q2NetClient& c, gs::SheetPart part, const std::string& text) {
  std::vector<uint8_t> b = {kSvcQ2GeSheet, static_cast<uint8_t>(part)};
  b.insert(b.end(), text.begin(), text.end());
  b.push_back(0);
  channel_reliable(c.ch, b.data(), b.size());
}

void queue_sheet(Q2Votes& v, int slot, const gs::SettingsSheet& sheet) {
  auto& parts = v.out[static_cast<size_t>(slot)];
  parts.clear();
  v.out_next[static_cast<size_t>(slot)] = 0;
  parts.emplace_back(gs::SheetPart::begin, std::string());
  for (const gs::SheetRow& r : sheet.rows)
    parts.emplace_back(gs::SheetPart::row, gs::sheet_row_text(r, kRowBytes));
  parts.emplace_back(gs::SheetPart::end, std::string());
}

bool hook(void* ctx, int slot, const std::string& line) {
  Q2Votes& v = *static_cast<Q2Votes*>(ctx);
  if (v.prior != nullptr && v.prior(v.prior_ctx, slot, line)) return true;
  if (v.rules == nullptr) return false;
  if (slot < 0 || slot >= kSlots || slot >= sh::kMaxClients) return false;
  const Q2NetClient& c = v.net->clients[slot];
  if (line == "ge_sheet" || line == "ge_sheet stop") {
    const uint32_t bit = 1u << slot;
    if (!agreed(c)) return true;  // ours, and never the rules' (a stock rule would echo it)
    if (line == "ge_sheet") {
      v.watchers |= bit;
      v.watcher_id[static_cast<size_t>(slot)] = c.userid;
      gs::SettingsSheet sheet;
      q2_votes_sheet(v, sheet);
      queue_sheet(v, slot, sheet);
    } else {
      v.watchers &= ~bit;
      v.out[static_cast<size_t>(slot)].clear();
    }
    return true;
  }
  if (c.stage != Q2Slot::active) return false;
  const gs::VoteTarget target{&v, check};
  return gs::vote_command(v.book, target, slot, line, v.net->now);
}

// The vote's lines told and its passed changes made.
void turn(Q2Votes& v) {
  gs::VoteBook& b = v.book;
  for (size_t i = 0; i < b.said.size(); i++) tell(v, b.notes[i].to, b.said[i] + "\n");
  b.said.clear();
  b.notes.clear();
  bool restart = false;
  std::string to_map;
  for (const gs::VoteChange& ch : gs::vote_take(b)) {
    v.changed = true;
    if (ch.name == "kick") {
      const int slot = seat_of(*v.net, ch.key.substr(5));
      if (slot < 0) continue;  // he left already
      tell(v, -1, v.net->clients[slot].name + " was kicked by a vote\n");
      q2_net_drop(*v.net, slot, "kicked by a vote");
      continue;
    }
    if (ch.name == "map") {
      to_map = ch.value;
      continue;
    }
    v.rules->console(ch.name + " \"" + ch.value + "\"");
    restart = restart || restarts(ch.name);
  }
  sv::RulesCommon& now = **v.rules;
  if (!to_map.empty()) {
    tell(v, -1, "The vote moves the server to " + to_map + "\n");
    now.pending_map = to_map;
    now.pending_change = sv::MapChange::change_level;
  } else if (restart && v.map) {
    tell(v, -1, "The map restarts so everyone plays by the new setting\n");
    now.pending_map = v.map();
    now.pending_change = sv::MapChange::restart;
  }
}

}  // namespace

void q2_votes_rules(Q2Votes& v, sv::RulesRunner& rules, const gs::VoteSettings& settings) {
  v.rules = &rules;
  v.book.settings = judged(settings);
}

void q2_votes_open(Q2Votes& v, Q2NetServer& net) {
  v.net = &net;
  v.offered.fill(-1);
  v.watcher_id.fill(-1);
  v.prior = net.line_hook;
  v.prior_ctx = net.hook_ctx;
  net.line_hook = hook;
  net.hook_ctx = &v;
}

void q2_votes_frame(Q2Votes& v) {
  if (v.net == nullptr || v.rules == nullptr) return;
  Q2NetServer& s = *v.net;
  for (int i = 0; i < kSlots && i < sh::kMaxClients; i++) {
    const Q2NetClient& c = s.clients[i];
    gs::vote_seat(v.book, i, c.stage == Q2Slot::active ? c.name : std::string(), s.now);
  }
  gs::vote_tick(v.book, s.now);
  turn(v);
  gs::SettingsSheet sheet;
  bool made = false;
  const auto now_sheet = [&]() -> const gs::SettingsSheet& {
    if (!made) q2_votes_sheet(v, sheet);
    made = true;
    return sheet;
  };
  for (int i = 0; i < kSlots && i < sh::kMaxClients; i++) {
    Q2NetClient& c = s.clients[i];
    const auto k = static_cast<size_t>(i);
    const uint32_t bit = 1u << i;
    if (!held(c) || ((v.watchers & bit) != 0 && v.watcher_id[k] != c.userid)) {
      v.watchers &= ~bit;  // the slot left, or another took it
      v.out[k].clear();
    }
    if (c.stage != Q2Slot::active) {
      v.offered[k] = -1;
      continue;
    }
    if (v.offered[k] != c.userid && agreed(c)) {  // entered: offered, or sent again if kept
      v.offered[k] = c.userid;
      if ((v.watchers & bit) != 0) {
        queue_sheet(v, i, now_sheet());
      } else {
        v.out[k].clear();
        v.out[k].emplace_back(gs::SheetPart::begin, std::string());
        v.out[k].emplace_back(gs::SheetPart::end, std::string());
        v.out_next[k] = 0;
      }
    } else if (v.changed && (v.watchers & bit) != 0) {
      queue_sheet(v, i, now_sheet());
    }
    auto& parts = v.out[k];
    size_t& next = v.out_next[k];
    while (next < parts.size() &&
           channel_reliable_backlog(c.ch) + parts[next].second.size() + 3 <= kSheetBacklog) {
      send_part(c, parts[next].first, parts[next].second);
      next++;
    }
    if (next >= parts.size()) {
      parts.clear();
      next = 0;
    }
  }
  v.changed = false;
}

void q2_votes_map_over(Q2Votes& v) {
  if (v.rules == nullptr) return;
  for (const gs::VoteChange& ch : gs::vote_map_over(v.book))
    if (!ch.act) v.rules->console(ch.name + " \"" + ch.value + "\"");
  v.changed = true;
}

void q2_votes_sheet(const Q2Votes& v, gs::SettingsSheet& out) {
  if (v.rules == nullptr) return;
  std::vector<gs::GameRule> rules;
  for (const ge::console::Cvar* c : (*v.rules)->cvars.sorted())
    rules.push_back({c->name.c_str(), c->help.c_str(), nullptr, c->values.c_str()});
  QwSheetFacts f;
  f.rules = rules;
  f.value = [&v](std::string_view name) {
    const ge::console::Cvar* c = (*v.rules)->cvars.find(name);
    return c != nullptr ? c->value : std::string();
  };
  const gs::VoteSettings set = v.book.settings;
  f.vote = &set;
  f.map = v.map ? v.map() : std::string();
  if (v.net != nullptr) f.ruleset = v.net->cfg.ruleset;
  qw_sheet_rows(f, out);
}

}  // namespace shooters_edge::net
