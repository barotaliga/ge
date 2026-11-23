// A game's menu script read over its menu table (menu_script.hpp).
//
// ge:summary The menu script's reader: lines, comments, expansion, options, rows to entries
// ge:keywords menu-script q2pro-menu parser include begin end rows warnings
// ge:skill ui console
// ge:test tests/shooters_edge_menu_script_test.cpp
#include "shooters_edge/menu/menu_script.hpp"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <set>
#include <utility>

#include "console/cvar.hpp"
#include "console/lex.hpp"

namespace shooters_edge::menu {

namespace {

using ge::ui::ItemKind;

constexpr size_t kMaxLine = 1024;
constexpr int kMaxIncludes = 16;

// The pages the game draws itself: a script's page by one of these names is left to ours.
constexpr std::string_view kOursOnly[] = {"setup", "load",     "save",        "servers",
                                          "demos", "controls", "screenshots", "games"};

bool ours_only(std::string_view page) {
  return std::find(std::begin(kOursOnly), std::end(kOursOnly), page) != std::end(kOursOnly);
}

struct Logical {
  std::string text;
  int line = 0;
};

// `text` as logical lines: comments out, joined lines in, blank lines dropped. A line with a quote
// not closed is told and dropped.
std::vector<Logical> logical_lines(std::string_view text, const std::string& where,
                                   std::vector<std::string>& warnings) {
  std::vector<Logical> out;
  std::string cur;
  int line = 1, start = 1;
  bool quote = false, block = false;
  const auto finish = [&] {
    if (quote)
      warnings.push_back(where + " line " + std::to_string(start) +
                         ": a quote is not closed, so the line is skipped.");
    else if (cur.find_first_not_of(" \t\r") != std::string::npos)
      out.push_back({cur, start});
    cur.clear();
    quote = false;
  };
  for (size_t i = 0; i < text.size(); i++) {
    const char c = text[i];
    if (block) {
      if (c == '\n') line++;
      if (c == '*' && i + 1 < text.size() && text[i + 1] == '/') {
        block = false;
        i++;
      }
      continue;
    }
    if (c == '\n') {
      finish();
      start = ++line;
      continue;
    }
    if (!quote && c == '\\') {  // a backslash ending the line joins the next one
      size_t j = i + 1;
      while (j < text.size() && text[j] == '\r') j++;
      if (j >= text.size() || text[j] == '\n') {
        i = j;
        line++;
        continue;
      }
    }
    if (c == '"') quote = !quote;
    if (!quote && c == '/' && i + 1 < text.size() && text[i + 1] == '/') {
      while (i < text.size() && text[i] != '\n') i++;
      i--;
      continue;
    }
    if (!quote && c == '/' && i + 1 < text.size() && text[i + 1] == '*') {
      block = true;
      i++;
      cur += ' ';
      continue;
    }
    cur += c;
  }
  finish();
  return out;
}

// `$name` expanded outside double quotes; `$$` a `$`.
std::string expand_unquoted(std::string_view s, const ScriptLookup& lookup) {
  std::string out;
  size_t at = 0;
  while (at < s.size()) {
    const size_t q = s.find('"', at);
    if (q == std::string_view::npos) {
      out += ge::console::expand(s.substr(at), lookup);
      break;
    }
    out += ge::console::expand(s.substr(at, q - at), lookup);
    const size_t e = s.find('"', q + 1);
    const size_t end = e == std::string_view::npos ? s.size() : e + 1;
    out += s.substr(q, end - q);
    at = end;
  }
  return out;
}

std::vector<std::string> words_of(std::string_view s) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at < s.size()) {
    const size_t b = s.find_first_not_of(" \t", at);
    if (b == std::string_view::npos) break;
    const size_t e = std::min(s.find_first_of(" \t", b), s.size());
    out.emplace_back(s.substr(b, e - b));
    at = e;
  }
  return out;
}

std::string joined(const std::vector<std::string>& w, size_t from) {
  std::string out;
  for (size_t i = from; i < w.size(); i++) out += (out.empty() ? "" : " ") + w[i];
  return out;
}

