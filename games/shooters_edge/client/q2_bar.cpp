// Quake II's status bar in the window (q2_bar.hpp).
//
// ge:summary Quake II's status bar drawn in the window from its program and the game's pictures
// ge:keywords q2 status-bar draw_q2_bar run_q2_bar
// ge:skill ui shooter
// ge:test tests/shooters_edge_q2_screen_test.cpp
#include "shooters_edge/client/q2_bar.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <span>
#include <string>
#include <utility>

#include "assets/pcx.hpp"
#include "classicenhance/cached.hpp"

namespace shooters_edge::client {

namespace {

constexpr int kNetTroubleMoves = 128;  // moves unanswered before the `net` picture shows

constexpr int kClear = 255;  // the pictures' see-through colour

const BarPicture* picture(Q2Bar& bar, const std::string& path) {
  auto it = bar.pictures.find(path);
  if (it == bar.pictures.end()) {
    BarPicture out;
    std::vector<uint8_t> store;
    const std::span<const uint8_t> bytes =
        bar.files != nullptr ? find_file(*bar.files, path, store) : std::span<const uint8_t>();
    ge::assets::IndexedImage img;
    std::string why;
    if (!bytes.empty() && ge::assets::decode_pcx(bytes, path, &img, &why) && !img.palette.empty()) {
      out.palette.assign(img.palette.begin(), img.palette.end());
      out.image.width = img.width;
      out.image.height = img.height;
      out.image.rgba = ge::assets::pcx_rgba(img);
      for (size_t i = 0; i < img.indices.size(); i++)
        if (img.indices[i] == kClear) out.image.rgba[i * 4 + 3] = 0;
      // Its enhanced copy, by the cache's name for it (the path without ".pcx").
      ge::assets::Image big;
      const std::string name = path.ends_with(".pcx") ? path.substr(0, path.size() - 4) : path;
      if (const uint32_t k =
              ge::classicenhance::cached_picture(bar.cache, name, static_cast<uint32_t>(img.width),
                                                 static_cast<uint32_t>(img.height), &big);
          k > 1) {
        out.image.width = big.width;
        out.image.height = big.height;
        out.image.rgba = std::move(big.rgba);
        out.k = static_cast<int>(k);
      }
    }
    it = bar.pictures.emplace(path, std::move(out)).first;
  }
  return it->second.image.empty() ? nullptr : &it->second;
}

constexpr int kInventoryRows = 17;

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& c : out)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return out;
}

// The first key bound exactly to `use <name>`, compared without case; "" none.
std::string hotkey(const std::map<std::string, std::string>* binds, std::string_view name) {
  if (binds == nullptr) return {};
  const std::string want = "use " + lower(name);
  for (const auto& [key, command] : *binds)
    if (lower(command) == want) return key;
  return {};
}

std::string right(std::string text, size_t width) {
  if (text.size() > width) text.resize(width);
  return std::string(width - text.size(), ' ') + text;
}

void add_text(std::vector<hud::LayoutOp>& ops, int x, int y, std::string text, bool alt) {
  hud::LayoutOp op;
  op.kind = hud::LayoutOp::Kind::text;
  op.x = x;
  op.y = y;
  op.text = std::move(text);
  op.alt = alt;
  ops.push_back(std::move(op));
}

