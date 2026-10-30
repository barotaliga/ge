// The classic-enhance menu's pages, events and progress panel (menu.hpp).
//
// ge:summary Builds the classic-enhance menu's pages, applies its events, draws the progress panel
// ge:keywords classicenhance menu pages toggle cvar progress panel rows bars thumbnails
// ge:skill classicenhance ui
// ge:test tests/classicenhance_menu_test.cpp
#include "classicenhance/menu.hpp"

#include <algorithm>
#include <cstdio>

#include "ui/font.hpp"

namespace ge::classicenhance {

namespace {

std::string group_page(std::string_view group) {
  std::string id = "switches.";
  for (const char c : group) id += c == ' ' ? '-' : char(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
  return id;
}

std::string_view group_of_page(std::string_view page) {
  for (size_t g = 0; g < kSwitchGroupCount; g++) {
    if (group_page(kSwitchGroups[g]) == page) return kSwitchGroups[g];
  }
  return {};
}

const char* state_word(StageState s) {
  switch (s) {
    case StageState::waiting:
      return "WAITING";
    case StageState::running:
      return "WORKING";
    case StageState::skipped:
      return "UP TO DATE";
    case StageState::done:
      return "DONE";
    case StageState::failed:
      return "FAILED";
  }
  return "";
}

std::string upper(std::string s) {
  for (char& c : s) c = c >= 'a' && c <= 'z' ? char(c - 'a' + 'A') : c;
  return s;
}

void set_switch(console::Cvars& cvars, std::string_view id, bool on) {
  cvars.set("ce_" + std::string(id), on ? "1" : "0", console::Source::local);
}

}  // namespace

std::string clock_text(double seconds) {
  const long s = long(std::max(0.0, seconds) + 0.5);
  char buf[32];
  if (s < 60) {
    std::snprintf(buf, sizeof buf, "%ld S", s);
  } else if (s < 3600) {
    std::snprintf(buf, sizeof buf, "%ld:%02ld", s / 60, s % 60);
  } else {
    std::snprintf(buf, sizeof buf, "%ld:%02ld:%02ld", s / 3600, (s / 60) % 60, s % 60);
  }
  return buf;
}

ui::Menu classic_menu(std::span<const std::string> packs, const Switches& values) {
  ui::Menu m;
  ui::Page& root = m.add_page("classic");
  ui::Item pack{.id = "pack", .text = "PACK", .kind = ui::ItemKind::choice};
  pack.hint = "Your own classic packs: an installed game's folder, the release zip or a pack.";
  for (const std::string& p : packs) pack.choices.push_back(upper(p));
  if (pack.choices.empty()) {
    pack.choices.push_back("NONE FOUND");
    pack.disabled = true;
  }
  root.items.push_back(pack);
  root.items.push_back({.id = "enhance",
                        .text = "ENHANCE",
                        .kind = ui::ItemKind::action,
                        .hint = "Enhance the pack into this machine's cache, stage by stage.",
                        .opens = "enhancing",
                        .disabled = packs.empty(),
                        .prominent = true});
  root.items.push_back({.id = "switches",
                        .text = "SWITCHES",
                        .kind = ui::ItemKind::action,
                        .hint = "Each enhancement on or off; off is the classic original.",
                        .opens = "switches"});
  root.items.push_back({.id = "back", .text = "BACK", .kind = ui::ItemKind::back});

  ui::Page& run = m.add_page("enhancing", "ENHANCING");
  run.items.push_back({.id = "cancel",
                       .text = "CANCEL",
                       .kind = ui::ItemKind::action,
                       .hint = "Stop after the item it is on; the finished stages are kept."});
  run.items.push_back({.id = "play",
                       .text = "PLAY",
                       .kind = ui::ItemKind::action,
                       .hint = "Start the first map with the enhanced pack.",
                       .disabled = true,
                       .prominent = true});
  run.items.push_back({.id = "back", .text = "BACK", .kind = ui::ItemKind::back});

  ui::Page& sw = m.add_page("switches", "SWITCHES");
  for (size_t g = 0; g < kSwitchGroupCount; g++) {
    sw.items.push_back({.id = group_page(kSwitchGroups[g]),
                        .text = std::string(kSwitchGroups[g]),
                        .kind = ui::ItemKind::action,
                        .opens = group_page(kSwitchGroups[g])});
  }
  sw.items.push_back({.id = "back", .text = "BACK", .kind = ui::ItemKind::back});

  for (size_t g = 0; g < kSwitchGroupCount; g++) {
    ui::Page& page = m.add_page(group_page(kSwitchGroups[g]), std::string(kSwitchGroups[g]));
    for (const SwitchSpec& s : switches()) {
      if (s.group != kSwitchGroups[g]) continue;
      ui::Item it{.id = std::string(s.id), .text = std::string(s.text), .kind = ui::ItemKind::toggle};
      it.hint = std::string(s.hint) + (s.bake ? " Changes the cache: ENHANCE again." : "");
      it.on = values.*s.field;
      page.items.push_back(it);
    }
    page.items.push_back({.id = "all-on", .text = "ALL ON", .kind = ui::ItemKind::action});
    page.items.push_back({.id = "all-off", .text = "ALL OFF", .kind = ui::ItemKind::action});
    page.items.push_back({.id = "defaults", .text = "DEFAULTS", .kind = ui::ItemKind::action});
    page.items.push_back({.id = "back", .text = "BACK", .kind = ui::ItemKind::back});
  }
  return m;
}

void sync_switches(ui::Menu& menu, const Switches& values) {
  for (size_t g = 0; g < kSwitchGroupCount; g++) {
    ui::Page* page = menu.page(group_page(kSwitchGroups[g]));
    if (page == nullptr) continue;
    for (const SwitchSpec& s : switches()) {
      if (ui::Item* it = page->find(s.id)) it->on = values.*s.field;
    }
  }
}

void sync_enhancing(ui::Menu& menu, const BoardView& board) {
  ui::Page* page = menu.page("enhancing");
  if (page == nullptr) return;
  const bool running = board.started && !board.finished && !board.failed;
  if (ui::Item* it = page->find("cancel")) it->disabled = !running;
  if (ui::Item* it = page->find("play")) it->disabled = !board.finished;
}

MenuAction apply_menu_event(const ui::MenuEvent& e, ui::Menu& menu, console::Cvars& cvars,
                            Switches* values, int* pack) {
  if (e.kind == ui::EventKind::back_at_root) return MenuAction::back;
  if (e.page == "classic" && e.item == "pack" && e.kind == ui::EventKind::changed) {
    if (const ui::Item* it = menu.page("classic")->find("pack")) *pack = it->choice;
    return MenuAction::none;
  }
  if (e.page == "classic" && e.item == "enhance" && e.kind == ui::EventKind::activated)
    return MenuAction::enhance;
  if (e.page == "enhancing" && e.kind == ui::EventKind::activated) {
    if (e.item == "cancel") return MenuAction::cancel;
    if (e.item == "play") return MenuAction::play;
  }
  const std::string_view group = group_of_page(e.page);
  if (group.empty()) return MenuAction::none;
  const ui::Page* page = menu.page(e.page);
  if (e.kind == ui::EventKind::changed) {
    if (const ui::Item* it = page->find(e.item); it && find_switch(e.item))
      set_switch(cvars, e.item, it->on);
  } else if (e.kind == ui::EventKind::activated) {
    static const Switches kDefaults;
    for (const SwitchSpec& s : switches()) {
      if (s.group != group) continue;
      if (e.item == "all-on") set_switch(cvars, s.id, true);
      if (e.item == "all-off") set_switch(cvars, s.id, false);
      if (e.item == "defaults") set_switch(cvars, s.id, kDefaults.*s.field);
    }
  }
  sync_switches(menu, *values);
  return MenuAction::none;
}

ui::Rect panel_area(const ui::MenuLayout& layout) {
  const ui::Rect& safe = layout.safe;
  const ui::Rect& column = layout.column;
  const float gap = 32 * layout.scale;
  if (layout.portrait) {
    const float top = safe.y + gap;
    return {safe.x + gap / 2, top, safe.w - gap, std::max(0.0f, column.y - gap / 2 - top)};
  }
  // Beside the column, and above the help and the footer along the bottom.
  const bool right = column.x - safe.x < safe.right() - column.right();
  const float x = right ? column.right() + gap : safe.x + gap;
  const float w = right ? safe.right() - gap - x : column.x - gap - x;
  float bottom = safe.bottom() - gap;
  for (const ui::Rect& r : {layout.help, layout.footer}) {
    if (r.h > 0) bottom = std::min(bottom, r.y - gap / 2);
  }
  return {x, safe.y + gap, std::max(0.0f, w), std::max(0.0f, bottom - safe.y - gap)};
}

void draw_progress(ui::DrawList& list, const BoardView& board, ui::Rect area,
                   const PanelStyle& st, std::vector<ui::Image>& store, const std::string& pack) {
  if (area.w < 40 || area.h < 40) return;
  const float s = st.scale;
  const ui::Font& bold = ui::ui_font(ui::Weight::semibold);
  const ui::Font& thin = ui::ui_font(ui::Weight::medium);
  ui::Box panel;
  panel.rect = area;
  panel.radius = 10 * s;
  panel.fill = ui::solid(st.panel);
  panel.border = 1;
  panel.border_color = st.border;
  list.box(panel);
  list.push_clip(area);
  const float pad = 22 * s;
  const float x = area.x + pad, right = area.right() - pad, w = right - x;
  float y = area.y + pad;

  // The head: what is being enhanced, the whole run's share, the time.
  const double f = board.fraction();
  char pct[16];
  std::snprintf(pct, sizeof pct, "%d%%", int(f * 100 + 0.5));
  y += 26 * s;
  const std::string what = board.finished ? "READY"
                           : board.failed ? "STOPPED"
                           : board.started ? "ENHANCING"
                                           : "NOT STARTED";
  list.text(bold, 28 * s, x, y, what, st.text);
  list.text(bold, 28 * s, right, y, pct, st.text, ui::Align::right);
  if (!pack.empty()) {
    y += 22 * s;
    list.text(thin, 16 * s, x, y, upper(pack), st.dim);
  }
  y += 16 * s;
  list.rect({x, y, w, 10 * s}, st.track, 5 * s);
  const ui::Color whole = board.failed ? st.failed : board.finished ? st.done : st.running;
  list.rect({x, y, float(w * f), 10 * s}, whole, 5 * s);
  y += 10 * s + 24 * s;
  std::string clock = "ELAPSED " + clock_text(board.elapsed);
  if (board.left >= 0 && !board.finished && !board.failed)
    clock += "    ABOUT " + clock_text(board.left) + " LEFT IN THIS STAGE";
  if (board.textures_done > 0) clock += "    " + std::to_string(board.textures_done) + " TEXTURES";
  list.text(thin, 16 * s, x, y, clock, st.dim);
  y += 22 * s;

  // A row a stage: its name, its state, its bar, its count and the item it is on. Wide, the bar
  // stands between the state and the count; narrow (a phone upright), it runs the panel's width
  // under them and the item stands beside the state.
  const bool narrow = w < 520 * s;
  const float row = (narrow ? 50 : 44) * s, name_w = 120 * s, state_w = (narrow ? 96 : 120) * s;
  const float count_w = (narrow ? 60 : 110) * s;
  for (const StageView& v : board.stages) {
    const ui::Color c = v.state == StageState::running ? st.running
                        : v.state == StageState::done  ? st.done
                        : v.state == StageState::failed ? st.failed
                        : v.state == StageState::skipped ? st.skipped
                                                         : st.dim;
    const float base = y + 20 * s;
    list.text(bold, 19 * s, x, base, upper(stage_name(v.stage)), st.text);
    list.text(bold, 14 * s, x + name_w, base, state_word(v.state), c);
    const ui::Rect bar = narrow ? ui::Rect{x, base + 10 * s, w, 6 * s}
                                : ui::Rect{x + name_w + state_w, base - 9 * s,
                                           std::max(0.0f, w - name_w - state_w - count_w), 8 * s};
    list.rect(bar, st.track, bar.h / 2);
    const double share = v.state == StageState::done || v.state == StageState::skipped ? 1.0
                         : v.total > 0 ? double(v.done) / double(v.total)
                                       : 0.0;
    list.rect({bar.x, bar.y, float(bar.w * share), bar.h}, c, bar.h / 2);
    if (v.total > 0) {
      list.text(thin, 16 * s, right, base, std::to_string(v.done) + " / " + std::to_string(v.total),
                st.text, ui::Align::right);
    }
    if (!v.item.empty() && v.state != StageState::waiting) {
      const ui::Rect clip = narrow ? ui::Rect{x + name_w + state_w, y, w - name_w - state_w - count_w,
                                              row - 22 * s}
                                   : ui::Rect{bar.x, base + 2 * s, w - name_w - state_w, row - 22 * s};
      list.push_clip(clip);
      list.text(thin, 14 * s, clip.x, narrow ? base : base + 17 * s, upper(v.item), st.dim);
      list.pop_clip();
    }
    y += row;
  }

  // The last textures finished: the pack's picture, then the enhanced one.
  store.clear();
  const float result_h = 56 * s;
  const float room = area.bottom() - pad - result_h - y;
  if (!board.recent.empty() && room > 60 * s) {
    y += 18 * s;
    list.text(bold, 14 * s, x, y, "LAST TEXTURES: THE PACK'S, THEN ENHANCED", st.dim);
    y += 10 * s;
    const float side = std::min({110 * s, room - 50 * s, (w - 3 * 16 * s) / 6});
    store.reserve(board.recent.size() * 2);
    float tx = x;
    for (const Thumb& t : board.recent) {
      if (tx + 2 * side + 6 * s > right + 0.5f) break;
      for (const assets::Image* img : {&t.before, &t.after}) {
        store.push_back({img->width, img->height, img->rgba});
        const ui::Image& u = store.back();
        const float k = side / float(std::max(u.width, u.height));
        ui::Box b;
        b.rect = {tx, y, float(u.width) * k, float(u.height) * k};
        b.image = &u;
        b.uv = {0, 0, float(u.width), float(u.height)};
        b.nearest = true;
        b.radius = 3 * s;
        list.box(b);
        tx += side + 6 * s;
      }
      list.push_clip({tx - 2 * side - 12 * s, y + side, 2 * side + 6 * s, 20 * s});
      list.text(thin, 13 * s, tx - 2 * side - 12 * s, y + side + 15 * s, upper(t.name), st.dim);
      list.pop_clip();
      tx += 16 * s;
    }
  }

  // The result.
  const float by = area.bottom() - pad;
  if (board.finished) {
    list.text(bold, 20 * s, x, by - 22 * s, "THE CACHE IS READY", st.done);
    list.text(thin, 16 * s, x, by, "PRESS PLAY, OR TYPE MAP AND A MAP'S NAME IN THE CONSOLE",
              st.dim);
  } else if (board.failed) {
    list.text(bold, 20 * s, x, by - 22 * s, upper(board.error), st.failed);
    list.text(thin, 16 * s, x, by, "ENHANCE AGAIN TO GO ON: THE FINISHED STAGES ARE KEPT", st.dim);
  } else if (board.started) {
    list.text(thin, 16 * s, x, by, "YOU CAN LEAVE THIS PAGE: THE ENHANCING GOES ON", st.dim);
  }
  list.pop_clip();
}

}  // namespace ge::classicenhance
