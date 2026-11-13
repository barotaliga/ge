// The list pages in the classic look (classic_pages.hpp): the frame they share, and DEMOS and JOIN
// A GAME laid out on it (demos_page.hpp, servers_page.hpp).
//
// ge:summary Draws DEMOS and JOIN A GAME on the classic screen in the game's glyph sheet
// ge:keywords menu classic demos server-browser list glyph-rows cursor footer draw_classic
// ge:skill ui
// ge:test tests/shooters_edge_menu_test.cpp
#include "shooters_edge/menu/classic_pages.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "shooters_edge/menu/demos_page.hpp"
#include "shooters_edge/menu/servers_page.hpp"

namespace shooters_edge::menu {

namespace {

namespace ui = ge::ui;
namespace gv = ge::vfs;
namespace gn = ge::net;

constexpr float kScreenW = 320;

std::string lower(std::string s) {
  for (char& c : s)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return s;
}

// Text a server sent: the high-bit letters folded to plain ones, control bytes dropped.
std::string printable(const std::string& s) {
  std::string out;
  for (char ch : s) {
    auto c = static_cast<unsigned char>(ch & 0x7f);
    if (c >= 0x12 && c <= 0x1b) c = static_cast<unsigned char>('0' + (c - 0x12));
    if (c < 0x20 || c == 0x7f) continue;
    out += static_cast<char>(c);
  }
  return out;
}

// `s` right-aligned to end at glyph column `end` (classic px) on row y.
void text_right(const ClassicPen& pen, std::string_view s, float end, float y, bool alt) {
  pen.text(s, end - 8 * static_cast<float>(s.size()), y, alt);
}

// A note of up to two rows of `n` glyphs, cut at a word.
void two_rows(const ClassicPen& pen, std::string_view note, float x, float y, size_t n, bool alt) {
  const std::string s = glyph_safe(note);
  if (s.size() <= n) {
    pen.text(s, x, y, alt);
    return;
  }
  size_t cut = s.rfind(' ', n);
  if (cut == std::string::npos || cut < n / 2) cut = n;
  pen.text(s.substr(0, cut), x, y, alt);
  pen.text(fit_chars(s.substr(cut == n ? cut : cut + 1), n), x, y + 8, alt);
}

}  // namespace

std::string glyph_safe(std::string_view s) {
  std::string out(s);
  for (char& c : out)
    if (static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) >= 0x7f) c = '?';
  return out;
}

std::string fit_chars(std::string_view s, size_t n, bool left) {
  if (s.size() <= n) return std::string(s);
  if (n <= 2) return std::string(s.substr(0, n));
  if (left) return ".." + std::string(s.substr(s.size() - (n - 2)));
  return std::string(s.substr(0, n - 2)) + "..";
}

void classic_list_frame(const ClassicPen& pen, std::string_view title, std::string_view carved,
                        std::string_view words) {
  pen.dl.rect({0, 0, static_cast<float>(pen.dl.width()), static_cast<float>(pen.dl.height())},
              ui::hex(0x000000, 0.6f));
  pen.picture(pen.art.find("qplaque"), 16, 4);
  if (const ui::Image* pic = title.empty() ? nullptr : pen.art.find(title)) {
    pen.picture(pic, std::floor((kScreenW - pen.art.width_of(*pic)) / 2), 4);
    return;
  }
  if (const float cw = carved.empty() ? 0 : carved_width(pen.art, carved); cw > 0) {
    carve_word(pen, carved, std::floor((kScreenW - cw) / 2), 4);
    return;
  }
  pen.big_text(words, std::floor((kScreenW - 16 * static_cast<float>(words.size())) / 2), 6);
}

void classic_cursor(const ClassicPen& pen, float y, double t) {
  pen.glyph(static_cast<uint8_t>(12 + (static_cast<int>(std::floor(t * 4)) & 1)), kListLeft, y);
}

// --- DEMOS ---

