// LAB `console`: the drop-down console (engine/console/view.hpp) LOOKED AT, drawn on the CPU from
// the same model the game's 2D layer will draw: a scripted session (a server's messages, chat to
// all, the team and one player, a game event, commands typed and their output, a half-typed
// line) pictured down, half-way through its slide and closed with the notify lines over the game,
// then autocomplete: the list under the input line, a row selected, and Enter on a cvar waiting
// for its value. Each kind of line has its colour. The metrics count the lines by kind and the
// slide's frames.
//
// ge:summary Lab: the drop-down console pictured down, sliding and closed, lines coloured by kind
// ge:keywords lab console view drop-down picture scrollback chat notify slide tilde
// ge:keywords autocomplete suggestion hint
// ge:skill console labs
// ge:test tests/console_view_test.cpp
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "console/view.hpp"
#include "lab/lab.hpp"

namespace {

namespace con = ge::console;
using ge::lab::Picture;
using ge::lab::Rgba;

constexpr int kScale = 2;  // the 5x7 font at twice its size
constexpr int kRow = ge::lab::kGlyphH * kScale + 4;
constexpr int kMargin = 12;

Rgba colour(con::LineKind k) {
  switch (k) {
    case con::LineKind::output:
      return {200, 200, 200};
    case con::LineKind::command:
      return {255, 255, 255};
    case con::LineKind::chat:
      return {120, 230, 120};
    case con::LineKind::team:
      return {110, 210, 255};
    case con::LineKind::whisper:
      return {240, 140, 240};
    case con::LineKind::nearby:
      return {200, 230, 160};
    case con::LineKind::server:
      return {250, 220, 90};
    case con::LineKind::event:
      return {255, 160, 70};
  }
  return {200, 200, 200};
}

// A stand-in for the game behind: a sky over a snow slope with a few trees.
Picture backdrop(int w, int h) {
  Picture p = ge::lab::create_picture(w, h, {0, 0, 0});
  for (int y = 0; y < h; ++y) {
    const double t = static_cast<double>(y) / h;
    const auto c = Rgba{static_cast<uint8_t>(70 + 90 * t), static_cast<uint8_t>(120 + 80 * t),
                        static_cast<uint8_t>(200 + 40 * t)};
    ge::lab::fill_rect(p, 0, y, w, 1, c);
  }
  for (int x = 0; x < w; ++x) {
    const double top = h * 0.45 + 0.25 * x;
    ge::lab::fill_rect(p, x, top, 1, h - top, {236, 240, 248});
  }
  for (int k = 0; k < 9; ++k) {
    const double x = 90 + 130.0 * k, base = h * 0.45 + 0.25 * x + 40 + 9 * (k % 3);
    for (int r = 0; r < 40; ++r)
      ge::lab::fill_rect(p, x - r * 0.45, base - 60 + r * 1.4, r * 0.9, 2, {30, 70, 45});
  }
  return p;
}

// The suggestions dropped under the input line, over the game: each name, then a cvar's value
// (after =), an alias's body (after :) or a command's help, dim; the selected row lit.
void draw_suggestions(Picture& p, const con::ConsoleView& v, int cover) {
  const std::vector<con::Suggestion> list = v.suggestions();
  if (list.empty()) return;
  auto detail_of = [](const con::Suggestion& s) {
    return s.kind == con::Suggestion::Kind::cvar    ? "= " + s.detail
           : s.kind == con::Suggestion::Kind::alias ? ": " + s.detail
                                                    : s.detail;
  };
  int name_w = 0, detail_w = 0;
  for (const con::Suggestion& s : list) {
    name_w = std::max(name_w, ge::lab::text_width(s.name, kScale));
    detail_w = std::max(detail_w, ge::lab::text_width(detail_of(s), kScale));
  }
  const int x = kMargin + ge::lab::text_width("] /", kScale) - 6;
  const int box_w = std::min(p.width - x - kMargin, name_w + detail_w + 42);
  const int max_chars = (box_w - name_w - 36) / (ge::lab::kGlyphW * kScale);
  int y = cover + 2;
  ge::lab::fill_rect(p, x, y, box_w, static_cast<int>(list.size()) * kRow + 8, {16, 20, 28, 235});
  y += 4;
  for (size_t i = 0; i < list.size(); ++i) {
    const con::Suggestion& s = list[i];
    const bool lit = static_cast<int>(i) == v.selected();
    if (lit) ge::lab::fill_rect(p, x, y - 2, box_w, kRow, {60, 80, 120});
    ge::lab::draw_text(p, x + 6, y, s.name, lit ? Rgba{255, 255, 255} : Rgba{220, 225, 235},
                       kScale);
    std::string detail = detail_of(s);
    if (static_cast<int>(detail.size()) > max_chars && max_chars > 3)
      detail = detail.substr(0, static_cast<size_t>(max_chars - 3)) + "...";
    ge::lab::draw_text(p, x + 6 + name_w + 24, y, detail, {130, 140, 160}, kScale);
    y += kRow;
  }
}

// The console over `p` as the view stands: the panel, its lines, the input line and its cursor;
// closed, the notify lines.
void draw(Picture& p, const con::ConsoleView& v) {
  const int w = p.width;
  const int cover = static_cast<int>(v.cover() * p.height);
  if (cover > 0) {
    const int rows = std::max(1, (cover - kRow - 2 * kMargin) / kRow);
    // The background: con_bgtop to con_bgbottom down the panel at con_bgalpha (no image here).
    const con::Background bg = v.background();
    const auto a = static_cast<uint8_t>(std::lround(bg.alpha * 255));
    for (int y = 0; y < cover; ++y) {
      const double t = cover > 1 ? static_cast<double>(y) / (cover - 1) : 0;
      auto mix = [t](uint8_t top, uint8_t bottom) {
        return static_cast<uint8_t>(std::lround(top + (bottom - top) * t));
      };
      ge::lab::fill_rect(
          p, 0, y, w, 1,
          {mix(bg.top.r, bg.bottom.r), mix(bg.top.g, bg.bottom.g), mix(bg.top.b, bg.bottom.b), a});
    }
    ge::lab::fill_rect(p, 0, cover - 2, w, 2, {250, 220, 90});
    const auto page = v.page(rows);
    // The newest line sits just over the input line; the page grows up from it.
    int y = cover - kMargin - 2 * kRow - static_cast<int>(page.size() - 1) * kRow;
    for (const con::ViewLine* l : page) {
      ge::lab::draw_text(p, kMargin, y, l->text, colour(l->kind), kScale);
      y += kRow;
    }
    const std::string shown = "] " + v.input();
    const int iy = cover - kMargin - kRow;
    ge::lab::draw_text(p, kMargin, iy, shown, {255, 255, 255}, kScale);
    const int cx = kMargin + ge::lab::text_width("] " + v.input().substr(0, v.cursor()), kScale);
    // The hint: the rest of the name Tab would take, dim after the typed text.
    const std::string hint = v.hint();
    if (!hint.empty()) {
      ge::lab::draw_text(p, kMargin + ge::lab::text_width(shown, kScale), iy, hint, {120, 130, 150},
                         kScale);
    }
    ge::lab::fill_rect(p, cx, iy - 1, 2 * kScale, ge::lab::kGlyphH * kScale, {255, 255, 255});
    if (v.scroll() > 0) ge::lab::draw_text(p, w - 200, iy, "^ ^ ^ ^", {250, 220, 90}, kScale);
    draw_suggestions(p, v, cover);
    return;
  }
  int y = kMargin;
  for (const con::ViewLine* l : v.notify()) {
    ge::lab::fill_rect(p, kMargin - 4, y - 3, ge::lab::text_width(l->text, kScale) + 8, kRow,
                       {16, 20, 28, 140});
    ge::lab::draw_text(p, kMargin, y, l->text, colour(l->kind), kScale);
    y += kRow;
  }
}

// The scripted session: what a multiplayer evening's console holds.
void play_session(con::Console& c, con::ConsoleView& v) {
  c.cvars().add({.name = "r_bloom",
                 .fallback = "0.04",
                 .flags = con::kArchive,
                 .help = "the share of the glow round bright light"});
  c.cvars().add({.name = "sv_maxclients",
                 .fallback = "16",
                 .flags = con::kServer | con::kServerInfo,
                 .help = "the most players the server takes"});
  for (const auto& [name, value] :
       {std::pair{"sv_deathmatch", "0"}, std::pair{"sv_maxspeed", "320"},
        std::pair{"sv_mapcycle", "maps.cfg"}, std::pair{"sv_hostname", "north bowl"}})
    c.cvars().add({.name = name,
                   .fallback = value,
                   .flags = con::kServer | con::kServerInfo,
                   .kind = value[0] >= 'a' ? con::CvarKind::text : con::CvarKind::number});
  c.execute("alias sv_dm \"exec dm.cfg\"");
  c.execute("name Niclas; con_chat 1; con_bgtop 22304a; con_bgbottom 0a0d14; con_bgalpha 0.88");
  auto line = [&](con::LineKind k, const char* text) {
    v.frame(0.5);
    v.add(k, text);
  };
  line(con::LineKind::server, "hosting on lan:27500 (free ride, 6 of 16)");
  line(con::LineKind::server, "Ada entered the game");
  line(con::LineKind::chat, "Ada: anyone up for the north bowl?");
  v.type("on my way, meet at the lift top");
  v.key(con::EditKey::enter);
  line(con::LineKind::team, "(Bo): powder under the cliffs, go left");
  line(con::LineKind::whisper, "[from Ada]: race you down");
  c.execute("whisper Ada you are on");
  c.execute("say_prox wait for me at the cornice");
  line(con::LineKind::event, "gate 12 missed: +2 s");
  v.type("/r_bloom 0.1");
  v.key(con::EditKey::enter);
  c.run();
  v.type("/help r_bloom");
  v.key(con::EditKey::enter);
  c.run();
  for (const char* typed : {"/sv_deathmatch 1", "/sv_deathmatch 1", "/sv_maxclients 12"}) {
    v.type(typed);
    v.key(con::EditKey::enter);
  }
  c.run();
  line(con::LineKind::server, "Bo left the game");
  line(con::LineKind::event, "a record: 1:42.37 on seed 38");
  v.type("/sv_dea");
}

void run(ge::lab::Lab& lab) {
  const int w = static_cast<int>(lab.arg_num("width"));
  const int h = static_cast<int>(lab.arg_num("height"));
  con::Console c;  // a listen game: the host sets the server's cvars
  con::ConsoleView v(c);
  play_session(c, v);

  // Down: the toggle, then frames until it rests.
  c.key("`", true);
  c.run();
  int frames = 0;
  while (v.drop() < 1 && frames < 1000) {
    v.frame(1.0 / 60);
    ++frames;
  }
  lab.metric("slide.frames_at_60hz", frames, "frames");
  Picture down = backdrop(w, h);
  draw(down, v);
  const std::vector<std::pair<std::string, std::string>> inputs = {
      {"size", std::to_string(w) + "x" + std::to_string(h)},
      {"con_height", "0.5"},
      {"con_speed", "3"},
      {"con_bg", "22304a to 0a0d14 at 0.88"}};
  lab.shot({"down", "THE CONSOLE DOWN",
            "The tilde pressed: the console covers con_height of the screen over the game on a "
            "gradient from con_bgtop to con_bgbottom, its scrollback above the input line, which "
            "holds a half-typed /sv_dea: Tab would take the dim rest of sv_deathmatch, the one "
            "listed under it.",
            "server lines yellow, chat green, team cyan, nearby pale green, whispers magenta, "
            "events orange, typed "
            "commands white after ], output grey; the newest line just over the input line",
            inputs},
           down);

  // Half-way through closing.
  v.toggle();
  v.frame(0.5 / 3 * 0.5);
  Picture mid = backdrop(w, h);
  draw(mid, v);
  lab.shot({"sliding", "THE CONSOLE SLIDING UP",
            "Escape or the tilde again: the panel slides up at con_speed screens a second; here "
            "half-way.",
            "the panel's bottom edge at about a quarter of the screen, the lines moving with it",
            inputs},
           mid);

  // Closed: a chat line and an event arrive, shown as the notify lines.
  for (int k = 0; k < 300; ++k) v.frame(1.0 / 60);
  v.add(con::LineKind::chat, "Ada: see you at the bottom");
  v.add(con::LineKind::event, "gate 14 taken, 0.8 s up");
  Picture closed = backdrop(w, h);
  draw(closed, v);
  lab.shot({"closed", "CLOSED: THE NOTIFY LINES",
            "The console up: the newest lines (at most con_notifylines, younger than "
            "con_notifytime) show at the top of the game.",
            "two lines at the top left over a dark band, nothing else of the console", inputs},
           closed);

  // Open again: the suggestions while a name is typed, the most submitted first.
  v.toggle();
  for (int k = 0; k < 60; ++k) v.frame(1.0 / 60);
  for (int k = 0; k < 4; ++k) v.key(con::EditKey::backspace);  // "/sv_dea" -> "/sv"
  v.type("_");
  const std::vector<std::pair<std::string, std::string>> typed = {
      {"typed", "/sv_"}, {"submitted", "sv_deathmatch twice, sv_maxclients once"}};
  Picture suggest = backdrop(w, h);
  draw(suggest, v);
  lab.shot({"suggest", "AUTOCOMPLETE: THE LIST",
            "/sv_ typed: every cvar, alias and command it starts is listed under the input line, "
            "the most submitted first (then the shortest), at most con_suggest of them, and the "
            "top one's rest is the dim hint Tab takes.",
            "sv_deathmatch first (2 uses), sv_maxclients second (1), then the rest shortest "
            "first; cvars show their value after =, the alias its body after :",
            typed},
           suggest);
  lab.metric("suggest.count", static_cast<double>(v.suggestions().size()), "names");

  v.key(con::EditKey::down);
  v.key(con::EditKey::down);
  Picture select = backdrop(w, h);
  draw(select, v);
  lab.shot({"select", "AUTOCOMPLETE: DOWN TWICE",
            "Down moves through the list and the hint follows the selected row; Enter on a cvar "
            "fills in its name for the value, Enter on a command or an alias runs it.",
            "the second row lit, the hint after /sv_ now its rest", typed},
           select);
  v.key(con::EditKey::enter);
  Picture value = backdrop(w, h);
  draw(value, v);
  lab.shot({"value", "AUTOCOMPLETE: ENTER ON A CVAR",
            "Enter on the selected cvar: its name is in the input line with a blank after it, "
            "waiting for the value; the list is gone.",
            "the input line reads ] /<name> with the cursor after the blank", typed},
           value);
  if (v.input().empty() || v.input().back() != ' ') {
    lab.fail("Enter on a selected cvar did not fill in its name for a value",
             "run tests/console_view_test.cpp; skill: console");
  }

  int counts[8] = {};
  for (const con::ViewLine& l : v.lines()) ++counts[static_cast<int>(l.kind)];
  for (int k = 0; k < 8; ++k)
    lab.metric("lines." + std::string(con::line_kind_name(static_cast<con::LineKind>(k))),
               counts[k], "lines");
  if (counts[static_cast<int>(con::LineKind::whisper)] != 2) {
    lab.fail("the session's whispers did not both reach the scrollback",
             "run tests/console_view_test.cpp; skill: console");
  }
}

const ge::lab::LabSpec kSpec{
    "console",
    ge::lab::LabKind::picture,
    "engine",
    "engine/console/view.hpp",
    "console",
    "What does the drop-down console look like down, sliding and closed, with every kind of line?",
    {
        {"width", "1280", "the picture's width, px"},
        {"height", "720", "the picture's height, px"},
    },
    "--width=640 --height=360",
    run,
};

GE_LAB(kSpec);

}  // namespace