// The inventory screen (q2_bar.hpp) into `ops`, on a `w` x `h` screen.
void inventory(const Q2Bar& bar, const server::RulesWorld& world, int slot, int selected, int w,
               int h, std::vector<hud::LayoutOp>& ops) {
  const std::vector<int16_t>& counts = world.clients[slot].inventory;
  std::vector<int> held;
  for (size_t n = 1; n < counts.size(); n++)
    if (counts[n] != 0) held.push_back(static_cast<int>(n));
  int at = -1;
  for (size_t i = 0; i < held.size(); i++)
    if (held[i] == selected) at = static_cast<int>(i);
  int top = std::max(at, 0) - kInventoryRows / 2;
  if (static_cast<int>(held.size()) >= kInventoryRows)
    top = std::min(top, static_cast<int>(held.size()) - kInventoryRows);
  top = std::max(top, 0);
  const int ox = (w - 256) / 2, oy = (h - 240) / 2;
  hud::LayoutOp pic;
  pic.name = "inventory";
  pic.x = ox;
  pic.y = oy + 8;
  ops.push_back(std::move(pic));
  add_text(ops, ox + 24, oy + 24, "hotkey ### item", false);
  add_text(ops, ox + 24, oy + 32, "------ --- ----", false);
  int y = oy + 40;
  for (int i = top; i < static_cast<int>(held.size()) && i < top + kInventoryRows; i++, y += 8) {
    const int n = held[static_cast<size_t>(i)];
    std::string scratch;
    const std::string name(q2_world_string(world, 1056 + n, scratch));
    std::string row = right(hotkey(bar.binds, name), 6) + " " +
                      right(std::to_string(counts[static_cast<size_t>(n)]), 3) + " " + name;
    const bool chosen = n == selected;
    if (chosen && (static_cast<int64_t>(bar.now * 10) & 1) != 0)
      add_text(ops, ox + 16, y, std::string(1, '\x0f'), false);
    add_text(ops, ox + 24, y, std::move(row), !chosen);
  }
}

struct Strings {
  const server::RulesWorld& world;
  mutable std::string scratch;
};

// The lag graph (scr_lag_draw) at its corner of the bar's screen, or, while 128 or more moves are
// unanswered, the pack's `net` picture there, shown 768 ms of every 1024.
bool draw_lag(ge::ui::DrawList& dl, Q2Bar& bar, float f, float alpha) {
  const Q2Screen* s = bar.screen;
  if (s == nullptr || s->draw2d == 0 || bar.lag == nullptr || f <= 0) return false;
  const int k = static_cast<int>(f);
  const int whole_w = bar.width / k, whole_h = bar.height / k;  // the smaller screen's, whole px
  const auto w = static_cast<float>(whole_w), h = static_cast<float>(whole_h);
  constexpr auto kSide = static_cast<float>(ge::hud::kLagColumns);
  const float x =
      s->lag_x < 0 ? w + static_cast<float>(s->lag_x) - kSide + 1 : static_cast<float>(s->lag_x);
  const float y =
      s->lag_y < 0 ? h + static_cast<float>(s->lag_y) - kSide + 1 : static_cast<float>(s->lag_y);
  bool drew = false;
  if (s->lag_draw > 0) {
    ge::hud::LagGraphLook look;
    look.min_ms = static_cast<float>(s->lag_min);
    look.max_ms = static_cast<float>(s->lag_max);
    look.box = s->lag_draw > 1;
    look.alpha = alpha;
    ge::hud::draw_lag_graph(dl, *bar.lag, {x * f, y * f, kSide * f, kSide * f}, look);
    drew = true;
  }
  const auto ms = static_cast<int64_t>(bar.now * 1000);
  if (bar.unanswered >= kNetTroubleMoves && (ms & 1023) < 768) {
    if (const BarPicture* img = picture(bar, "pics/net.pcx")) {
      ge::ui::Box b;
      b.rect = {x * f, y * f, img->width() * f, img->height() * f};
      b.image = &img->image;
      b.uv = {0, 0, static_cast<float>(img->image.width), static_cast<float>(img->image.height)};
      b.nearest = img->k == 1;
      b.feather = 0;
      b.opacity = alpha;
      dl.box(b);
      drew = true;
    }
  }
  return drew;
}

}  // namespace

std::string_view q2_world_string(const server::RulesWorld& w, int index, std::string& scratch) {
  const auto in = [](const std::vector<std::string>& list, int n) -> std::string_view {
    return n > 0 && n < static_cast<int>(list.size()) ? std::string_view(list[size_t(n)])
                                                      : std::string_view();
  };
  if (index >= hud::kLayoutImages && index < hud::kLayoutImages + 256)
    return in(w.images, index - hud::kLayoutImages);
  if (index >= 1056 && index < 1056 + 256) return in(w.item_names, index - 1056);
  if (index >= hud::kLayoutSkins && index < hud::kLayoutSkins + ge::shooter::kMaxClients) {
    const server::WorldClient& c = w.clients[index - hud::kLayoutSkins];
    if (!c.active) return {};
    scratch = c.name + "\\male/grunt";
    return scratch;
  }
  return {};
}