void DemosPage::draw_classic(ui::DrawList& list, const ClassicArt& art, int width, int height,
                             double t) {
  problems_.clear();
  cells_.clear();
  chips_.clear();
  const ClassicLayout l = classic_screen(width, height);
  const ClassicPen pen{list, art, l};
  classic_list_frame(pen, "", "demos", "DEMOS");
  const float x = kListLeft + 8;

  // The place, LEFT and RIGHT turning it, and the folder open under it (its tail kept).
  const std::vector<gv::Place>& places = browser_.places();
  const int here = browser_.place();
  const std::string place =
      here >= 0 && here < static_cast<int>(places.size())
          ? "< " + lower(glyph_safe(places[static_cast<size_t>(here)].name)) + " >"
          : "< elsewhere >";
  pen.text(place, std::floor(x + (kListChars * 8 - 8 * static_cast<float>(place.size())) / 2),
           kListTop, false);
  pen.text(fit_chars(glyph_safe(browser_.dir()), kListChars, true), x, kListTop + 10, true);

  // The rows: folders in the darker set ending in '/', recordings with their sizes.
  constexpr float kRowsTop = kListTop + 24;
  constexpr int kFits = 12;
  browser_.set_page_rows(kFits);
  const std::vector<gv::FolderEntry>& rows = browser_.entries();
  const int n = static_cast<int>(rows.size());
  const int selected = browser_.selected();
  if (selected < first_) first_ = selected;
  if (selected >= first_ + kFits) first_ = selected - kFits + 1;
  first_ = std::clamp(first_, 0, std::max(0, n - kFits));
  for (int r = first_; r < n && r < first_ + kFits; r++) {
    const gv::FolderEntry& e = rows[static_cast<size_t>(r)];
    const float y = kRowsTop + 8 * static_cast<float>(r - first_);
    cells_.push_back(pen.at(kListLeft, y, kListRight - kListLeft, 8));
    if (r == selected) classic_cursor(pen, y, t);
    if (e.folder) {
      const std::string name = e.name == ".." ? ".. up a folder" : glyph_safe(e.name) + "/";
      pen.text(fit_chars(name, kListChars), x, y, true);
      continue;
    }
    const std::string size = lower(gv::size_text(e.bytes));
    pen.text(fit_chars(glyph_safe(e.name), kListChars - size.size() - 1), x, y, false);
    text_right(pen, size, kListRight, y, true);
  }
  if (browser_.files() == 0 && n < kFits) {
    const float y = kRowsTop + 8 * static_cast<float>(n + 1);
    pen.text("No demos here.", x, y, false);
    pen.text("Recordings, plain or packed:", x, y + 10, true);
    pen.text(".gz .zip .7z .xz .tar.gz", x, y + 18, true);
  }
  if (n > kFits) {
    const std::string more = std::to_string(first_ + 1) + "-" +
                             std::to_string(std::min(n, first_ + kFits)) + " of " +
                             std::to_string(n);
    text_right(pen, more, kListRight, kRowsTop + 8 * kFits + 2, true);
  }

  // The foot: what the check said, else the selected entry, over the keys.
  constexpr float kNoteY = 172;
  if (!browser_.note().empty())
    two_rows(pen, browser_.note(), kFootLeft, kNoteY, kFootChars, false);
  else if (const gv::FolderEntry* sel = browser_.selection(); sel != nullptr && !sel->folder)
    pen.text(fit_chars(glyph_safe(sel->path), kFootChars, true), kFootLeft, kNoteY, true);
  pen.text("enter plays  bksp up  esc back", kFootLeft, 190, true);
}

// --- JOIN A GAME ---