// A row's options: which it takes, read off its words before the first positional one.
struct OptionSpec {
  std::string_view name;
  char brief;
  bool value;
};
struct Options {
  std::map<std::string, std::string> set;  // by long name; a flag's value is ""
  size_t first = 1;                        // the first positional word
  std::string fault;                       // the player's words when the options are wrong
};

Options read_options(const std::vector<std::string>& w, std::span<const OptionSpec> spec) {
  Options o;
  size_t i = 1;
  for (; i < w.size(); i++) {
    const std::string& a = w[i];
    if (a == "--") {
      i++;
      break;
    }
    if (a.size() < 2 || a[0] != '-') break;
    std::string name, value;
    bool inline_value = false;
    const OptionSpec* s = nullptr;
    if (a[1] == '-') {
      const size_t eq = a.find('=');
      name = a.substr(2, eq == std::string::npos ? std::string::npos : eq - 2);
      if (eq != std::string::npos) {
        value = a.substr(eq + 1);
        inline_value = true;
      }
      for (const OptionSpec& x : spec)
        if (x.name == name) s = &x;
    } else if (a.size() == 2) {
      for (const OptionSpec& x : spec)
        if (x.brief == a[1]) s = &x;
    }
    if (s == nullptr) {
      o.fault = a + " is not one of " + w[0] + "'s options";
      return o;
    }
    if (!s->value && inline_value) {
      o.fault = a + " takes no value";
      return o;
    }
    if (s->value && !inline_value) {
      if (i + 1 >= w.size()) {
        o.fault = a + " needs a value";
        return o;
      }
      value = w[++i];
    }
    o.set[std::string(s->name)] = value;
  }
  o.first = i;
  return o;
}

constexpr OptionSpec kStatus[] = {{"status", 's', true}};
constexpr OptionSpec kAction[] = {{"status", 's', true}, {"align", 'a', false}};
constexpr OptionSpec kBitmap[] = {{"status", 's', true}, {"altname", 'N', true}};
constexpr OptionSpec kBind[] = {{"status", 's', true}, {"altstatus", 'S', true}};
constexpr OptionSpec kField[] = {{"status", 's', true},
                                 {"center", 'c', false},
                                 {"integer", 'i', false},
                                 {"numeric", 'n', false},
                                 {"width", 'w', true}};
constexpr OptionSpec kStyle[] = {{"compact", 'c', false},
                                 {"no-compact", 'C', false},
                                 {"transparent", 't', false},
                                 {"no-transparent", 'T', false}};

constexpr std::string_view kStates[] = {"normal", "active", "selection", "disabled"};

// The first command of a line, if it is the only one: `pushmenu <page>` opens that page.
std::string pushed_page(std::string_view line) {
  const std::vector<std::string> w = ge::console::tokenize(line);
  if (w.size() != 2 || ge::console::lower(w[0]) != "pushmenu" ||
      line.find(';') != std::string_view::npos)
    return {};
  return std::string(script_page(ge::console::lower(w[1])));
}

}  // namespace

std::string_view script_page(std::string_view name) {
  static constexpr std::pair<std::string_view, std::string_view> kNames[] = {
      {"singleplayer", "single"},   {"multiplayer", "multi"}, {"startserver", "host"},
      {"addressbook", "addresses"}, {"players", "setup"},     {"loadgame", "load"},
      {"savegame", "save"}};
  for (const auto& [script, ours] : kNames)
    if (name == script) return ours;
  return name;
}

std::string_view ScriptMenu::keep(std::string s) { return text_.emplace_back(std::move(s)); }

// One read of a script and the files it includes.
struct ScriptMenu::Reader {
  ScriptMenu& out;
  const ScriptFiles& files;
  const ScriptLookup& lookup;
  std::set<std::string, std::less<>> server_cvars =
      {};  // settings the game's table sets on the server

