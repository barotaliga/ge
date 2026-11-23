// The window's view of a server's settings sheet (qw_remote_sheet.hpp).
//
// ge:summary The server settings page's sheet kept from a listen, our server or a stock one
// ge:keywords remote_sheet_command remote_sheet_step remote_sheet_take_print serversheet
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_sheet_test.cpp
#include "shooters_edge/net/qw_remote_sheet.hpp"

#include <algorithm>

#include "shooters_edge/net/q2_remote.hpp"
#include "shooters_edge/net/qw_ext.hpp"
#include "shooters_edge/net/qw_remote.hpp"

namespace shooters_edge::net {

namespace gc = ge::classic;
namespace gs = ge::server;

namespace {

// Our server: QuakeWorld's agreed the bit; Quake II's offered its sheet (svc 120) as we entered.
bool ours(const Remote& r) {
  if (!r.active || r.nq) return false;
  if (r.q2) return q2_demo(*r.q2).sheet_offered;
  return r.end.ext.has(ExtFamily::ge, kGeSettings);
}
// The sheet our server sent this end.
gs::SettingsSheet& sent(Remote& r) { return r.q2 ? q2_demo(*r.q2).sheet : r.end.sheet; }

void say(Remote& r, const std::string& line) { remote_command(r, line); }

// The listings of the server the window is on asked (again).
void ask_listings(Remote& r, double now_s) {
  RemoteSheet& s = r.sheet;
  if (s.source == gc::SheetSource::qw_match) {
    for (const std::string& line : gc::qw_match_listing_asks()) say(r, line);
    s.listen_until = now_s + kListenSeconds;
  } else if (s.source == gc::SheetSource::q2_plain || s.source == gc::SheetSource::q2_match) {
    s.q2_info.clear();
    s.q2_match_asked = false;
    say(r, "info");
    s.listen_until = now_s + kListenSeconds;
  }
}

// A stock QuakeWorld server's sheet read from its info again, a listing's values kept where the
// info says nothing of them.
void read_qw_info(Remote& r) {
  RemoteSheet& s = r.sheet;
  const gs::SettingsSheet before = s.sheet;
  gc::qw_sheet_from_info(s.sheet, r.end.serverinfo);
  for (gs::SheetRow& row : s.sheet.rows)
    if (row.value.empty())
      if (const gs::SheetRow* was = gs::sheet_find(before, row.name)) row.value = was->value;
}

}  // namespace

void remote_sheet_command(Remote& r, const std::string& word, double now_s) {
  RemoteSheet& s = r.sheet;
  if (word == "close") {
    if (s.open && s.source == gc::SheetSource::ours && ours(r)) say(r, "ge_sheet stop");
    s.open = false;
    s.listen_until = -1;
    s.ask_at = -1;
    return;
  }
  if (word == "changed") {  // a change sent: the listings asked again a moment later
    if (s.source != gc::SheetSource::ours && s.source != gc::SheetSource::none)
      s.ask_at = now_s + 0.5;
    return;
  }
  const bool opening = word == "open" || !s.open;
  s.open = true;
  // The window's own server, unless a joined server is up: the window keeps its listen game (the
  // demo behind the menu) while it plays elsewhere, and the page is the joined server's then.
  if (!r.active && s.local && s.local(s.sheet)) {
    s.source = gc::SheetSource::ours;
    return;
  }
  if (!r.active || r.nq) {
    s.source = gc::SheetSource::none;
    s.sheet = gs::SettingsSheet{};
    return;
  }
  if (ours(r)) {
    s.source = gc::SheetSource::ours;
    if (opening) {
      sent(r).asked = true;
      say(r, "ge_sheet");
    }
    return;
  }
  if (!r.q2) {
    s.source = gc::qw_sheet_source(r.end.serverinfo, false);
    read_qw_info(r);
    if (opening) ask_listings(r, now_s);
    return;
  }
  if (opening) {
    if (s.source != gc::SheetSource::q2_match) s.source = gc::SheetSource::q2_plain;
    ask_listings(r, now_s);
  }
}

bool remote_sheet_take_print(RemoteSheet& s, bool q2, const std::string& text, std::string& keep) {
  keep.clear();
  bool took = false;
  size_t at = 0;
  while (at < text.size()) {
    const size_t end = std::min(text.find('\n', at), text.size());
    const std::string line = text.substr(at, end - at);
    const std::string with_end = text.substr(at, end + 1 - at);
    at = end + 1;
    bool mine = false;
    if (q2) {
      if (std::pair<std::string, std::string> kv; gc::q2_info_read(line, kv)) {
        s.q2_info.push_back(std::move(kv));
        mine = true;
      } else if (s.source == gc::SheetSource::q2_match) {
        mine = gc::q2_match_listing_read(s.sheet, line);
      }
    } else if (s.source == gc::SheetSource::qw_match) {
      mine = gc::qw_match_listing_read(s.sheet, line);
    }
    took = took || mine;
    if (!mine) keep += with_end;
  }
  return took;
}

void remote_sheet_step(Remote& r, std::vector<client::DemoPrint>& prints, double now_s) {
  RemoteSheet& s = r.sheet;
  if (!s.open) return;
  // The sheet as asked: a whole one, never the empty one offered before the window asked.
  if (s.source == gc::SheetSource::ours && !s.local && ours(r) && sent(r).revision != s.ours_seen &&
      sent(r).whole && !sent(r).asked) {
    s.ours_seen = sent(r).revision;
    s.sheet = sent(r);
  }
  if (s.source == gc::SheetSource::qw_plain || s.source == gc::SheetSource::qw_match) {
    read_qw_info(r);  // the info changes as the server's settings do
  }
  if (s.ask_at >= 0 && now_s >= s.ask_at) {
    s.ask_at = -1;
    ask_listings(r, now_s);
  }
  if (s.listen_until < 0 || now_s > s.listen_until) return;
  const bool q2 = r.q2 != nullptr;
  const size_t info_before = s.q2_info.size();
  for (client::DemoPrint& p : prints) {
    std::string keep;
    if (remote_sheet_take_print(s, q2, p.text, keep)) p.text = keep;
  }
  std::erase_if(prints, [](const client::DemoPrint& p) { return p.text.empty(); });
  if (q2 && s.q2_info.size() != info_before) {
    s.source = gc::q2_sheet_source(s.q2_info);
    const gs::SettingsSheet before = s.sheet;
    gc::q2_sheet_from_info(s.sheet, s.q2_info);
    for (gs::SheetRow& row : s.sheet.rows)
      if (row.value.empty())
        if (const gs::SheetRow* was = gs::sheet_find(before, row.name)) row.value = was->value;
    if (s.source == gc::SheetSource::q2_match && !s.q2_match_asked) {
      s.q2_match_asked = true;
      say(r, "settings");
      s.listen_until = now_s + kListenSeconds;
    }
  }
}

}  // namespace shooters_edge::net
