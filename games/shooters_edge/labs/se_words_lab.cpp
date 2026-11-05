// THE SE-WORDS LAB: what a player reads from the server, in Shooter's Edge's own words and in a
// loaded classic game's (profiles/words.hpp), side by side, drawn in a console font the way the
// classic client draws them: the first half of the glyph sheet white, the second (red text) brown,
// the gold figures and brackets gold.
//
// - THE LOBBY: two players join, get ready, one breaks the countdown, both get ready again, the
//   countdown runs out and the match begins; the lobby's lines without a game's words on the left,
//   with the classic game's on the right (server/announce.hpp).
// - THE COUNTDOWN: the classic game's centre print at 10 s for a four-a-side team game (deathmatch
//   1, teamplay 2, timelimit 20, antilag 2), drawn in the middle of the screen.
// - THE MATCH: the baseline mod's two bots play two minutes on the built-in arena, once in the
//   mod's words and once with the classic game's (the item names and the server words as local
//   info): the last lines of each, the deaths, the clock and the end.
//
// Metrics: the deaths and how many the classic words told, the lines left in the mod's own words
// with the classic words on (0), the countdown print's bytes.
//
// ge:summary The se-words lab: the server's messages in our words and the classic game's, drawn
// ge:keywords lab se-words words classic-words obituary countdown lobby clock console-font
// ge:keywords side-by-side redtext gold-digits
// ge:skill shooter labs
#include <algorithm>
#include <string>
#include <vector>

#include "charset/charset.hpp"
#include "core/num_text.hpp"
#include "hud/face.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/client/demo_caption.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/server/announce.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/words.hpp"
#include "ui/raster.hpp"