  struct Page {
    std::string title;
    std::vector<Entry> rows;
  };
  std::map<std::string, Page, std::less<>> pages = {};
  std::vector<std::string> order = {};  // the pages in the order they were first defined
  std::string open = {};                // the page being read, "" outside one
  Page page = {};
  int row_id = 0;

  void warn(const std::string& where, int line, const std::string& what) {
    out.warnings_.push_back(where + " line " + std::to_string(line) + ": " + what);
  }

  // A row of setting `cvar`, set on the server when the game's own table sets it there.
  Entry setting(std::string_view text, std::string_view cvar, ItemKind kind) {
    Entry e{.page = {}, .id = out.keep(std::string(cvar)), .text = out.keep(std::string(text))};
    e.kind = kind;
    e.needs = e.id;
    e.target = server_cvars.contains(cvar) ? Target::server : Target::client;
    return e;
  }
  std::string_view next_id(std::string_view stem) {
    return out.keep(std::string(stem) + std::to_string(row_id++));
  }
  bool taken(std::string_view id) const {
    return std::any_of(page.rows.begin(), page.rows.end(),
                       [&](const Entry& e) { return e.id == id; });
  }

  // `values` and `strings`: items, an item `$name` the name's words (one item each).
  std::vector<std::string> items_of(const std::vector<std::string>& w, size_t from) {
    std::vector<std::string> out_items;
    for (size_t i = from; i < w.size(); i++) {
      const std::string& it = w[i];
      if (it.size() > 1 && it[0] == '$' && it[1] == '$') {
        out_items.push_back(it.substr(1));
      } else if (it.size() > 1 && it[0] == '$') {
        std::string name = it.substr(1), value;
        if (name.size() > 2 && name.front() == '{' && name.back() == '}')
          name = name.substr(1, name.size() - 2);
        if (lookup && lookup(name, value))
          for (std::string& word : words_of(value)) out_items.push_back(std::move(word));
      } else {
        out_items.push_back(it);
      }
    }
    return out_items;
  }

  void add(Entry e) {
    if (e.needs.empty() && !e.line.empty())
      e.needs = out.keep(ge::console::tokenize(e.line).empty()
                             ? std::string()
                             : ge::console::lower(ge::console::tokenize(e.line)[0]));
    page.rows.push_back(e);
  }