bool q2_world_of(const DemoState& ds, server::RulesWorld& out) {
  if (ds.format != DemoFormat::q2 || ds.status_bar.empty()) return false;
  if (ds.player < 0 || ds.player >= ge::shooter::kMaxClients) return false;
  out.status_bar = ds.status_bar;
  out.images = ds.images;
  out.item_names = ds.item_names;
  for (int i = 0; i < ge::shooter::kMaxClients; i++) {
    server::WorldClient& c = out.clients[i];
    c.name = ds.names[static_cast<size_t>(i)];
    c.active = !c.name.empty() || i == ds.player;
  }
  server::WorldClient& me = out.clients[ds.player];
  std::copy(ds.q2_stats.begin(), ds.q2_stats.end(), std::begin(me.numbers.stats));
  me.layout = ds.layout;
  me.inventory = ds.inventory;
  return true;
}

void draw_q2_hit_marker(ge::ui::DrawList& dl, Q2Bar& bar, int64_t hits, int width, int height) {
  if (hits > bar.hits_seen) bar.hit_at = bar.now;  // a new hit: the fade starts over
  bar.hits_seen = hits;                            // (fewer: a new recording or a seek back)
  const float alpha =
      bar.fx != nullptr ? ge::classic::q2_hit_marker_alpha(*bar.fx, bar.now - bar.hit_at) : 0;
  if (alpha <= 0 || (bar.screen != nullptr && bar.screen->draw2d == 0)) return;
  const BarPicture* mark = picture(bar, hud::layout_picture_path("marker"));
  if (mark == nullptr) return;  // the pack has no marker picture: none drawn
  const auto u = static_cast<float>(q2_bar_scale(bar, width, height));
  ge::ui::Box b;
  b.fill = ge::ui::solid({1, 0, 0, alpha});
  b.rect = {(static_cast<float>(width) - mark->width() * u) / 2,
            (static_cast<float>(height) - mark->height() * u) / 2, mark->width() * u,
            mark->height() * u};
  b.image = &mark->image;
  b.nearest = mark->k == 1;
  b.feather = 0;
  dl.box(b);
}

void add_q2_turtle(Q2Bar& bar, uint8_t trouble, int width, int height) {
  const int turtle = bar.screen != nullptr ? bar.screen->turtle : 1;
  if (turtle <= 0 || trouble == 0 || (bar.screen != nullptr && bar.screen->draw2d == 0)) return;
  const int scale = q2_bar_scale(bar, width, height);
  int y = height / scale - 11 * 8;  // one word a line, eleven lines over the bottom
  const std::pair<uint8_t, const char*> words[] = {{kQ2FrameBad, "BADFRAME"},
                                                   {kQ2FrameOld, "OLDFRAME"},
                                                   {kQ2FrameNoDelta, "NODELTA"},
                                                   {kQ2FrameSuppressed, "SUPPRESSED"}};
  for (const auto& [bit, word] : words) {
    if ((trouble & bit) == 0 || (bit == kQ2FrameSuppressed && turtle < 2)) continue;
    add_text(bar.ops, 0, y, word, true);
    y += 8;
  }
}

bool draw_recorded_q2_bar(ge::ui::DrawList& dl, Q2Bar& bar, const DemoState& ds, int width,
                          int height) {
  if (!bar.recorded) bar.recorded = std::make_shared<server::RulesWorld>();
  if (!q2_world_of(ds, *bar.recorded)) return false;
  run_q2_bar(bar, *bar.recorded, ds.player, ds.frames, width, height);
  add_q2_turtle(bar, ds.q2_trouble, width, height);
  draw_q2_bar(dl, bar, q2_bar_scale(bar, width, height));
  draw_q2_hit_marker(dl, bar, ds.hits, width, height);
  return true;
}

