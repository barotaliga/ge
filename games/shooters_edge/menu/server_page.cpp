// OPTIONS > SERVER SETTINGS (server_page.hpp).
//
// ge:summary The server settings pages filled from the sheet; changes sent, votes asked on leaving
// ge:keywords server-page fill vote-question serversheet
// ge:skill ui net
// ge:test tests/shooters_edge_server_page_test.cpp
#include "shooters_edge/menu/server_page.hpp"

#include <algorithm>
#include <cctype>

namespace shooters_edge::menu {

namespace gs = ge::server;
using ge::ui::ItemKind;

namespace {

// A meaning longer than this shows the value instead: the modern look's column, and the classic
// look's value column of 12 glyphs.
constexpr size_t kMeaningChars = 20;
constexpr size_t kClassicMeaningChars = 12;

std::string cased(std::string_view s, bool lower) {
  std::string out(s);
  for (char& c : out)
    c = static_cast<char>(lower ? std::tolower(static_cast<unsigned char>(c))
                                : std::toupper(static_cast<unsigned char>(c)));
  return out;
}

std::string group_page(std::string_view group) { return "server/" + std::string(group); }

// What a choice shows: its meaning when short, else the value.
std::string shown(const gs::SheetChoice& c, bool lower, size_t most) {
  if (!c.meaning.empty() && c.meaning.size() <= most) return cased(c.meaning, lower);
  return c.value.empty() ? std::string("-") : c.value;  // a value as the server writes it
}

}  // namespace

bool ServerPage::owns(std::string_view page) {
  return page == kServerPage || page.starts_with("server/");
}

void ServerPage::fill(ge::ui::Menu& menu, const gs::SettingsSheet* sheet,
                      const std::vector<std::string>& maps, bool lower, bool classic) {
  sheet_ = sheet;
  lower_ = lower;
  values_.clear();
  const auto page_of = [&menu, lower](std::string_view id, std::string_view title) {
    ge::ui::Page* p = menu.page(id);
    if (p == nullptr) p = &menu.add_page(std::string(id), cased(title, lower));
    return p;
  };
  std::vector<std::string> groups;
  if (sheet != nullptr) groups = gs::sheet_groups(*sheet);
  // Each page's items made again, the focus kept where it was.
  std::vector<std::pair<ge::ui::Page*, int>> kept;
  const auto start = [&](ge::ui::Page* p) {
    kept.emplace_back(p, p->focus);
    p->items.clear();
  };
  meaning_chars_ = classic ? kClassicMeaningChars : kMeaningChars;
  ge::ui::Page* first = page_of(kServerPage, "SERVER SETTINGS");
  start(first);
  for (const std::string& g : groups) start(page_of(group_page(g), g));
  bool votes = false;
  if (sheet != nullptr)
    for (const gs::SheetRow& r : sheet->rows) {
      ge::ui::Page* p = r.group.empty() ? first : menu.page(group_page(r.group));
      ge::ui::Item it{.id = r.name, .text = cased(r.words.empty() ? r.name : r.words, lower)};
      it.text += gs::sheet_mark(r.how);
      votes = votes || r.how == gs::SheetHow::vote;
      it.kind = ItemKind::choice;
      std::vector<gs::SheetChoice> choices = gs::sheet_choices(r);
      if (r.name == "map" && !maps.empty()) {  // the maps the player has
        choices.clear();
        for (const std::string& m : maps) choices.push_back({m, {}});
        if (std::none_of(choices.begin(), choices.end(),
                         [&r](const gs::SheetChoice& c) { return c.value == r.value; }))
          choices.insert(choices.begin(), {r.value, {}});
      }
      if (r.value.empty()) choices.insert(choices.begin(), {"", {}});  // not known yet
      std::vector<std::string>& values = values_[r.name];
      for (const gs::SheetChoice& c : choices) {
        values.push_back(c.value);
        it.choices.push_back(c.value.empty() && c.meaning.empty()
                                 ? std::string("-")
                                 : shown(c, lower, meaning_chars_));
      }
      const std::string& want = pending_ && pending_->name == r.name ? pending_->value : r.value;
      const auto at = std::find(values.begin(), values.end(), want);
      it.choice = at == values.end() ? 0 : static_cast<int>(at - values.begin());
      it.hint = r.help;
      if (r.how == gs::SheetHow::vote)
        it.hint += (it.hint.empty() ? "" : " ") + std::string("Changing it calls a vote.");
      if (r.how == gs::SheetHow::fixed || r.how == gs::SheetHow::admin) {
        it.disabled = true;
        it.hint += (it.hint.empty() ? "" : " ") + std::string("The server's owner sets it.");
      }
      p->items.push_back(std::move(it));
    }
  for (const std::string& g : groups) {
    ge::ui::Item it{.id = group_page(g), .text = cased(g, lower), .kind = ItemKind::action};
    it.opens = group_page(g);
    first->items.push_back(std::move(it));
  }
  if (sheet == nullptr || (sheet->rows.empty() && !sheet->asked))
    first->items.push_back(
        {.id = "none", .text = cased("Not on a server", lower), .kind = ItemKind::label});
  else if (sheet->rows.empty())
    first->items.push_back(
        {.id = "none", .text = cased("Asking the server...", lower), .kind = ItemKind::label});
  if (votes)
    first->items.push_back(
        {.id = "legend", .text = cased("* changes by a vote", lower), .kind = ItemKind::label});
  for (auto& [p, focus] : kept) {
    p->items.push_back({.id = "back", .text = cased("Back", lower), .kind = ItemKind::back});
    p->items.back().disabled = classic;  // ESCAPE goes back in the classic look
    p->focus = std::min(focus, static_cast<int>(p->items.size()) - 1);
    if (p->focus >= 0 && !p->items[static_cast<size_t>(p->focus)].stop()) p->focus = -1;
  }
}

void ServerPage::send(const gs::SheetRow& row, const std::string& value,
                      ge::console::Console& con) {
  for (const std::string& line : gs::sheet_change_lines(row, value)) con.execute("cmd " + line);
  con.execute("serversheet changed");
}

void ServerPage::ask(ge::ui::Menu& menu) {
  if (!pending_) return;
  ge::ui::Page* p = menu.page(kVotePage);
  if (p == nullptr) p = &menu.add_page(std::string(kVotePage), cased("Call a vote?", lower_));
  p->items.clear();
  p->items.push_back(
      {.id = "what",
       .text = cased(pending_->words + ": " + pending_->from + " to " + pending_->to, lower_),
       .kind = ItemKind::label});
  p->items.push_back(
      {.id = "yes", .text = cased("Yes, call it", lower_), .kind = ItemKind::action});
  p->items.push_back({.id = "no", .text = cased("No, leave it", lower_), .kind = ItemKind::action});
  p->focus = -1;
  menu.open(kVotePage);
  menu.focus("yes");
}

void ServerPage::event(const ge::ui::MenuEvent& e, ge::ui::Menu& menu, ge::console::Console& con) {
  if (e.kind == ge::ui::EventKind::opened && e.page == kServerPage && !open_) {
    open_ = true;
    since_ = 0;
    con.execute("serversheet open");
    return;
  }
  if (e.page == kVotePage && e.kind == ge::ui::EventKind::activated) {
    if (e.item == "yes" && pending_ && sheet_ != nullptr)
      if (const gs::SheetRow* row = gs::sheet_find(*sheet_, pending_->name))
        send(*row, pending_->value, con);
    pending_.reset();
    menu.close();
    return;
  }
  if (e.page == kVotePage && e.kind == ge::ui::EventKind::closed) {
    pending_.reset();  // ESCAPE on the question: nothing called
    return;
  }
  if (e.kind == ge::ui::EventKind::closed && pending_) {
    ask(menu);  // going back a page with a vote not asked
    return;
  }
  if (e.kind != ge::ui::EventKind::changed || sheet_ == nullptr) return;
  const gs::SheetRow* row = gs::sheet_find(*sheet_, e.item);
  const ge::ui::Page* page = menu.page(e.page);
  const ge::ui::Item* it = page != nullptr ? page->find(e.item) : nullptr;
  const auto values = values_.find(e.item);
  if (row == nullptr || it == nullptr || values == values_.end() || it->choice < 0 ||
      static_cast<size_t>(it->choice) >= values->second.size())
    return;
  const std::string& value = values->second[static_cast<size_t>(it->choice)];
  if (row->how == gs::SheetHow::vote) {
    if (value == row->value) {
      pending_.reset();  // turned back to what it is
      return;
    }
    const std::vector<gs::SheetChoice> choices = gs::sheet_choices(*row);
    const auto said = [&choices, this](const std::string& v) {
      for (const gs::SheetChoice& c : choices)
        if (c.value == v && !c.meaning.empty() && c.meaning.size() <= meaning_chars_)
          return c.meaning;
      return v.empty() ? std::string("-") : v;
    };
    pending_ = Pending{e.page, row->name, value, row->words, said(row->value), said(value)};
    return;
  }
  if (row->how == gs::SheetHow::direct) send(*row, value, con);
}

void ServerPage::after(ge::ui::Menu& menu, std::string_view page_before,
                       std::string_view item_before, ge::console::Console& con) {
  if (pending_ && menu.top().id == page_before && page_before == pending_->page &&
      item_before == pending_->name) {
    const ge::ui::Item* now = menu.focused();
    if (now == nullptr || now->id != pending_->name) ask(menu);
  }
  if (!open_) return;
  const auto& stack = menu.stack();
  if (std::none_of(stack.begin(), stack.end(), [](const std::string& p) { return owns(p); }))
    leave(con);
}

void ServerPage::tick(double dt, ge::console::Console& con) {
  if (!open_) return;
  since_ += dt;
  if (since_ < 0.5) return;
  since_ = 0;
  con.execute("serversheet refresh");
}

void ServerPage::leave(ge::console::Console& con) {
  pending_.reset();
  if (!open_) return;
  open_ = false;
  con.execute("serversheet close");
}

}  // namespace shooters_edge::menu