  // One row; false when the line is not a row word.
  bool row(const std::vector<std::string>& w, const std::string& where, int line) {
    const std::string kw = ge::console::lower(w[0]);
    const auto usage = [&](std::string_view form) {
      warn(where, line,
           kw + " is written " + kw + " " + std::string(form) + ", so the row is left out.");
    };
    const auto options = [&](std::span<const OptionSpec> spec, Options& o) {
      o = read_options(w, spec);
      if (o.fault.empty()) return true;
      warn(where, line, o.fault + ", so the row is left out.");
      return false;
    };
    const auto hint = [&](const Options& o, Entry& e) {
      if (auto s = o.set.find("status"); s != o.set.end()) e.hint = out.keep(s->second);
    };
    Options o;
    if (kw == "values" || kw == "strings") {
      if (!options(kStatus, o)) return true;
      if (w.size() < o.first + 3) {
        usage("<name> <setting> <choice> [choice ...]");
        return true;
      }
      if (taken(w[o.first + 1])) return true;
      Entry e = setting(w[o.first], w[o.first + 1], ItemKind::choice);
      hint(o, e);
      std::vector<Choice>& cs = out.choices_.emplace_back();
      const std::vector<std::string> items = items_of(w, o.first + 2);
      for (size_t i = 0; i < items.size(); i++) {
        const std::string_view text = out.keep(items[i]);
        cs.push_back({text, kw == "values" ? out.keep(std::to_string(i)) : text});
      }
      e.choices = cs;
      add(e);
      return true;
    }
    if (kw == "pairs") {
      if (!options(kStatus, o)) return true;
      const size_t n = w.size() - std::min(w.size(), o.first + 2);
      if (w.size() < o.first + 4 || n % 2 != 0) {
        usage("<name> <setting> <shown> <value> [<shown> <value> ...]");
        return true;
      }
      if (taken(w[o.first + 1])) return true;
      Entry e = setting(w[o.first], w[o.first + 1], ItemKind::choice);
      hint(o, e);
      std::vector<Choice>& cs = out.choices_.emplace_back();
      for (size_t i = o.first + 2; i + 1 < w.size(); i += 2)
        cs.push_back({out.keep(w[i]), out.keep(w[i + 1])});
      e.choices = cs;
      add(e);
      return true;
    }
    if (kw == "range") {
      if (!options(kStatus, o)) return true;
      double lo = 0, hi = 0, step = 0;
      const bool ok = w.size() >= o.first + 4 && w.size() <= o.first + 5 &&
                      ge::console::parse_number(w[o.first + 2], lo) &&
                      ge::console::parse_number(w[o.first + 3], hi) &&
                      (w.size() == o.first + 4 || ge::console::parse_number(w[o.first + 4], step));
      if (!ok || lo == hi) {
        usage("<name> <setting> <min> <max> [step]");
        return true;
      }
      if (taken(w[o.first + 1])) return true;
      Entry e = setting(w[o.first], w[o.first + 1], ItemKind::slider);
      hint(o, e);
      if (w.size() == o.first + 4) step = (hi - lo) / 10;
      e.reversed = lo > hi;  // the number falls to the right
      e.min = std::min(lo, hi);
      e.max = std::max(lo, hi);
      e.step = std::abs(step);
      add(e);
      return true;
    }
    if (kw == "toggle") {
      if (!options(kStatus, o)) return true;
      if (w.size() < o.first + 2 || w.size() > o.first + 3) {
        usage("<name> <setting> [~][bit]");
        return true;
      }
      bool inverted = false;
      int bit = -1;
      if (w.size() == o.first + 3) {
        std::string_view b = w[o.first + 2];
        if (!b.empty() && b[0] == '~') {
          inverted = true;
          b.remove_prefix(1);
        }
        if (!b.empty()) {
          bit = std::atoi(std::string(b).c_str());
          if (bit < 0 || bit > 31 || b.find_first_not_of("0123456789") != std::string_view::npos) {
            warn(where, line,
                 "a toggle's bit is 0 to 31, not " + std::string(b) + ", so the row is left out.");
            return true;
          }
        }
      }
      const std::string& cvar = w[o.first + 1];
      if (bit < 0 && taken(cvar)) return true;
      Entry e = setting(w[o.first], cvar, ItemKind::toggle);
      if (bit >= 0) e.id = out.keep(cvar + "." + std::to_string(bit));  // the needs names the cvar
      if (bit >= 0 && taken(e.id)) return true;
      e.bit = bit;
      e.inverted = inverted;
      hint(o, e);
      add(e);
      return true;
    }
    if (kw == "bind") {
      if (!options(kBind, o)) return true;
      if (w.size() < o.first + 2) {
        usage("<name> <command>");
        return true;
      }
      Entry e{.page = {}, .id = next_id("bind"), .text = out.keep(w[o.first])};
      e.kind = ItemKind::choice;
      e.bind = out.keep(joined(w, o.first + 1));
      hint(o, e);
      add(e);
      return true;
    }
    if (kw == "action" || kw == "bitmap") {
      if (!options(kw == "action" ? std::span<const OptionSpec>(kAction) : kBitmap, o)) return true;
      if (w.size() < o.first + 2) {
        usage("<name> <command>");
        return true;
      }
      std::string text = w[o.first];
      if (kw == "bitmap") {  // a picture's name: its last word (`m_main_game` is "game")
        const size_t slash = text.find_last_of("/\\");
        if (slash != std::string::npos) text = text.substr(slash + 1);
        const size_t dot = text.rfind('.');
        if (dot != std::string::npos) text = text.substr(0, dot);
        const size_t under = text.rfind('_');
        if (under != std::string::npos && under + 1 < text.size()) text = text.substr(under + 1);
      }
      Entry e{.page = {}, .id = next_id(kw), .text = out.keep(text)};
      e.line = out.keep(joined(w, o.first + 1));
      hint(o, e);
      add(e);
      return true;
    }
    if (kw == "savegame" || kw == "loadgame") {
      if (!options(kStatus, o)) return true;
      if (w.size() != o.first + 1) {
        usage("<slot>");
        return true;
      }
      const bool save = kw == "savegame";
      Entry e{.page = {}, .id = next_id(kw), .text = out.keep(w[o.first])};
      e.line = out.keep(std::string(save ? "save " : "load ") + w[o.first]);
      e.needs = save ? "save" : "load";
      e.closes = true;
      e.hint = save ? "Save the game here." : "Load this game.";
      hint(o, e);
      add(e);
      return true;
    }
    if (kw == "blank") {
      add({.page = {}, .id = next_id("blank"), .text = {}, .kind = ItemKind::label});
      return true;
    }
    if (kw == "field") {
      if (!options(kField, o)) return true;
      const bool centred = o.set.contains("center");
      const size_t want = centred ? 1 : 2;
      if (w.size() != o.first + want) {
        usage(centred ? "-c <setting>" : "<name> <setting>");
        return true;
      }
      if (auto width = o.set.find("width"); width != o.set.end()) {
        const int n = std::atoi(width->second.c_str());
        if (n < 1 || n > 32) {
          warn(where, line, "a field's width is 1 to 32, so the row is left out.");
          return true;
        }
      }
      const std::string& cvar = w[o.first + want - 1];
      if (taken(cvar)) return true;
      Entry e = setting(centred ? cvar : w[o.first], cvar, ItemKind::action);
      e.target = Target::none;  // typed in the console, not set by the row
      e.field = true;
      e.line = "toggleconsole";
      e.closes = true;
      e.hint = out.keep("Type it in the console: " + cvar + ", then the value.");
      hint(o, e);
      add(e);
      return true;
    }
    return false;
  }