int q2_bar_scale(int width, int height, int setting) {
  if (setting > 0) return std::min(setting, 10);
  const int side = width >= height ? height : width;
  const int step = width >= height ? 1080 : 1920;  // a scale of 2 from here, 4 from twice it
  return side >= 2 * step ? 4 : side >= step ? 2 : 1;
}

void q2_bar_net(Q2Bar& bar, const ge::hud::NetHistory* lag, int unanswered, double now) {
  bar.lag = lag;
  bar.unanswered = unanswered;
  bar.now = now;
}

int q2_bar_scale(const Q2Bar& bar, int width, int height) {
  return q2_bar_scale(width, height, bar.screen != nullptr ? bar.screen->scale : 0);
}

namespace {

// A console command's look at the world, now that a frame has it.
void answer(Q2Bar& bar, const server::RulesWorld& world, int slot, const hud::LayoutInput& in) {
  ge::console::Console& c = *bar.asked_by;
  const std::vector<std::string> a = std::move(bar.asked);
  bar.asked.clear();
  bar.asked_by = nullptr;
  const std::string& word = a[0];
  const server::WorldClient& me = world.clients[slot];
  // A name made a file's under `folder`: no folder of its own, `.txt` added.
  const auto file_of = [](std::string_view folder, std::string name) {
    for (char& ch : name)
      if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_' && ch != '-' && ch != '.')
        ch = '_';
    if (!name.ends_with(".txt")) name += ".txt";
    return std::string(folder) + "/" + name;
  };
  if (word == "dumpclients") {
    for (int i = 0; i < ge::shooter::kMaxClients; i++) {
      const std::string& name = world.clients[i].name;
      if (name.empty()) continue;
      char num[8];
      std::snprintf(num, sizeof num, "%3d", i);
      c.print(std::string(num) + ": " + name);
    }
    return;
  }
  if (word == "dumpstatusbar" || word == "dumplayout") {
    const bool status = word == "dumpstatusbar";
    const std::string& program = status ? world.status_bar : me.layout;
    if (program.empty()) {
      c.print(status ? "No status bar to dump." : "No layout to dump.");
      return;
    }
    const std::string file = file_of("layouts", a[1]);
    if (c.write_file(file, program))
      c.print(std::string("Dumped ") + (status ? "status bar" : "layout") + " program to " + file +
              ".");
    else
      c.print("Error writing " + file);
    return;
  }
  // aashot and aadump: the bar and the layout up, as letters.
  const std::vector<std::string> rows =
      hud::layout_text(in, (in.stats[13] & 1) != 0 ? std::string_view(me.layout) : "");
  if (word == "aadump") {
    for (const std::string& r : rows) c.print(r);
    return;
  }
  std::string file;
  if (a.size() > 1) {
    file = file_of("scoreshots", a[1]);
  } else {
    std::string text;
    for (int n = 0; n < 1000 && file.empty(); n++) {
      char name[32];
      std::snprintf(name, sizeof name, "scoreshots/quake%03d.txt", n);
      if (!c.read_file(name, text)) file = name;
    }
    if (file.empty()) {
      c.print("All scoreshot slots are full.");
      return;
    }
  }
  std::string text;
  for (const std::string& r : rows) text += r + "\n";
  if (c.write_file(file, text))
    c.print("Wrote " + file + ".");
  else
    c.print("Error writing " + file);
}

}  // namespace

void add_q2_bar_commands(ge::console::Console& con, Q2Bar& bar) {
  const auto ask = [&bar](ge::console::Console& c, const ge::console::Args& a) {
    if (bar.ops.empty()) {  // no Quake II bar ran last frame: no level up
      c.print(a[0] == "aashot" || a[0] == "aadump" ? "Must be in a level."
                                                   : "Must be in a level to dump.");
      return;
    }
    bar.asked = a.words;
    bar.asked_by = &c;
  };
  con.add_command("dumpclients", "dumpclients: each player's slot and name", ask);
  for (const char* word : {"dumpstatusbar", "dumplayout"})
    con.add_command(word,
                    std::string(word) + " <file>: the " +
                        (std::string_view(word) == "dumplayout" ? "layout up" : "status bar") +
                        "'s program written to layouts/<file>.txt",
                    [ask](ge::console::Console& c, const ge::console::Args& a) {
                      if (a.size() != 2) {
                        c.print("Usage: " + a[0] + " <filename>");
                        return;
                      }
                      ask(c, a);
                    });
  con.add_command("aashot",
                  "aashot [name]: the status bar and the scores as text, to scoreshots/<name>.txt",
                  ask);
  con.add_command("aadump", "aadump: the status bar and the scores as text, in the console", ask);
}

