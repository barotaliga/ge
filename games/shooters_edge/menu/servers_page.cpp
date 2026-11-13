// The server browser page: the list's config from the cvars, the keys, the table drawn
// (servers_page.hpp).
//
// ge:summary The server browser page's refresh, keys, table, footer and empty states
// ge:keywords server-browser servers page table rows footer sort refresh join draw
// ge:skill ui net
// ge:test tests/shooters_edge_menu_test.cpp
#include "shooters_edge/menu/servers_page.hpp"

#include <algorithm>
#include <string_view>

#include "console/console.hpp"
#include "ui/font.hpp"

namespace shooters_edge::menu {

namespace {

namespace ui = ge::ui;
namespace gn = ge::net;

constexpr std::string_view kTitle = "SERVERS";
constexpr std::string_view kHelp = "ENTER JOIN   LEFT/RIGHT SORT   R REFRESH   ESC BACK";
constexpr std::string_view kSep = "  \xc2\xb7  ";  // a middle dot

std::vector<std::string> split_words(const std::string& text) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at < text.size()) {
    while (at < text.size() && (text[at] == ' ' || text[at] == ',' || text[at] == ';')) at++;
    size_t end = at;
    while (end < text.size() && text[end] != ' ' && text[end] != ',' && text[end] != ';') end++;
    if (end > at) out.push_back(text.substr(at, end - at));
    at = end;
  }
  return out;
}

std::string upper(std::string s) {
  for (char& c : s) c = static_cast<char>(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
  return s;
}

std::string fit(const ui::Font& font, std::string_view text, float size, float width) {
  const size_t n = font.fit(text, size, width);
  if (n >= text.size()) return std::string(text);
  const size_t m = font.fit(text, size, std::max(0.0f, width - font.advance("...", size)));
  return std::string(text.substr(0, m)) + "...";
}

// Text a server sent, made printable: the classic high-bit "gold" letters folded to plain ones,
// control bytes dropped.
std::string printable(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (char ch : s) {
    auto c = static_cast<unsigned char>(ch & 0x7f);
    if (c >= 0x12 && c <= 0x1b) c = static_cast<unsigned char>('0' + (c - 0x12));
    if (c < 0x20 || c == 0x7f) continue;
    out += static_cast<char>(c);
  }
  return out;
}

}  // namespace

ServersPage::~ServersPage() { gn::close_transport(own_); }

void ServersPage::add_cvars(ge::console::Console& con) {
  con.cvars().bind({.name = "sb_lan",
                    .fallback = "1",
                    .flags = ge::console::kArchive,
                    .help = "Whether JOIN A GAME looks for servers on the LAN, by a broadcast.",
                    .kind = ge::console::CvarKind::boolean,
                    .values = "0: no; 1: yes (the default)"},
                   &lan_);
  con.cvars().bind(
      {.name = "sb_masters",
       .fallback = "",
       .flags = ge::console::kArchive,
       .help = "The masters JOIN A GAME asks for their servers, beside those the loaded game's own "
               "server lists name.",
       .kind = ge::console::CvarKind::text,
       .values = "\"\": none of your own (the default); TEXT: host:port words with spaces between"},
      &masters_);
  con.cvars().bind(
      {.name = "sb_servers",
       .fallback = "",
       .flags = ge::console::kArchive,
       .help = "The servers JOIN A GAME always asks, your favourites.",
       .kind = ge::console::CvarKind::text,
       .values = "\"\": none (the default); TEXT: host:port words with spaces between"},
      &servers_);
}

