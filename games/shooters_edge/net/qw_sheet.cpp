// Shooter's Edge's QuakeWorld server's settings sheet (qw_sheet.hpp).
//
// ge:summary The QuakeWorld server's sheet rows from its rules, votes and aids; sent as svc ge_sheet
// ge:keywords qw_sheet_rows qw_send_sheet qw_sheet_group
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_sheet_test.cpp
#include "shooters_edge/net/qw_sheet.hpp"

#include <array>
#include <cctype>

#include "classic/protocol/info.hpp"
#include "shooters_edge/net/qw_admin.hpp"
#include "shooters_edge/net/qw_ext.hpp"
#include "shooters_edge/net/qw_svc.hpp"

namespace shooters_edge::net {

namespace gs = ge::server;

namespace {

// The match's own rules, on the first page in this order.
constexpr std::array<std::string_view, 8> kFirstPage = {
    "deathmatch",          "teamplay", "timelimit", "fraglimit", "sv_overtime",
    "sv_overtime_minutes", "k_midair", "samelevel"};

constexpr std::string_view kHelp = "Player help", kMoves = "Movement", kRules = "Rules";

constexpr size_t kRowBytes = 700;  // a row's text at most: two fit a reliable message
// The channel's waiting messages below which a sheet's next parts are sent (of kBackBuffers).
constexpr int kSheetQueue = 3;

// Our own rules' words on the pages; any other rule's come from its name (sheet_words_of).
std::string words_of(std::string_view name) {
  struct Known {
    std::string_view name, words;
  };
  static constexpr Known kKnown[] = {
      {"sv_rjump", "Rocket jump helper"},
      {"sv_rrjump", "Reverse jump helper"},
      {"sv_aimhelp_allow", "Aim help weapons"},
      {"sv_aimhelp_deny", "Aim help never on"},
      {"sv_aimhelp_x", "Aim help across"},
      {"sv_aimhelp_y", "Aim help up"},
      {"pm_speedjump", "Easy jump share"},
      {"sv_overtime", "Overtime"},
      {"sv_overtime_minutes", "Overtime minutes"},
      {"k_midair", "Midair"},
      {"k_midair_minheight", "Midair least height"},
  };
  for (const Known& k : kKnown)
    if (k.name == name) return std::string(k.words);
  return gs::sheet_words_of(name);
}

}  // namespace

std::string qw_sheet_group(std::string_view name) {
  for (const std::string_view first : kFirstPage)
    if (name == first) return {};
  if (name.starts_with("sv_rjump") || name.starts_with("sv_rrjump") ||
      name.starts_with("sv_aimhelp") || name == "pm_speedjump")
    return std::string(kHelp);
  if (name.starts_with("pm_") || name == "sv_gravity" || name == "sv_friction" ||
      name == "sv_stopspeed" || name == "sv_maxspeed" || name == "sv_spectatormaxspeed" ||
      name == "sv_accelerate" || name == "sv_airaccelerate" || name == "sv_wateraccelerate" ||
      name == "sv_waterfriction" || name == "sv_maxvelocity" || name == "sv_playermove")
    return std::string(kMoves);
  return std::string(kRules);
}

void qw_sheet_rows(const QwSheetFacts& f, gs::SettingsSheet& out) {
  out.rows.clear();
  out.whole = true;
  out.revision++;
  const gs::VoteSettings votes = f.vote != nullptr ? *f.vote : gs::VoteSettings{};
  {
    gs::SheetRow map;
    map.name = "map";
    map.words = "Map";
    map.value = f.map;
    map.values = "TEXT: the map's name";
    map.help = "The map being played.";
    map.how = gs::sheet_how_by_vote(votes, "map");
    gs::sheet_put(out, std::move(map));
  }
  // The first page's rules first, in their order, then the rest as the game lists them.
  const auto add = [&](const gs::GameRule& r) {
    const std::string name = r.name;
    if (gs::sheet_find(out, name) != nullptr || (f.fixed && f.fixed(name))) return;
    const std::string group = qw_sheet_group(name);
    const gs::SheetHow how = gs::sheet_how_by_vote(votes, name);
    if (how == gs::SheetHow::fixed && !group.empty()) return;  // only what may change
    gs::SheetRow row;
    row.name = name;
    row.words = words_of(name);
    row.group = group;
    row.value = f.value ? f.value(name) : std::string();
    if (row.value.empty() && r.fallback != nullptr) row.value = r.fallback;
    row.values = r.values != nullptr ? r.values : "";
    row.help = r.help != nullptr ? r.help : "";
    row.how = how;
    gs::sheet_put(out, std::move(row));
  };
  for (const std::string_view first : kFirstPage)
    for (const gs::GameRule& r : f.rules)
      if (first == r.name) add(r);
  for (const gs::GameRule& r : f.rules) add(r);
  // Each player's aids, a row a player and an aid.
  if (f.players == nullptr) return;
  for (const gs::AidKind& kind : qw_aids()) {
    if (gs::sheet_how_by_vote(votes, kind.name) != gs::SheetHow::vote) continue;
    for (int i = 0; i < sh::kMaxClients; i++) {
      const QwClient& c = f.players->clients[i];
      if (c.state != ConnState::spawned || c.spectator ||
          !ge::classic::protocol::info_get(c.userinfo, "*bot").empty())
        continue;
      gs::SheetRow row;
      row.name = gs::aid_vote_key(kind, c.userid);
      std::string words = kind.words;
      if (!words.empty())
        words[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(words[0])));
      row.words = words + " for " + c.name;
      row.group = std::string(kHelp);
      row.value = gs::aid_vote_word(kind, qw_aid_granted(*f.players, i, kind));
      row.values = "off: not granted; on: granted";
      row.line = "vote " + std::string(kind.name) + " " + std::to_string(c.userid) + " %v";
      row.how = gs::SheetHow::vote;
      gs::sheet_put(out, std::move(row));
    }
  }
}

void qw_send_sheet(QwServer& s, int slot, const gs::SettingsSheet& sheet, QwSheetOut& out) {
  out = QwSheetOut{};
  if (slot < 0 || slot >= sh::kMaxClients || s.clients[slot].local ||
      !s.clients[slot].ext.has(ExtFamily::ge, kGeSettings))
    return;  // a client that did not agree the bit would not read the message
  out.parts.emplace_back(gs::SheetPart::begin, std::string());
  for (const gs::SheetRow& r : sheet.rows)
    out.parts.emplace_back(gs::SheetPart::row, gs::sheet_row_text(r, kRowBytes));
  out.parts.emplace_back(gs::SheetPart::end, std::string());
  qw_sheet_pump(s, slot, out);
}

void qw_sheet_pump(QwServer& s, int slot, QwSheetOut& out) {
  if (out.next >= out.parts.size() || slot < 0 || slot >= sh::kMaxClients) return;
  QwClient& c = s.clients[slot];
  if (c.state == ConnState::free || c.local) {
    out = QwSheetOut{};
    return;
  }
  // A message's worth at a time, while the channel keeps at most half its queue waiting.
  while (out.next < out.parts.size() && c.ch.pending_count < kSheetQueue) {
    SvcMsg m;
    m.id = svc::ge_sheet;
    m.ints = {static_cast<int>(out.parts[out.next].first)};
    m.strings = {out.parts[out.next].second};
    reliable_svc(s, slot, m);
    out.next++;
  }
}

}  // namespace shooters_edge::net