void run_q2_bar(Q2Bar& bar, const server::RulesWorld& world, int slot, int64_t frame, int width,
                int height) {
  bar.ops.clear();
  bar.width = width;
  bar.height = height;
  bar.layout_up = false;
  if (world.status_bar.empty() || slot < 0 || slot >= ge::shooter::kMaxClients) return;
  // scr_draw2d: 0 none of it; 1 no status bar; 3 no layouts (the scoreboard, the help computer).
  const int draw2d = bar.screen != nullptr ? bar.screen->draw2d : 2;
  if (draw2d == 0) return;
  const int scale = q2_bar_scale(bar, width, height);
  Strings strings{world, {}};
  hud::LayoutInput in;
  in.program = world.status_bar;
  in.stats = std::span<const int16_t>(world.clients[slot].numbers.stats, hud::kLayoutStats);
  in.width = width / scale;
  in.height = height / scale;
  in.frame = frame;
  in.self = slot;
  in.ctx = &strings;
  in.string = [](const void* ctx, int index) {
    const auto& s = *static_cast<const Strings*>(ctx);
    return q2_world_string(s.world, index, s.scratch);
  };
  if (draw2d != 1) hud::run_layout(in, bar.ops);  // its text copied out of the strings as read
  if (bar.asked_by != nullptr && !bar.asked.empty()) answer(bar, world, slot, in);
  const server::WorldClient& me = world.clients[slot];
  bar.layout_up = (in.stats[13] & 3) != 0;
  const bool shown = (in.stats[13] & 1) != 0 || bar.f1 == 2;  // F1 in a recording: the last one
  if (shown && !me.layout.empty() && (draw2d != 3 || bar.f1 != 0)) {
    std::vector<hud::LayoutOp> over;  // the screen the rules sent him, over the bar
    in.program = me.layout;
    hud::run_layout(in, over);
    for (hud::LayoutOp& op : over) bar.ops.push_back(std::move(op));
  }
  if ((in.stats[13] & 2) != 0 && !me.inventory.empty())
    inventory(bar, world, slot, in.stats[12], in.width, in.height, bar.ops);
}

void add_q2_screen(Q2Bar& bar, int crosshair, std::string_view centre, int width, int height) {
  if (bar.screen != nullptr && bar.screen->draw2d == 0) return;
  const int scale = q2_bar_scale(bar, width, height);
  const int w = width / scale, h = height / scale;
  if (crosshair >= 1 && crosshair <= 3) {
    hud::LayoutOp op;
    op.name = "ch" + std::to_string(crosshair);
    if (const BarPicture* img = picture(bar, hud::layout_picture_path(op.name))) {
      op.x = (w - static_cast<int>(img->width())) / 2;
      op.y = (h - static_cast<int>(img->height())) / 2;
      bar.ops.insert(bar.ops.begin(), std::move(op));  // under the bar
    }
  }
  if (centre.empty()) return;
  std::vector<std::string_view> lines;
  for (size_t at = 0; at <= centre.size();) {
    const size_t end = std::min(centre.find('\n', at), centre.size());
    lines.push_back(centre.substr(at, end - at));
    at = end + 1;
  }
  int y = lines.size() <= 4 ? h * 35 / 100 : 48;
  for (const std::string_view line : lines) {
    hud::LayoutOp op;
    op.kind = hud::LayoutOp::Kind::text;
    op.text = std::string(line.substr(0, std::min<size_t>(line.size(), 40)));
    op.x = (w - 8 * static_cast<int>(op.text.size())) / 2;
    op.y = y;
    bar.ops.push_back(std::move(op));
    y += 8;
  }
}