bool ServersPage::open(std::string* why) {
  note_.clear();
  if (!has_query()) {
    note_ = "this game names no server protocol";
    if (why) *why = note_;
    return false;
  }
  gn::Transport* t = borrowed_;
  if (t == nullptr) {
    if (own_.kind == gn::TransportKind::none) {
      std::string error;
      if (!gn::open_udp_transport(own_, gn::ipv4(0, 0, 0, 0, 0), &error)) {
        note_ = "the browser's socket did not open: " + error +
                ". Check that the computer is on a network.";
        if (why) *why = note_;
        return false;
      }
    }
    t = &own_;
  }
  config_ = gn::ServerListConfig{};
  if (lan_) {
    for (uint32_t p = lan_first_; p <= lan_last_; p++)
      config_.lan.push_back(borrowed_ != nullptr
                                ? gn::sim_broadcast(static_cast<uint16_t>(p))
                                : gn::ipv4(255, 255, 255, 255, static_cast<uint16_t>(p)));
  }
  // Names resolved once here, when the player opens the page; a name that does not resolve is
  // left out and said in the footer.
  std::vector<std::string> masters = split_words(masters_);
  masters.insert(masters.end(), install_masters_.begin(), install_masters_.end());
  auto resolve = [&](const std::string& text, uint16_t port, std::vector<gn::Address>& into) {
    gn::Address a;
    if (gn::parse_address(text, a)) {
      // already an address (an IPv4 or a simulated node)
    } else if (std::string error; !gn::resolve_ipv4(text, port, a, &error)) {
      if (note_.empty()) note_ = error;
      return;
    }
    if (std::find(into.begin(), into.end(), a) == into.end()) into.push_back(a);
  };
  for (const std::string& m : masters) resolve(m, 27000, config_.masters);
  for (const std::string& s : split_words(servers_)) resolve(s, lan_first_, config_.servers);
  if (!inited_ || list_.transport != t) gn::server_list_init(list_, config_, query_, *t);
  list_.config = config_;
  inited_ = true;
  refresh();
  return true;
}

void ServersPage::refresh() {
  gn::server_list_refresh(list_, now_us_);
  selected_ = 0;
  first_ = 0;
  build_rows();
}

void ServersPage::frame(double dt) {
  carry_us_ += dt * 1e6;
  const auto whole = static_cast<int64_t>(carry_us_);
  carry_us_ -= static_cast<double>(whole);
  now_us_ += whole;
  if (!inited_) return;
  gn::server_list_update(list_, now_us_);
  // The selection stays on its server as the rows sort themselves around it.
  const int was = selected_ >= 0 && selected_ < static_cast<int>(rows_.size())
                      ? rows_[static_cast<size_t>(selected_)]
                      : -1;
  build_rows();
  if (was >= 0) {
    const auto it = std::find(rows_.begin(), rows_.end(), was);
    if (it != rows_.end()) selected_ = static_cast<int>(it - rows_.begin());
  }
}

void ServersPage::build_rows() {
  gn::ServerFilter f;
  gn::server_list_view(list_, f, sort_, rows_);
  if (selected_ >= static_cast<int>(rows_.size()))
    selected_ = std::max(0, static_cast<int>(rows_.size()) - 1);
}

int ServersPage::hit(float x, float y) const {
  for (size_t i = 0; i < cells_.size(); i++)
    if (cells_[i].contains(x, y)) return first_ + static_cast<int>(i);
  return -1;
}

ServersAction ServersPage::input(const ui::MenuInput& in, bool refresh_key) {
  if (in.back) return ServersAction::leave;
  if (refresh_key && inited_) {
    refresh();
    return ServersAction::none;
  }
  const int n = static_cast<int>(rows_.size());
  if (in.up && selected_ > 0) selected_--;
  if (in.down && selected_ + 1 < n) selected_++;
  if (in.left || in.right) {
    const int k = (static_cast<int>(sort_) + (in.right ? 1 : 4)) % 5;
    sort_ = static_cast<gn::ServerSort>(k);
    const int was = selected_ < n ? rows_[static_cast<size_t>(selected_)] : -1;
    build_rows();
    const auto it = std::find(rows_.begin(), rows_.end(), was);
    selected_ = it != rows_.end() ? static_cast<int>(it - rows_.begin()) : 0;
  }
  bool join = in.accept;
  if (in.click || in.pointer_moved) {
    const int at = hit(in.pointer_x, in.pointer_y);
    if (at >= 0 && at < n) {
      selected_ = at;
      join = join || in.click;
    } else if (in.click) {
      join = false;
    }
  }
  if (join && selected_ < n) {
    chosen_ = gn::to_string(
        list_.entries[static_cast<size_t>(rows_[static_cast<size_t>(selected_)])].address);
    return ServersAction::join;
  }
  return ServersAction::none;
}