void ServersPage::draw_classic(ui::DrawList& list, const ClassicArt& art, int width, int height,
                               double t) {
  problems_.clear();
  cells_.clear();
  const ClassicLayout l = classic_screen(width, height);
  const ClassicPen pen{list, art, l};
  classic_list_frame(pen, "p_multi", "", "SERVERS");
  const float x = kListLeft + 8;

  // The columns: the name, the map, players over slots and the ping; the sorted one's head lit.
  constexpr size_t kName = 12, kMap = 8;
  constexpr float kMapX = kListLeft + 8 + 8 * (kName + 1);
  constexpr float kPlayersEnd = kListRight - 8 * 4;
  const bool going = inited_ && !gn::server_list_done(list_, now_us_);
  pen.text("name", x, kListTop, sort_ != gn::ServerSort::name);
  pen.text("map", kMapX, kListTop, sort_ != gn::ServerSort::map);
  text_right(pen, "plyr", kPlayersEnd, kListTop, sort_ != gn::ServerSort::players);
  text_right(pen, "ms", kListRight, kListTop, sort_ != gn::ServerSort::ping);
  pen.glyph(29, x, kListTop + 8);
  for (int c = 1; c < kListChars - 1; c++)
    pen.glyph(30, x + 8 * static_cast<float>(c), kListTop + 8);
  pen.glyph(31, x + 8 * (kListChars - 1), kListTop + 8);

  constexpr float kRowsTop = kListTop + 18;
  constexpr int kFits = 13;
  const int n = static_cast<int>(rows_.size());
  if (selected_ < first_) first_ = selected_;
  if (selected_ >= first_ + kFits) first_ = selected_ - kFits + 1;
  first_ = std::clamp(first_, 0, std::max(0, n - kFits));
  for (int r = first_; r < n && r < first_ + kFits; r++) {
    const gn::ServerEntry& e = list_.entries[static_cast<size_t>(rows_[static_cast<size_t>(r)])];
    const gn::ServerInfo& in = e.info;
    const float y = kRowsTop + 8 * static_cast<float>(r - first_);
    cells_.push_back(pen.at(kListLeft, y, kListRight - kListLeft, 8));
    if (r == selected_) classic_cursor(pen, y, t);
    const std::string name = (in.locked ? "*" : "") + glyph_safe(printable(in.name));
    pen.text(fit_chars(name, kName), x, y, false);
    pen.text(fit_chars(glyph_safe(printable(in.map)), kMap), kMapX, y, true);
    const bool full = in.max_players > 0 && in.players >= in.max_players;
    text_right(pen, std::to_string(in.players) + "/" + std::to_string(in.max_players), kPlayersEnd,
               y, !full);
    text_right(pen, e.ping_ms < 0 ? "-" : std::to_string(std::min(e.ping_ms, 999)), kListRight, y,
               false);
  }
  if (n == 0) {
    const float y = kRowsTop + 16;
    pen.text(going ? "Looking for servers..." : "No servers found.", x, y, false);
    two_rows(pen,
             going                     ? "on the LAN and at the masters"
             : config_.masters.empty() ? "none on the LAN; name masters with sb_masters"
                                       : "none on the LAN or at the masters; R asks again",
             x, y + 12, kListChars, true);
  }

  // The foot: how the refresh stands, the selected server's address and players, the keys.
  const int answered = gn::server_list_answered(list_);
  const std::string state = std::to_string(answered) + (answered == 1 ? " server" : " servers") +
                            (going ? ", refreshing" : "");
  text_right(pen, state, kListRight, kRowsTop + 8 * kFits + 2, true);
  constexpr float kNoteY = 168;
  const gn::ServerEntry* e =
      selected_ < n ? &list_.entries[static_cast<size_t>(rows_[static_cast<size_t>(selected_)])]
                    : nullptr;
  if (!note_.empty()) {
    two_rows(pen, note_, kFootLeft, kNoteY, kFootChars, false);
  } else if (e != nullptr) {
    std::string facts = gn::to_string(e->address);
    if (!e->info.mode.empty()) facts += "  " + lower(printable(e->info.mode));
    pen.text(fit_chars(glyph_safe(facts), kFootChars), kFootLeft, kNoteY, true);
    std::string who;
    for (const gn::ServerPlayer& p : e->info.roster) {
      if (p.spectator) continue;
      if (!who.empty()) who += ", ";
      who += printable(p.name) + " " + std::to_string(p.frags);
    }
    pen.text(fit_chars(glyph_safe(who.empty() ? "nobody playing" : who), kFootChars), kFootLeft,
             kNoteY + 10, false);
  }
  pen.text("enter joins  r again  esc back", kFootLeft, 190, true);
}

}  // namespace shooters_edge::menu