void draw_q2_crosshair(ge::ui::DrawList& dl, Q2Bar& bar, const ge::hud::CrosshairSettings& s,
                       int width, int height, int health) {
  // Quake II's own `crosshair` numbers its pictures (client/cvar_layers.hpp); Quake's
  // over Quake II's bar (remix) takes its first picture as the game's own mark.
  const int32_t number = s.pictures ? std::clamp(s.mark, 1, 3) : 1;
  const BarPicture* own = picture(bar, hud::layout_picture_path("ch" + std::to_string(number)));
  static const ge::ui::Image kNone;
  bool shown = false;
  const ge::hud::Picture p = ge::hud::crosshair_picture(
      s, own != nullptr ? own->image : kNone, own != nullptr ? static_cast<float>(own->k) : 1,
      own != nullptr ? std::span<const uint8_t>(own->palette) : std::span<const uint8_t>(), &shown,
      health);
  if (!shown || (bar.screen != nullptr && bar.screen->draw2d == 0)) return;
  const auto u = static_cast<float>(q2_bar_scale(bar, width, height));
  const ge::hud::Size z = ge::hud::measure(p, u);
  const float x = (static_cast<float>(width) - z.w) / 2 + static_cast<float>(s.offset[0]) * u;
  const float y = (static_cast<float>(height) - z.h) / 2 + static_cast<float>(s.offset[1]) * u;
  ge::hud::draw(dl, p, {x, y, z.w, z.h}, u);
}

bool draw_q2_bar(ge::ui::DrawList& dl, Q2Bar& bar, int scale) {
  const auto f = static_cast<float>(scale);
  const float alpha =
      bar.screen != nullptr ? static_cast<float>(std::clamp(bar.screen->alpha, 0.0, 1.0)) : 1;
  bool drew = draw_lag(dl, bar, f, alpha);
  const auto box = [&](const BarPicture* img, float x, float y, float w, float h, ge::ui::Rect uv,
                       const uint8_t rgba[4]) {
    ge::ui::Box b;
    b.fill =
        ge::ui::solid({rgba[0] / 255.0f, rgba[1] / 255.0f, rgba[2] / 255.0f, rgba[3] / 255.0f});
    b.rect = {x * f, y * f, w * f, h * f};
    b.image = &img->image;
    b.uv = uv;
    b.nearest = img->k == 1;
    b.feather = 0;
    b.opacity = alpha;
    dl.box(b);
    drew = true;
  };
  const BarPicture* glyphs = nullptr;
  for (const hud::LayoutOp& op : bar.ops) {
    if (op.kind == hud::LayoutOp::Kind::picture) {
      const BarPicture* img = picture(bar, hud::layout_picture_path(op.name));
      if (img == nullptr && !op.fallback.empty())
        img = picture(bar, hud::layout_picture_path(op.fallback));
      if (img == nullptr) continue;
      box(img, static_cast<float>(op.x), static_cast<float>(op.y), img->width(), img->height(),
          {0, 0, static_cast<float>(img->image.width), static_cast<float>(img->image.height)},
          op.rgba);
      continue;
    }
    if (glyphs == nullptr) glyphs = picture(bar, "pics/conchars.pcx");
    if (glyphs == nullptr) return drew;
    const float cell = static_cast<float>(glyphs->image.width) / 16;
    for (size_t i = 0; i < op.text.size(); i++) {
      auto code = static_cast<uint8_t>(op.text[i]);
      if (op.alt) code = static_cast<uint8_t>(code | 0x80);
      if ((code & 0x7F) == ' ') continue;
      const float x = static_cast<float>(op.x) + 8 * static_cast<float>(i);
      if (op.y <= -8) continue;
      box(glyphs, x, static_cast<float>(op.y), 8, 8,
          {static_cast<float>(code & 15) * cell, static_cast<float>(code >> 4) * cell, cell, cell},
          op.rgba);
    }
  }
  return drew;
}

}  // namespace shooters_edge::client