  void close_page(const std::string& where, int line) {
    if (page.rows.empty()) {
      warn(where, line, "the page " + open + " has no rows, so it is left out.");
    } else {
      if (!pages.contains(open)) order.push_back(open);
      pages[open] = std::move(page);
    }
    page = {};
    open.clear();
  }

  // One file; false when the rest of everything is not read.
  bool file(std::string_view name, std::string_view text, int depth) {
    const std::string where(name);
    for (const Logical& l : logical_lines(text, where, out.warnings_)) {
      if (l.text.size() > kMaxLine) {
        warn(where, l.line, "the line is too long, so it is skipped.");
        continue;
      }
      const std::string line = expand_unquoted(l.text, lookup);
      if (line.size() > kMaxLine) {
        warn(where, l.line, "the line grows too long when expanded, so it is skipped.");
        continue;
      }
      const std::vector<std::string> w = ge::console::tokenize(line);
      if (w.empty()) continue;
      const std::string kw = ge::console::lower(w[0]);
      if (!open.empty()) {
        if (kw == "end") {
          close_page(where, l.line);
        } else if (kw == "title") {
          page.title = joined(w, 1);
        } else if (kw == "banner") {
          if (w.size() != 2) warn(where, l.line, "banner is written banner <picture>.");
          if (page.title.empty() && w.size() == 2) page.title = "\x01" + w[1];  // ours by default
        } else if (kw == "plaque") {
          if (w.size() < 2 || w.size() > 3)
            warn(where, l.line, "plaque is written plaque <picture> [logo].");
        } else if (kw == "background") {
          if (w.size() != 2) warn(where, l.line, "background is written background <colour>.");
        } else if (kw == "style") {
          Options o = read_options(w, kStyle);
          if (!o.fault.empty()) warn(where, l.line, o.fault + ".");
        } else if (!row(w, where, l.line)) {
          warn(where, l.line, "'" + w[0] + "' is not a menu script word, so the line is skipped.");
        }
        continue;
      }
      if (kw == "begin") {
        if (w.size() < 2) {
          warn(where, l.line, "begin needs a page's name, so the rest of the file is not read.");
          return false;
        }
        open = std::string(script_page(ge::console::lower(w[1])));
        page = {};
        continue;
      }
      if (kw == "include") {
        if (w.size() < 2) {
          warn(where, l.line, "include needs a file's name, so the rest of the file is not read.");
          return false;
        }
        if (depth + 1 >= kMaxIncludes) {
          warn(where, l.line, "includes go too deep, so " + w[1] + " is not read.");
          continue;
        }
        const std::optional<std::string> inner = files ? files(w[1]) : std::nullopt;
        if (!inner) {
          warn(where, l.line, w[1] + " is not in the game's files, so it is not included.");
          continue;
        }
        file(w[1], *inner, depth + 1);
        continue;
      }
      if (kw == "background" || kw == "font" || kw == "cursor" || kw == "weapon") {
        if (w.size() != 2)
          warn(where, l.line,
               std::string(kw).append(" is written ").append(kw).append(" <picture>."));
        continue;
      }
      if (kw == "color") {
        if (w.size() != 3)
          warn(where, l.line, "color is written color <state> <colour>.");
        else if (std::find(std::begin(kStates), std::end(kStates), ge::console::lower(w[1])) ==
                 std::end(kStates))
          warn(where, l.line,
               "'" + w[1] + "' is not a state (normal, active, selection or disabled).");
        continue;
      }
      warn(where, l.line,
           "'" + w[0] + "' is not a menu script word, so the rest of the file is not read.");
      return false;
    }
    if (!open.empty()) {
      warn(where, 0, "the page " + open + " has no end, so it is left out.");
      page = {};
      open.clear();
    }
    return true;
  }
};