void ServersPage::draw(ui::DrawList& list, int width, int height, const ui::MenuStyle& style) {
  problems_.clear();
  cells_.clear();
  const float w = static_cast<float>(width), h = static_cast<float>(height);
  const float s = std::min(w, h) / 720.0f;
  const bool portrait = h > w;
  const ui::Font& bold = style.title_font ? *style.title_font : ui::ui_font(ui::Weight::semibold);
  const ui::Font& small = style.small_font ? *style.small_font : ui::ui_font(ui::Weight::medium);
  const float margin = (portrait ? 24.0f : 48.0f) * s;
  const float ts = std::max(s, 0.75f);
  const float title = style.title_size * ts, word = style.small_size * ts;
  const float line = word * 1.6f;
  const float inner = w - 2 * margin;
  list.rect({0, 0, w, h}, ui::hex(0x000000, 0.66f));

  // The title row: the page's name, then how the refresh stands.
  const float title_base = margin + title;
  list.text(bold, title, margin, title_base, kTitle, style.text, ui::Align::left, style.tracking);
  const int answered = gn::server_list_answered(list_);
  const int known = static_cast<int>(list_.entries.size());
  const bool going = inited_ && !gn::server_list_done(list_, now_us_);
  std::string where = std::to_string(answered) + (answered == 1 ? " SERVER" : " SERVERS");
  if (known > answered) where += " OF " + std::to_string(known) + " ASKED";
  where += std::string(kSep) +
           (going ? "REFRESHING" : "SORTED BY " + upper(gn::server_sort_name(sort_)));
  const float title_w = bold.advance(kTitle, title, style.tracking);
  if (title_w + small.advance(where, word) + 24 * s > inner)
    list.text(small, word, margin, title_base + line, where, style.text_dim);
  else
    list.text(small, word, w - margin, title_base, where, style.text_dim, ui::Align::right);

  // The columns: the name takes what the others leave.
  struct Col {
    std::string_view head;
    float width;  // at the reference, px; 0: the name's
    bool right;
  };
  const bool narrow = inner < 760 * s;
  const Col cols[] = {{"NAME", 0, false},  {"MODE", narrow ? 0.0f : 96, false},
                      {"MAP", 120, false}, {"PLAYERS", 104, true},
                      {"PING", 72, true},  {"SKILL", narrow ? 0.0f : 64, true}};
  constexpr size_t kCols = std::size(cols);
  float fixed = 0;
  for (size_t c = 1; c < kCols; c++) fixed += cols[c].width * s;
  float xs[kCols], ws[kCols];
  float x = margin + 12 * s;
  for (size_t c = 0; c < kCols; c++) {
    ws[c] = c == 0 ? std::max(80 * s, inner - 24 * s - fixed) : cols[c].width * s;
    xs[c] = x;
    x += ws[c];
  }
  const float head_base = title_base + (portrait ? 2.4f : 1.6f) * line;
  for (size_t c = 0; c < kCols; c++) {
    if (ws[c] <= 0) continue;
    const bool sorted =
        (c == 0 && sort_ == gn::ServerSort::name) || (c == 1 && sort_ == gn::ServerSort::mode) ||
        (c == 2 && sort_ == gn::ServerSort::map) || (c == 3 && sort_ == gn::ServerSort::players) ||
        (c == 4 && sort_ == gn::ServerSort::ping);
    const float tx = cols[c].right ? xs[c] + ws[c] - 8 * s : xs[c];
    list.text(bold, word * 0.85f, tx, head_base, cols[c].head,
              sorted ? style.accent_hi : style.text_dim,
              cols[c].right ? ui::Align::right : ui::Align::left, style.tracking);
  }

  // The footer: the selected server's facts and players, over the keys.
  const float help_base = h - margin;
  const float roster_base = help_base - line * 1.3f;
  const float facts_base = roster_base - line;
  list.text(small, word, margin, help_base, fit(small, kHelp, word, inner), style.text_dim);
  const gn::ServerEntry* e =
      selected_ < static_cast<int>(rows_.size())
          ? &list_.entries[static_cast<size_t>(rows_[static_cast<size_t>(selected_)])]
          : nullptr;
  if (!note_.empty()) {
    list.text(small, word, margin, facts_base, fit(small, note_, word, inner), style.accent_hi);
  } else if (e != nullptr) {
    std::string facts = gn::to_string(e->address);
    if (!e->info.game.empty()) facts += std::string(kSep) + upper(e->info.game);
    if (!e->info.state.empty()) facts += std::string(kSep) + upper(printable(e->info.state));
    if (e->info.max_spectators > 0 || e->info.spectators > 0)
      facts += std::string(kSep) + std::to_string(e->info.spectators) + "/" +
               std::to_string(e->info.max_spectators) + " WATCHING";
    if (e->from & gn::kServerFromLan) facts += std::string(kSep) + "LAN";
    list.text(small, word, margin, facts_base, fit(small, facts, word, inner), style.text_dim);
  }
  if (e != nullptr) {
    std::string who;
    for (const gn::ServerPlayer& p : e->info.roster) {
      if (p.spectator) continue;
      if (!who.empty()) who += ", ";
      who += printable(p.name);
      if (!p.team.empty()) who += " (" + printable(p.team) + ")";
      who += " " + std::to_string(p.frags);
    }
    if (who.empty()) who = "NOBODY PLAYING";
    list.text(small, word, margin, roster_base, fit(small, who, word, inner), style.text);
  }

  // The rows.
  const float top = head_base + line * 0.6f;
  const float bottom = facts_base - line * 1.2f;
  const float row_h = std::max(word * 1.9f, 34 * s);
  const int fits = std::max(1, static_cast<int>((bottom - top) / row_h));
  const int n = static_cast<int>(rows_.size());
  if (selected_ < first_) first_ = selected_;
  if (selected_ >= first_ + fits) first_ = selected_ - fits + 1;
  first_ = std::clamp(first_, 0, std::max(0, n - fits));
  if (n == 0) {
    const float cy = (top + bottom) * 0.5f;
    const std::string_view big = going ? "LOOKING FOR SERVERS" : "NO SERVERS FOUND";
    list.text(bold, title * 0.7f, w * 0.5f, cy, big, style.text, ui::Align::center, style.tracking);
    const std::string how =
        going ? "On the LAN and at the masters."
        : config_.masters.empty()
            ? "None answered on the LAN. Name masters with sb_masters, or servers with sb_servers."
            : "None answered on the LAN or at the masters. R asks again.";
    list.text(small, word, w * 0.5f, cy + line * 1.4f, fit(small, how, word, inner), style.text_dim,
              ui::Align::center);
    return;
  }
  for (int r = first_; r < n && r < first_ + fits; r++) {
    const gn::ServerEntry& se = list_.entries[static_cast<size_t>(rows_[static_cast<size_t>(r)])];
    const gn::ServerInfo& in = se.info;
    const ui::Rect cell{margin, top + static_cast<float>(r - first_) * row_h, inner, row_h - 4 * s};
    cells_.push_back(cell);
    const bool sel = r == selected_;
    list.rect(cell, sel ? style.accent : style.slab, style.radius * s);
    const float base = cell.y + cell.h * 0.5f + word * 0.36f;
    const bool full = in.max_players > 0 && in.players >= in.max_players;
    const ui::Color tc = sel ? style.text_focus : style.text;
    const ui::Color dim = sel ? style.text_focus : style.text_dim;
    const std::string name = (in.locked ? "* " : "") + printable(in.name);
    const std::string cells[kCols] = {
        name,
        upper(printable(in.mode)),
        printable(in.map),
        std::to_string(in.players) + " / " + std::to_string(in.max_players),
        se.ping_ms < 0 ? "-" : std::to_string(se.ping_ms),
        in.skill < 0 ? "-" : std::to_string(in.skill)};
    for (size_t c = 0; c < kCols; c++) {
      if (ws[c] <= 0) continue;
      const ui::Font& f = c == 0 ? bold : small;
      const std::string text = fit(f, cells[c], word, ws[c] - 12 * s);
      const float tx = cols[c].right ? xs[c] + ws[c] - 8 * s : xs[c];
      const ui::Color col = c == 0 ? tc : (c == 3 && full) ? style.accent_hi : dim;
      list.text(f, word, tx, base, text, sel && c == 3 && full ? tc : col,
                cols[c].right ? ui::Align::right : ui::Align::left);
    }
  }
  if (n > fits) {
    const std::string more = std::to_string(first_ + 1) + "-" +
                             std::to_string(std::min(n, first_ + fits)) + " OF " +
                             std::to_string(n);
    list.text(small, word * 0.85f, w - margin, bottom + line * 0.8f, more, style.text_dim,
              ui::Align::right);
  }
  if (small.advance(kHelp, word) > inner)
    problems_.push_back("the keys are wider than the screen at " + std::to_string(width) + "x" +
                        std::to_string(height));
}

}  // namespace shooters_edge::menu