namespace {

namespace gl = ge::lab;
namespace ui = ge::ui;
namespace sv = shooters_edge::server;
namespace pf = shooters_edge::profiles;
namespace cl = shooters_edge::client;

// A console glyph sheet (16 x 16 cells of 8 x 8) from the engine's charset: 33..126 white, the
// same letters at 161..254 brown (red text), the gold figures at 146..155 and 18..27, the gold
// brackets at 144 and 145 (and 16, 17), the centred dot at 143, the rule's ends and middle at 157,
// 158 and 159.
ui::Image console_sheet() {
  const ge::charset::Atlas atlas = ge::charset::build_atlas(ge::charset::FontId::terminal, 128);
  ui::Image img;
  img.width = img.height = 128;
  img.rgba.assign(size_t{128} * 128 * 4, 0);
  const auto put = [&img](int code, int x, int y, uint32_t rgb) {
    const size_t at = ((static_cast<size_t>((code >> 4) * 8 + y) * 128) +
                       static_cast<size_t>((code & 15) * 8 + x)) *
                      4;
    img.rgba[at] = static_cast<uint8_t>(rgb >> 16);
    img.rgba[at + 1] = static_cast<uint8_t>(rgb >> 8);
    img.rgba[at + 2] = static_cast<uint8_t>(rgb);
    img.rgba[at + 3] = 255;
  };
  const auto glyph = [&](int code, int from, uint32_t rgb) {
    const ge::charset::CellRect c = ge::charset::cell_rect(128, static_cast<uint8_t>(from));
    for (int y = 0; y < 8; y++)
      for (int x = 0; x < 8; x++)
        if (std::max(atlas.at(c.x + x, c.y + 2 * y), atlas.at(c.x + x, c.y + 2 * y + 1)) >= 128)
          put(code, x, y, rgb);
  };
  constexpr uint32_t kWhite = 0xd8d0c4, kBrown = 0xb0703c, kGold = 0xf0c048;
  for (int code = 33; code < 127; code++) {
    glyph(code, code, kWhite);
    glyph(code + 128, code, kBrown);
  }
  for (int d = 0; d < 10; d++) {
    glyph(146 + d, '0' + d, kGold);
    glyph(18 + d, '0' + d, kGold);
  }
  glyph(144, '[', kGold);
  glyph(145, ']', kGold);
  glyph(16, '[', kGold);
  glyph(17, ']', kGold);
  for (int y = 3; y < 5; y++)
    for (int x = 3; x < 5; x++) put(143, x, y, kWhite);
  for (int x = 0; x < 8; x++) {
    put(158, x, 3, kBrown);
    put(158, x, 4, kBrown);
    if (x >= 3) put(157, x, 3, kBrown), put(157, x, 4, kBrown);
    if (x <= 4) put(159, x, 3, kBrown), put(159, x, 4, kBrown);
  }
  return img;
}

sv::Info classic_info() {
  sv::Info info;
  for (const pf::WordRow& w : pf::kClassicServerWords)
    info.emplace_back(std::string(w.key), std::string(w.text));
  return info;
}

// The text split into its lines (a trailing newline makes no empty last line).
std::vector<std::string> split(const std::string& text) {
  std::vector<std::string> lines(1);
  for (char c : text) {
    if (c == '\n')
      lines.emplace_back();
    else
      lines.back() += c;
  }
  if (lines.back().empty()) lines.pop_back();
  return lines;
}

// One column of lines over a dark ground with its heading, `w` x `h`.
ui::Image column(const ui::Image& sheet, const std::string& heading,
                 const std::vector<std::string>& lines, int w, int h, const std::string& centre) {
  ui::DrawList dl;
  dl.begin(w, h);
  std::vector<cl::CaptionLine> top = {{heading, true}};
  std::vector<cl::CaptionLine> body;
  for (const std::string& l : lines) body.push_back({l, false});
  cl::draw_caption(dl, sheet, w, h, top, body);
  ui::Image img = ui::make_image(w, h, ui::hex(0x15130f));
  ui::render(dl, img);
  if (centre.empty()) return img;
  // The centre print as the screen shows it, in the column's lower half under the lines.
  ui::DrawList cd;
  cd.begin(w, h);
  cl::draw_centre(cd, sheet, w, h, centre);
  ui::Image c = ui::make_image(w, h, ui::hex(0x1f1b16));
  ui::render(cd, c);
  const int from = h / 6, to = h / 2;
  const size_t row = static_cast<size_t>(w) * 4;
  std::copy_n(
      c.rgba.begin() + static_cast<std::ptrdiff_t>(from) * static_cast<std::ptrdiff_t>(row),
      static_cast<size_t>(h - to) * row,
      img.rgba.begin() + static_cast<std::ptrdiff_t>(to) * static_cast<std::ptrdiff_t>(row));
  return img;
}

// Two columns side by side with a seam between.
gl::Picture side_by_side(const ui::Image& a, const ui::Image& b) {
  gl::Picture p;
  p.width = a.width + b.width + 4;
  p.height = std::max(a.height, b.height);
  p.rgba.assign(static_cast<size_t>(p.width) * static_cast<size_t>(p.height) * 4, 60);
  const auto blit = [&p](const ui::Image& img, int x0) {
    for (int y = 0; y < img.height; y++)
      std::copy_n(img.rgba.begin() + static_cast<std::ptrdiff_t>(y) * img.width * 4,
                  static_cast<size_t>(img.width) * 4,
                  p.rgba.begin() + (static_cast<std::ptrdiff_t>(y) * p.width + x0) * 4);
  };
  blit(a, 0);
  blit(b, a.width + 4);
  for (size_t i = 3; i < p.rgba.size(); i += 4) p.rgba[i] = 255;
  return p;
}

// The lobby's script, its lines in `info`'s words (empty: the lobby's own).
std::vector<std::string> lobby_script(const sv::Info& info, std::string* centre) {
  sv::Lobby l;
  l.settings.countdown_s = 10;
  sv::Announcer a;
  const sv::MatchLook look{.deathmatch = 3, .timelimit = 10};
  std::vector<std::string> lines;
  const auto take = [&](double now) {
    sv::announce(a, l, info, look, now);
    for (const sv::NetEvent& ev : a.pending) {
      if (ev.kind == sv::EventKind::print && (ev.to & 1u) != 0)
        for (std::string& s : split(ev.text)) lines.push_back(std::move(s));
      if (ev.kind == sv::EventKind::centerprint && !ev.text.empty() && centre->empty())
        *centre = ev.text;
    }
    a.pending.clear();
  };
  sv::lobby_join(l, 0, "ann", false, 0);
  sv::lobby_join(l, 1, "bob", false, 0);
  take(0);
  sv::lobby_ready(l, 0, true, 1);
  take(1);
  sv::lobby_command(l, 0, "ready", 2);
  take(2);
  sv::lobby_ready(l, 1, true, 3);
  take(3);
  sv::lobby_ready(l, 1, false, 5);
  take(5);
  sv::lobby_ready(l, 1, true, 6);
  take(6);
  for (double t = 7; t <= 16.5; t += 0.5) {
    if (sv::lobby_due(l, t)) {
      take(t);
      break;
    }
    take(t);
  }
  return lines;
}

const ge::progs::Progs* baseline(gl::Lab& lab) {
  static ge::progs::Progs progs;
  static bool built = false, ok = false;
  if (!built) {
    built = true;
    std::string error;
    ok = sv::compile_baseline_mod(lab.arg("root"), progs, error);
    if (!ok)
      lab.fail("se-words: the baseline mod did not compile: " + error,
               "next: build it with `ge qc build games/shooters_edge/qc/arena/progs.src`");
  }
  return ok ? &progs : nullptr;
}

sv::ModMatchReport match(const ge::progs::Progs& progs, bool classic, double minutes) {
  sv::ModMatchOptions o;
  o.compiled = &progs;
  o.compiled_name = "the baseline mod";
  o.entities = sv::kBaselineArenaEntities;
  o.bots = 2;
  o.max_frames = static_cast<int>(minutes * 60 / 0.013) + 500;
  if (classic) {
    o.console = pf::item_name_lines(pf::kClassicNames);
    for (std::string& line : pf::word_lines(pf::kClassicServerWords)) o.console.push_back(line);
  }
  o.console.push_back("deathmatch 1");
  o.console.push_back("timelimit " + ge::core::num::to_string(minutes));
  return sv::run_mod_match(o);
}

std::vector<std::string> lines_of(const sv::ModMatchReport& r) {
  std::string all;
  for (const std::string& p : r.prints) all += p;
  return split(all);
}

void run(gl::Lab& lab) {
  const ui::Image sheet = console_sheet();
  const int w = 960, h = 1080;
  const sv::Info classic = classic_info();

  // The lobby.
  std::string own_centre, classic_centre;
  const std::vector<std::string> own_lobby = lobby_script({}, &own_centre);
  const std::vector<std::string> classic_lobby = lobby_script(classic, &classic_centre);
  lab.shot({"lobby",
            "THE LOBBY: OUR WORDS AND THE CLASSIC GAME'S",
            "Two players join, get ready, one breaks the countdown, both get ready again and the "
            "match begins. Left: the lobby's own lines (the banner counts down). Right: the "
            "classic game's, with its countdown's first centre print.",
            "the right: names plain, the rest of a ready line brown, Timer started, the count "
            "in gold figures in the middle, The match has begun! in brown",
            {{"players", "2"}, {"countdown", "10 s"}}},
           side_by_side(column(sheet, "SHOOTER'S EDGE'S OWN", own_lobby, w, h, ""),
                        column(sheet, "THE CLASSIC GAME'S", classic_lobby, w, h, classic_centre)));

  // The countdown of a four-a-side team game.
  {
    sv::Lobby l;
    l.settings.countdown_s = 10;
    for (int i = 0; i < 8; i++) sv::lobby_join(l, i, "p" + std::to_string(i), false, 0);
    for (int i = 0; i < 8; i++) sv::lobby_ready(l, i, true, 0);
    sv::Announcer a;
    sv::announce(a, l, classic, {.deathmatch = 1, .teamplay = 2, .timelimit = 20, .antilag = 2}, 0);
    std::string centre;
    for (const sv::NetEvent& ev : a.pending)
      if (ev.kind == sv::EventKind::centerprint) centre = ev.text;
    lab.metric("countdown_bytes", static_cast<double>(centre.size()), "bytes");
    gl::Picture p;
    ui::Image img = column(sheet, "THE COUNTDOWN, 4 ON 4", {}, 1280, 720, centre);
    p.width = img.width, p.height = img.height, p.rgba = std::move(img.rgba);
    lab.shot({"countdown",
              "THE CLASSIC COUNTDOWN AT 10 S, FOUR A SIDE",
              "The centre print every client gets each second of the countdown: the seconds in "
              "gold, the match's settings a line each, values right-aligned.",
              "Countdown in brown, 10 in gold, Mode T e a m, the numbers lined up on the right",
              {{"deathmatch", "1"}, {"teamplay", "2"}, {"timelimit", "20"}, {"antilag", "2"}}},
             p);
  }

  // The match.
  const ge::progs::Progs* progs = baseline(lab);
  if (progs == nullptr) return;
  const double minutes = lab.arg_num("minutes");
  const sv::ModMatchReport own = match(*progs, false, minutes);
  const sv::ModMatchReport theirs = match(*progs, true, minutes);
  if (!own.ok || !theirs.ok) {
    lab.fail("se-words: a match did not run: " + own.error + theirs.error,
             "next: run tests/shooters_edge_words_test.cpp");
    return;
  }
  const std::vector<std::string> a = lines_of(own), b = lines_of(theirs);
  int deaths = 0;
  for (const sv::PlayerPath& p : theirs.players) deaths += p.deaths;
  int own_words = 0;
  for (const std::string& l : b)
    own_words += l.find(" was shot by ") != std::string::npos ||
                 l.find("fragged themselves") != std::string::npos || l == "Timelimit hit.";
  lab.metric("deaths", deaths, "deaths");
  lab.metric("lines", static_cast<double>(b.size()), "lines");
  lab.metric("own_words_left", own_words, "lines");
  if (own_words != 0)
    lab.fail("se-words: " + std::to_string(own_words) + " lines in the mod's own words",
             "next: give each a key in profiles/words.hpp and say it with game_words");
  const auto tail = [](const std::vector<std::string>& v) {
    const size_t keep = 32;
    return v.size() <= keep ? v : std::vector<std::string>(v.end() - keep, v.end());
  };
  lab.shot({"match",
            "A BOT MATCH: OUR WORDS AND THE CLASSIC GAME'S",
            "The baseline mod's two bots on the arena, the match's last lines. Left: the mod's "
            "own words. Right: the same mod with the classic game's words as local info.",
            "the right: every death in the classic words, the clock's gold figures in gold "
            "brackets, The match is over",
            {{"bots", "2"}, {"timelimit", ge::core::num::to_string(minutes) + " min"}}},
           side_by_side(column(sheet, "SHOOTER'S EDGE'S OWN", tail(a), w, h, ""),
                        column(sheet, "THE CLASSIC GAME'S", tail(b), w, h, "")));
  gl::LabTable t{"the classic match's lines", {"line"}, {}};
  for (const std::string& l : b) t.rows.push_back({cl::plain_text(l)});
  lab.table(std::move(t));
}

const gl::LabSpec kSpec{
    "se-words",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "games/shooters_edge/server/words, announce and profiles/words: a loaded classic game's "
    "server words in the baseline mod and the lobby",
    "shooter",
    "Does a player with a classic game loaded read its server's words (obituaries, the lobby, "
    "the countdown, the clock, the end) in its colours, while Shooter's Edge keeps its own?",
    {{"minutes", "2", "the bot match's time limit, minutes"},
     {"root", ".", "the repository's root (the mod's sources are under it)"}},
    "--minutes=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