void ScriptMenu::read(std::string_view name, std::string_view text, const MenuTable& base,
                      const ScriptFiles& files, const ScriptLookup& lookup) {
  text_.clear();
  choices_.clear();
  page_list_.clear();
  rows_.clear();
  warnings_.clear();
  Reader r{.out = *this, .files = files, .lookup = lookup};
  for (const Entry& e : base.entries)
    if (e.target == Target::server) r.server_cvars.emplace(e.bit >= 0 ? e.needs : e.id);
  r.file(name, text, 0);
  // The script's pages the game draws itself are left to it.
  std::erase_if(r.order, [](const std::string& p) { return ours_only(p); });
  pages_read_ = static_cast<int>(r.order.size());

  std::set<std::string_view> replaced, ids;
  for (const std::string& p : r.order) replaced.insert(p);
  for (const PageTitle& p : base.pages) {
    ids.insert(p.id);
    if (!replaced.contains(p.id)) {
      page_list_.push_back(p);
      continue;
    }
    const std::string& title = r.pages[std::string(p.id)].title;
    // A script's own title, else ours (a banner names a picture ours draws in words).
    page_list_.push_back({p.id, title.empty() || title[0] == '\x01' ? p.title : keep(title)});
  }
  for (const std::string& p : r.order) {
    if (ids.contains(p)) continue;
    const std::string& title = r.pages[p].title;
    const std::string_view id = keep(p);
    ids.insert(id);
    page_list_.push_back({id, title.empty() || title[0] == '\x01' ? id : keep(title)});
  }
  for (const Entry& e : base.entries) {
    // The pages the script did not replace, and SELECT GAME first on the root.
    if (!replaced.contains(e.page) || (e.page == "main" && e.id == "games")) rows_.push_back(e);
  }
  for (const std::string& p : r.order) {
    const std::string_view page = *ids.find(p);
    for (Entry e : r.pages[p].rows) {
      e.page = page;
      // `pushmenu <page>` alone opens the page, ours or the script's.
      if (const std::string to = pushed_page(e.line);
          !to.empty() && (ids.contains(to) || ours_only(to))) {
        e.opens = keep(to);
        e.line = {};
        e.needs = {};
      }
      rows_.push_back(e);
    }
  }
  table_ = base;
  table_.pages = page_list_;
  table_.entries = rows_;
  table_.over = base.over != nullptr ? base.over : &base;
}

}  // namespace shooters_edge::menu
