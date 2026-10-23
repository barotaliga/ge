// A GAME'S MENUS AS A MODEL: pages of items, which page is up, which item has the focus, and what
// the player's presses do to them. It holds no pixels (menu_view.hpp lays it out and draws it),
// so a menu is built, driven and tested headless, and a lab or a bot walks it by the same inputs
// a player gives it.
//
// A PAGE is a title and a column of items. An ITEM is one of:
//   - action:  a press that does something the game names by the item's id (PLAY, QUIT), or
//              opens another page (`opens`: OPTIONS);
//   - toggle:  on or off (left, right or a press flips it);
//   - choice:  one of a list (left and right step through it, wrapping; a press steps on);
//   - slider:  a number between `min` and `max` in `step`s (left and right);
//   - label:   words that are not a stop (a heading, a note); the focus skips them;
//   - back:    returns to the page below (the same as the back press).
// An item may be `disabled` (shown, skipped by the focus) and `prominent` (a bigger slab: the
// one press most players want, PLAY or RESUME).
//
// INPUT is what the game's input layer (M19's `menu` context) made of this frame's presses, each
// an edge (true on the frame it was pressed): up, down, left, right, accept, back; and the pointer
// (moved, where, clicked) against the item boxes of the last layout. Holding a direction to
// repeat it is the input layer's (its repeat), not the menu's.
//
// OUTPUT is a list of events the game acts on: an action pressed, a value changed, a page opened
// or closed, and `back` on the root page (the game decides: nothing, a quit prompt, resume).
// The menu never calls the game, so the game's state stays the game's and a menu replays.
//
// ge:summary Menus as a model: pages, items (action toggle choice slider label back), focus, events
// ge:keywords ui menu main-menu front-door title page item button action toggle choice slider
// ge:keywords label back focus navigation cursor keyboard gamepad pointer mouse event options
// ge:keywords pause settings
// ge:skill ui
// ge:test tests/ui_menu_test.cpp
#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

#include "ui/image.hpp"

namespace ge::ui {

enum class ItemKind : uint8_t { action, toggle, choice, slider, label, back };

struct Item {
  std::string id{};    // what the game knows it by; unique in its page
  std::string text{};  // what the player reads (from the game's strings table)
  ItemKind kind = ItemKind::action;
  std::string hint{};   // a line about the item, shown while it has the focus
  std::string opens{};  // action: the page it opens, or ""
  bool disabled = false;
  bool prominent = false;
  bool on = false;                     // toggle
  std::vector<std::string> choices{};  // choice
  int choice = 0;
  double value = 0, min = 0, max = 1, step = 0.1;  // slider
  std::string unit{};                              // slider: shown after the value ("%")

  bool stop() const { return kind != ItemKind::label && !disabled; }
};

struct Page {
  std::string id;
  std::string title;  // "" on the root page, where the game's logo stands instead
  std::vector<Item> items;
  int focus = -1;  // the focused item; -1 before the page is first shown

  Item* find(std::string_view item_id);
  const Item* find(std::string_view item_id) const;
};

struct MenuInput {
  bool up = false, down = false, left = false, right = false;
  bool accept = false, back = false;
  bool pointer_moved = false;
  bool click = false;  // the pointer's primary button released over where it was pressed
  float pointer_x = 0, pointer_y = 0;
};

enum class EventKind : uint8_t { activated, changed, opened, closed, back_at_root };

struct MenuEvent {
  EventKind kind;
  std::string page;  // the page it happened on (opened: the page opened)
  std::string item;  // the item ("" for opened, closed and back_at_root)
};

class Menu {
 public:
  // Pages are added before they are opened; the first page added is the root and is open.
  Page& add_page(std::string id, std::string title = {});
  Page* page(std::string_view id);
  const Page* page(std::string_view id) const;

  // The page up, and the stack of pages under it (root first).
  Page& top();
  const Page& top() const;
  const std::vector<std::string>& stack() const { return stack_; }
  bool at_root() const { return stack_.size() <= 1; }

  // Opens `id` over the page up (a press on an item that `opens` does this).
  bool open(std::string_view id, std::vector<MenuEvent>* events = nullptr);
  // Closes the page up; false at the root.
  bool close(std::vector<MenuEvent>* events = nullptr);
  // Back to the root, closing every page over it.
  void reset();

  // The focus: moved by `step` stops (wrapping), set directly, or the first stop.
  void move_focus(int step);
  bool focus(std::string_view item_id);
  const Item* focused() const;

  // One frame's presses. `hit` is the item boxes of the last layout (menu_view.hpp), index for
  // index with the page's items, for the pointer; empty: the pointer is ignored.
  std::vector<MenuEvent> input(const MenuInput& in, const std::vector<Rect>& hit = {});

 private:
  void press(Item& item, std::vector<MenuEvent>& out);
  void adjust(Item& item, int dir, std::vector<MenuEvent>& out);
  std::deque<Page> pages_;  // a deque: a page added keeps the others where they are
  std::vector<std::string> stack_;
};

// The value an item shows on its right: "ON"/"OFF", the choice, the slider's number with its unit.
std::string item_value(const Item& item, std::string_view on = "ON", std::string_view off = "OFF");

}  // namespace ge::ui
