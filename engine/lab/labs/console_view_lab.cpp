// LAB `console`: the drop-down console (engine/console/view.hpp) LOOKED AT, drawn on the CPU from
// the same model the game's 2D layer will draw: a scripted session (a server's messages, chat to
// all, the team and one player, a game event, commands typed and their output, a half-typed
// line) pictured down, half-way through its slide and closed with the notify lines over the game.
// Each kind of line has its colour. The metrics count the lines by kind and the slide's frames.
//
// ge:summary Lab: the drop-down console pictured down, sliding and closed, lines coloured by kind
// ge:keywords lab console view drop-down picture scrollback chat notify slide tilde
// ge:skill console labs
// ge:test tests/console_view_test.cpp
#include <algorithm>
#include <string>

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

// The console over `p` as the view stands: the panel, its lines, the input line and its cursor;
// closed, the notify lines.
void draw(Picture& p, const con::ConsoleView& v) {
  const int w = p.width;
  const int cover = static_cast<int>(v.cover() * p.height);
  if (cover > 0) {
    const int rows = std::max(1, (cover - kRow - 2 * kMargin) / kRow);
    ge::lab::fill_rect(p, 0, 0, w, cover, {16, 20, 28, 215});
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
    ge::lab::fill_rect(p, cx, iy - 1, 2 * kScale, ge::lab::kGlyphH * kScale, {255, 255, 255});
    if (v.scroll() > 0) ge::lab::draw_text(p, w - 200, iy, "^ ^ ^ ^", {250, 220, 90}, kScale);
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
  c.cvars().add(
      {.name = "sv_maxclients", .fallback = "16", .flags = con::kServer | con::kServerInfo});
  c.execute("name Niclas; con_chat 1");
  auto line = [&](con::LineKind k, const char* text) {
    v.frame(0.5);
    v.add(k, text);
  };
  line(con::LineKind::server, "connected to lan:27500 (free ride, 6 of 16)");
  line(con::LineKind::server, "Ada entered the game");
  line(con::LineKind::chat, "Ada: anyone up for the north bowl?");
  v.type("on my way, meet at the lift top");
  v.key(con::EditKey::enter);
  line(con::LineKind::team, "(Bo): powder under the cliffs, go left");
  line(con::LineKind::whisper, "[from Ada]: race you down");
  c.execute("whisper Ada you are on");
  line(con::LineKind::event, "gate 12 missed: +2 s");
  v.type("/r_bloom 0.1");
  v.key(con::EditKey::enter);
  c.run();
  v.type("/help r_bloom");
  v.key(con::EditKey::enter);
  c.run();
  line(con::LineKind::server, "Bo left the game");
  line(con::LineKind::event, "a record: 1:42.37 on seed 38");
  v.type("/sv_ma");
}

void run(ge::lab::Lab& lab) {
  const int w = static_cast<int>(lab.arg_num("width"));
  const int h = static_cast<int>(lab.arg_num("height"));
  con::Console c(false);
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
      {"con_speed", "3"}};
  lab.shot({"down", "THE CONSOLE DOWN",
            "The tilde pressed: the console covers con_height of the screen over the game, its "
            "scrollback above the input line, which holds a half-typed /sv_ma.",
            "server lines yellow, chat green, team cyan, whispers magenta, events orange, typed "
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

  int counts[7] = {};
  for (const con::ViewLine& l : v.lines()) ++counts[static_cast<int>(l.kind)];
  for (int k = 0; k < 7; ++k)
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
