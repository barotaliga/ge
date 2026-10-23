// Menus as a model, defined: the page stack, the focus walk over the stops, a press and an
// adjustment for each item kind, the pointer's hover and click, and the events out.
//
// ge:summary Menus defined: the page stack, focus walking, presses, adjustments, the pointer
// ge:keywords ui menu page stack focus wrap press adjust toggle choice slider pointer hover click
// ge:skill ui
// ge:test tests/ui_menu_test.cpp
#include "ui/menu.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ge::ui {

Item* Page::find(std::string_view item_id) {
  for (Item& it : items)
    if (it.id == item_id) return &it;
  return nullptr;
}

const Item* Page::find(std::string_view item_id) const {
  return const_cast<Page*>(this)->find(item_id);
}

Page& Menu::add_page(std::string id, std::string title) {
  pages_.push_back({std::move(id), std::move(title), {}, -1});
  if (stack_.empty()) stack_.push_back(pages_.front().id);
  return pages_.back();
}

Page* Menu::page(std::string_view id) {
  for (Page& p : pages_)
    if (p.id == id) return &p;
  return nullptr;
}

const Page* Menu::page(std::string_view id) const { return const_cast<Menu*>(this)->page(id); }

Page& Menu::top() { return *page(stack_.back()); }
const Page& Menu::top() const { return *page(stack_.back()); }

namespace {

// The first stop at or after `from` going `dir`, wrapping; -1 when the page has none.
int next_stop(const Page& p, int from, int dir) {
  const int n = static_cast<int>(p.items.size());
  for (int k = 0; k < n; k++) {
    const int i = ((from + dir * k) % n + n) % n;
    if (p.items[static_cast<size_t>(i)].stop()) return i;
  }
  return -1;
}

void settle_focus(Page& p) {
  if (p.focus < 0 || p.focus >= static_cast<int>(p.items.size()) ||
      !p.items[static_cast<size_t>(p.focus)].stop())
    p.focus = next_stop(p, std::max(p.focus, 0), 1);
}

}  // namespace

bool Menu::open(std::string_view id, std::vector<MenuEvent>* events) {
  Page* p = page(id);
  if (p == nullptr) return false;
  stack_.emplace_back(id);
  settle_focus(*p);
  if (events) events->push_back({EventKind::opened, std::string(id), {}});
  return true;
}

bool Menu::close(std::vector<MenuEvent>* events) {
  if (at_root()) return false;
  if (events) events->push_back({EventKind::closed, stack_.back(), {}});
  stack_.pop_back();
  return true;
}

void Menu::reset() {
  if (!stack_.empty()) stack_.resize(1);
}

void Menu::move_focus(int step) {
  Page& p = top();
  settle_focus(p);
  if (p.focus < 0 || step == 0) return;
  const int dir = step > 0 ? 1 : -1;
  for (int k = 0; k < std::abs(step); k++) p.focus = next_stop(p, p.focus + dir, dir);
}

bool Menu::focus(std::string_view item_id) {
  Page& p = top();
  for (size_t i = 0; i < p.items.size(); i++) {
    if (p.items[i].id == item_id && p.items[i].stop()) {
      p.focus = static_cast<int>(i);
      return true;
    }
  }
  return false;
}

const Item* Menu::focused() const {
  const Page& p = top();
  if (p.focus < 0 || p.focus >= static_cast<int>(p.items.size())) return nullptr;
  return &p.items[static_cast<size_t>(p.focus)];
}

void Menu::press(Item& item, std::vector<MenuEvent>& out) {
  const std::string page_id = top().id;
  switch (item.kind) {
    case ItemKind::action:
      out.push_back({EventKind::activated, page_id, item.id});
      if (!item.opens.empty()) open(item.opens, &out);
      break;
    case ItemKind::toggle:
    case ItemKind::choice:
      adjust(item, 1, out);
      break;
    case ItemKind::back:
      close(&out);
      break;
    case ItemKind::slider:
    case ItemKind::label:
      break;
  }
}

void Menu::adjust(Item& item, int dir, std::vector<MenuEvent>& out) {
  switch (item.kind) {
    case ItemKind::toggle:
      item.on = !item.on;
      break;
    case ItemKind::choice: {
      const int n = static_cast<int>(item.choices.size());
      if (n == 0) return;
      item.choice = ((item.choice + dir) % n + n) % n;
      break;
    }
    case ItemKind::slider: {
      // Counted in steps from min, so a value never drifts off the grid.
      const double steps = item.step > 0 ? std::round((item.value - item.min) / item.step) : 0;
      const double next = std::clamp(item.min + (steps + dir) * item.step, item.min, item.max);
      if (next == item.value) return;
      item.value = next;
      break;
    }
    default:
      return;
  }
  out.push_back({EventKind::changed, top().id, item.id});
}

std::vector<MenuEvent> Menu::input(const MenuInput& in, const std::vector<Rect>& hit) {
  std::vector<MenuEvent> out;
  if (stack_.empty()) return out;
  Page& p = top();
  settle_focus(p);
  // The pointer first: hovering moves the focus, so a click presses what is under it.
  int under = -1;
  if (!hit.empty() && (in.pointer_moved || in.click)) {
    for (size_t i = 0; i < hit.size() && i < p.items.size(); i++) {
      if (hit[i].contains(in.pointer_x, in.pointer_y) && p.items[i].stop()) {
        under = static_cast<int>(i);
        p.focus = under;
      }
    }
  }
  if (in.up) move_focus(-1);
  if (in.down) move_focus(1);
  if (p.focus >= 0) {
    Item& item = p.items[static_cast<size_t>(p.focus)];
    if (in.left) adjust(item, -1, out);
    if (in.right) adjust(item, 1, out);
    if (in.accept || (in.click && under == p.focus)) press(item, out);
  }
  if (in.back) {
    if (!close(&out)) out.push_back({EventKind::back_at_root, top().id, {}});
  }
  return out;
}

std::string item_value(const Item& item, std::string_view on, std::string_view off) {
  switch (item.kind) {
    case ItemKind::toggle:
      return std::string(item.on ? on : off);
    case ItemKind::choice:
      return item.choices.empty()
                 ? std::string()
                 : item.choices[static_cast<size_t>(item.choice) % item.choices.size()];
    case ItemKind::slider: {
      char buf[32];
      const bool whole = item.step >= 1 && std::floor(item.step) == item.step;
      std::snprintf(buf, sizeof buf, whole ? "%.0f" : "%.1f", item.value);
      return buf + item.unit;
    }
    default:
      return {};
  }
}

}  // namespace ge::ui
