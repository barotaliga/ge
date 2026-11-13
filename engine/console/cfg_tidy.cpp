// A cfg checked and tidied, defined: the text read into commands with their comments, the check,
// the plain, commented and pretty writers.
//
// ge:summary A cfg's check and format, defined: commands with their comments, sections, alignment
// ge:keywords cfg-check cfg-format check_cfg format_cfg sections align banner
// ge:skill console
// ge:test tests/console_cfg_tidy_test.cpp
#include "console/cfg_tidy.hpp"

#include <algorithm>
#include <deque>
#include <map>
#include <set>

#include "console/lex.hpp"
#include "console/words.hpp"
#include "core/num_text.hpp"

namespace ge::console {

namespace {

// One command of the text with the comments that go with it.
struct Item {
  std::string text;                // the command as written, trimmed
  std::vector<std::string> above;  // whole-line comments right above it, as written
  std::string beside;              // a comment after it on its line ("" none)
  int line = 0;
  int blank_before = 0;  // blank lines right above (and its comments), kept by the plain writer
};

std::string trim(std::string_view s) {
  size_t a = 0, b = s.size();
  while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
  return std::string(s.substr(a, b - a));
}

// `text` read into its commands, each with the comments round it; comments after the last command
// come back in `tail`.
std::vector<Item> read_items(std::string_view text, std::vector<std::string>& tail) {
  std::vector<Item> items;
  std::vector<std::string> pending;
  int blanks = 0, line_no = 0;
  size_t at = 0;
  while (at <= text.size()) {
    const size_t end = std::min(text.find('\n', at), text.size());
    const std::string_view line = text.substr(at, end - at);
    ++line_no;
    // The comment starts at `//` outside quotes; commands split at `;` outside quotes.
    std::vector<std::string> commands;
    std::string cur, comment;
    bool quoted = false;
    for (size_t i = 0; i < line.size(); ++i) {
      const char ch = line[i];
      if (ch == '"') quoted = !quoted;
      if (!quoted && ch == '/' && i + 1 < line.size() && line[i + 1] == '/') {
        comment = trim(line.substr(i));
        break;
      }
      if (!quoted && ch == ';') {
        commands.push_back(trim(cur));
        cur.clear();
        continue;
      }
      cur += ch;
    }
    commands.push_back(trim(cur));
    std::erase_if(commands, [](const std::string& c) { return c.empty(); });
    if (commands.empty()) {
      if (!comment.empty())
        pending.push_back(comment);
      else if (pending.empty())
        ++blanks;
      else
        pending.push_back("");  // a blank inside a run of comments is kept in it
    } else {
      for (size_t k = 0; k < commands.size(); ++k) {
        Item it;
        it.text = commands[k];
        it.line = line_no;
        if (k == 0) {
          it.above = std::move(pending);
          pending.clear();
          it.blank_before = blanks;
          blanks = 0;
        }
        if (k + 1 == commands.size()) it.beside = comment;
        items.push_back(std::move(it));
      }
    }
    if (end == text.size()) break;
    at = end + 1;
  }
  while (!pending.empty() && pending.back().empty()) pending.pop_back();
  tail = std::move(pending);
  return items;
}

std::string join_from(const std::vector<std::string>& words, size_t from) {
  std::string out;
  for (size_t i = from; i < words.size(); ++i) out += (i > from ? " " : "") + words[i];
  return out;
}

bool is_set_word(std::string_view w) { return w == "set" || w == "seta"; }

// What a command sets: the cvar (null when it sets none), the name as written, the value and
// whether one was given; the word before the name (`set`, `seta`, or "").
struct Setting {
  const Cvar* cvar = nullptr;
  std::string name, value, verb;
  bool has_value = false;
  bool is_setting = false;  // a set, or a word that names a cvar
  int part = -1;            // a part synonym's word of its target (its value is one word)
  bool synonym = false;     // the name is another name for the cvar
};

// The cvar a name sets: its own, or a synonym's target (a part synonym's too, which `find` does
// not answer); null for none or for one created at run time, which nothing reads.
const Cvar* cvar_named(const Console& c, std::string_view name, Setting& s) {
  if (const Synonym* syn = c.cvars().synonym(name); syn != nullptr && syn->target != nullptr) {
    s.synonym = true;
    s.part = syn->part;
    return syn->target;
  }
  const Cvar* v = c.cvars().find(name);
  return v != nullptr && (v->flags & kCreated) == 0 ? v : nullptr;
}

Setting setting_of(const Console& c, const std::vector<std::string>& words) {
  Setting s;
  if (words.empty()) return s;
  const std::string head = lower(words[0]);
  if (is_set_word(head) && words.size() >= 2) {
    s.verb = head;
    s.name = words[1];
    s.cvar = cvar_named(c, words[1], s);
    s.has_value = words.size() >= 3;
    s.value = join_from(words, 2);
    s.is_setting = true;
  } else if (const Cvar* v = cvar_named(c, words[0], s); v != nullptr) {
    s.name = words[0];
    s.cvar = v;
    s.has_value = words.size() >= 2;
    s.value = join_from(words, 1);
    s.is_setting = true;
  } else {
    s = Setting{};  // the synonym's marks, when the name was none
  }
  return s;
}

std::string takes_text(const Cvar& v) {
  if (!v.values.empty()) return "it takes " + v.values;
  if (v.kind == CvarKind::boolean) return "it takes 0 or 1";
  const bool low = v.min > -1e300, high = v.max < 1e300;
  if (low && high)
    return "it takes a number from " + core::num::to_string(v.min) + " to " +
           core::num::to_string(v.max);
  if (low) return "it takes a number of at least " + core::num::to_string(v.min);
  if (high) return "it takes a number of at most " + core::num::to_string(v.max);
  return "it takes a number";
}

}  // namespace

std::vector<CfgFinding> check_cfg(const Console& c, std::string_view text) {
  std::vector<std::string> tail;
  const std::vector<Item> items = read_items(text, tail);
  std::vector<CfgFinding> out;
  std::set<std::string> aliases;
  std::map<std::string, int> first;  // a setting's name to the line that last set it
  for (const Item& it : items) {
    const std::vector<std::string> words = tokenize(it.text);
    if (words.empty()) continue;
    const std::string head = lower(words[0]);
    if ((head == "alias" || head == "tempalias") && words.size() > 1) {
      aliases.insert(lower(words[1]));
      continue;
    }
    const Setting s = setting_of(c, words);
    if (!s.is_setting) {
      if (c.has_command(head) || c.aliases().count(head) != 0 || aliases.count(head) != 0) continue;
      const std::string near = c.suggest(head);
      out.push_back({it.line, "'" + words[0] + "' is no setting or command of this game",
                     near.empty() ? "remove the line, or check its spelling"
                                  : "did you mean '" + near + "'?"});
      continue;
    }
    if (s.cvar == nullptr) {
      const std::string near = c.suggest(s.name);
      out.push_back({it.line, "'" + s.name + "' is no setting of this game",
                     near.empty() ? "remove the line, or check its spelling"
                                  : "did you mean '" + near + "'?"});
      continue;
    }
    if (!s.has_value) continue;
    if (s.part < 0 && s.value.find('$') == std::string::npos && !value_fits(*s.cvar, s.value)) {
      out.push_back(
          {it.line, "'" + s.cvar->name + "' is \"" + s.value + "\", which is not one of its values",
           takes_text(*s.cvar)});
    }
    const std::string key = s.part < 0 ? s.cvar->name : lower(s.name);  // a part: its own word
    const auto [at, fresh] = first.emplace(key, it.line);
    if (!fresh) {
      out.push_back({at->second,
                     "'" + key + "' is set again on line " + std::to_string(it.line) +
                         ", which overrides this one",
                     "keep one of the two lines"});
      at->second = it.line;
    }
  }
  std::stable_sort(out.begin(), out.end(),
                   [](const CfgFinding& a, const CfgFinding& b) { return a.line < b.line; });
  return out;
}

std::string_view cfg_section(std::string_view name) {
  struct Prefix {
    std::string_view prefix, section;
  };
  // The sections in the order a pretty cfg writes them; a name matches its longest prefix.
  static constexpr Prefix kPrefixes[] = {
      {"name", "Player"},
      {"team", "Player"},
      {"topcolor", "Player"},
      {"bottomcolor", "Player"},
      {"skin", "Player"},
      {"rate", "Player"},
      {"hostname", "Server"},
      {"sv_", "Server"},
      {"maxclients", "Server"},
      {"maxspectators", "Server"},
      {"password", "Server"},
      {"spectator_password", "Server"},
      {"deathmatch", "Match"},
      {"teamplay", "Match"},
      {"fraglimit", "Match"},
      {"timelimit", "Match"},
      {"samelevel", "Match"},
      {"k_", "Match"},
      {"dmflags", "Match"},
      {"skill", "Match"},
      {"pm_", "Movement"},
      {"qtv_", "Relays"},
      {"cl_", "Client"},
      {"sensitivity", "Mouse and keys"},
      {"m_", "Mouse and keys"},
      {"in_", "Mouse and keys"},
      {"lookspring", "Mouse and keys"},
      {"lookstrafe", "Mouse and keys"},
      {"r_", "Picture"},
      {"gamma", "Picture"},
      {"ce_", "Classic enhancements"},
      {"vid_", "Video"},
      {"viewsize", "Screen"},
      {"scr_", "Screen"},
      {"hud_", "HUD"},
      {"con_", "Console"},
      {"volume", "Sound"},
      {"s_", "Sound"},
      {"menu_", "Menus"},
      {"sb_", "Menus"},
  };
  std::string_view best = "Other settings";
  size_t best_len = 0;
  for (const Prefix& p : kPrefixes) {
    const bool whole = p.prefix.back() != '_';
    const bool match =
        whole ? name == p.prefix : name.size() > p.prefix.size() && name.starts_with(p.prefix);
    if (match && p.prefix.size() > best_len) {
      best = p.section;
      best_len = p.prefix.size();
    }
  }
  return best;
}

namespace {

int section_rank(std::string_view section) {
  static constexpr std::string_view kOrder[] = {"Player",         "Server",  "Match",
                                                "Movement",       "Relays",  "Client",
                                                "Mouse and keys", "Picture", "Classic enhancements",
                                                "Video",          "Screen",  "HUD",
                                                "Console",        "Sound",   "Menus",
                                                "Other settings"};
  for (size_t i = 0; i < std::size(kOrder); ++i)
    if (kOrder[i] == section) return static_cast<int>(i);
  return static_cast<int>(std::size(kOrder));
}

// The comment column of a pretty cfg stops here: a longer setting line takes its comment one
// space after it rather than pushing every other comment far right.
constexpr size_t kCommentColumnMost = 40;

// A meaning without its default mark ("that rate (10 the default)" -> "that rate"), its first
// letter a capital.
std::string choice_meaning(std::string meaning) {
  if (const size_t at = meaning.find("the default)"); at != std::string::npos) {
    const size_t open = meaning.rfind('(', at);
    if (open != std::string::npos) {
      meaning.erase(open, at + std::string_view("the default)").size() - open);
      while (!meaning.empty() && meaning.back() == ' ') meaning.pop_back();
    }
  }
  if (!meaning.empty() && meaning[0] >= 'a' && meaning[0] <= 'z')
    meaning[0] = static_cast<char>(meaning[0] - 'a' + 'A');
  return meaning;
}

// A setting's default as a cfg writes it: its own, or when that is empty (left to its owner, a
// game's rule) the one its words name.
std::string default_of(const Cvar& v) {
  return v.fallback.empty() ? stated_default(v.values) : v.fallback;
}

// A setting's comment: its help, then each of its own values with what it does (the values it
// names, not a number, text or range of any kind: "1: Classic, 2: Enhanced."), then its default: "// How to render shadows.
// 1: Classic, 2: Enhanced. (Default: 1)."
std::string help_comment(const Cvar& v) {
  if (v.help.empty()) return "";
  std::string out = "// " + v.help;
  std::vector<ValueEntry> entries;
  std::string choices;
  if (parse_values(v.values, entries))
    for (const ValueEntry& e : entries) {
      if (e.pattern == "N" || e.pattern == "TEXT" || e.pattern.find("..") != std::string::npos)
        continue;  // a number or text of any kind, or a range: the help says what it is
      const std::string meaning = choice_meaning(e.meaning);
      if (meaning.empty()) continue;
      choices += (choices.empty() ? "" : ", ") + e.pattern + ": " + meaning;
    }
  if (!choices.empty()) out += " " + choices + (choices.ends_with('.') ? "" : ".");
  if (const std::string d = default_of(v); !d.empty()) out += " (Default: " + d + ").";
  return out;
}

std::string banner(std::string_view title) {
  const std::string rule = "//" + std::string(70, '=');
  return rule + "\n// " + std::string(title) + "\n" + rule + "\n";
}

// A setting written back: `set`/`seta` kept, the name as written, the value quoted, padded.
std::string setting_line(const Setting& s, size_t pad) {
  std::string head = s.verb.empty() ? s.name : s.verb + " " + s.name;
  if (!s.has_value) return head;
  if (head.size() < pad) head.append(pad - head.size(), ' ');
  return head + " \"" + s.value + "\"";
}

// Whether a comment line is part of a banner the pretty writer adds (its rules and title).
bool banner_line(const std::string& line) {
  if (line.starts_with("//==")) return true;
  if (!line.starts_with("// ")) return false;
  const std::string_view title = std::string_view(line).substr(3);
  return section_rank(title) < section_rank("Other settings") || title == "Other settings" ||
         title == "Binds" || title == "Aliases" || title == "Commands";
}

// The comments above an item, less a help comment the commented writers add themselves (any
// line that starts with the setting's help: what an earlier writer put there, with or without
// its values and default) and, for the pretty writer, its banners and the blank lines between.
std::vector<std::string> kept_above(const Item& it, const Cvar* help_of, bool pretty) {
  const std::string help = help_of != nullptr && !help_of->help.empty() ? "// " + help_of->help : "";
  std::vector<std::string> out;
  for (const std::string& line : it.above) {
    if (!help.empty() && line.starts_with(help)) continue;
    if (pretty && (line.empty() || banner_line(line))) continue;
    out.push_back(line);
  }
  return out;
}

void put_comments(std::string& out, const std::vector<std::string>& lines) {
  for (const std::string& l : lines) out += l + "\n";
}

std::string with_beside(const std::string& line, const Item& it) {
  return it.beside.empty() ? line : line + " " + it.beside;
}

}  // namespace

std::string format_cfg(const Console& c, std::string_view text, const CfgFormat& how) {
  std::vector<std::string> tail;
  const std::vector<Item> items = read_items(text, tail);
  const bool comments = how.comments || how.pretty;
  std::string out;
  if (!how.pretty) {
    for (size_t i = 0; i < items.size(); ++i) {
      const Item& it = items[i];
      if (it.blank_before > 0 && !out.empty()) out += "\n";
      const Setting s = setting_of(c, tokenize(it.text));
      const std::string help = s.cvar != nullptr ? help_comment(*s.cvar) : "";
      put_comments(out, kept_above(it, comments ? s.cvar : nullptr, false));
      if (comments && !help.empty()) out += help + "\n";
      out += with_beside(s.is_setting ? setting_line(s, 0) : it.text, it) + "\n";
    }
    if (!tail.empty()) out += (out.empty() ? "" : "\n");
    put_comments(out, tail);
    return out;
  }
  // Pretty: settings by section and name, then binds by key, aliases, the other commands.
  struct Placed {
    const Item* item;
    Setting s;
    size_t order;
  };
  std::map<int, std::vector<Placed>> sections;  // by rank
  std::map<int, std::string> titles;
  std::vector<Placed> binds, aliases, others;
  for (size_t i = 0; i < items.size(); ++i) {
    const std::vector<std::string> words = tokenize(items[i].text);
    const std::string head = words.empty() ? "" : lower(words[0]);
    Placed p{&items[i], setting_of(c, words), i};
    if (p.s.synonym && p.s.part < 0) p.s.name = p.s.cvar->name;  // the game's own name
    if (p.s.is_setting && p.s.cvar != nullptr) {
      const std::string_view sec = cfg_section(p.s.cvar->name);
      sections[section_rank(sec)].push_back(p);
      titles[section_rank(sec)] = std::string(sec);
    } else if (p.s.is_setting) {
      sections[section_rank("Other settings")].push_back(p);
      titles[section_rank("Other settings")] = "Other settings";
    } else if (head == "bind") {
      binds.push_back(p);
    } else if (head == "alias" || head == "tempalias") {
      aliases.push_back(p);
    } else {
      others.push_back(p);
    }
  }
  // `defaults`: every other setting of those flags, at its default, in its section.
  std::deque<Item> made;  // the made items, kept in place while `sections` points at them
  if (how.defaults != 0) {
    std::set<const Cvar*> set;
    for (const auto& [rank, list] : sections)
      for (const Placed& p : list)
        if (p.s.cvar != nullptr) set.insert(p.s.cvar);
    for (const Cvar* v : c.cvars().sorted()) {
      if ((v->flags & how.defaults) == 0 || (v->flags & (kReadOnly | kCreated)) != 0) continue;
      if (set.count(v) != 0) continue;
      // Written at its default: its own, or the one its words name when that is left to its
      // owner (a game's rule).
      const std::string value = default_of(*v);
      Item& it = made.emplace_back();
      it.text = v->name + " \"" + value + "\"";
      Setting s;
      s.cvar = v;
      s.name = v->name;
      s.value = value;
      s.has_value = true;
      s.is_setting = true;
      const std::string_view sec = cfg_section(v->name);
      sections[section_rank(sec)].push_back({&it, s, items.size() + made.size()});
      titles[section_rank(sec)] = std::string(sec);
    }
  }
  const auto key = [](const Placed& p) {
    return p.s.cvar != nullptr && p.s.part < 0 ? p.s.cvar->name : lower(p.s.name);
  };
  // One column for every value in the file.
  size_t pad = 0;
  for (const auto& [rank, list] : sections)
    for (const Placed& p : list)
      pad = std::max(pad, (p.s.verb.empty() ? 0 : p.s.verb.size() + 1) + p.s.name.size());
  // Each setting one line: the name, its value in one column, then its help (and a comment of
  // its own after it) in another, as far right as the longest line that is not overlong.
  struct Row {
    std::string head, comment;
    std::vector<std::string> above;
  };
  std::map<int, std::vector<Row>> rows;
  size_t column = 0;
  for (auto& [rank, list] : sections) {
    std::stable_sort(list.begin(), list.end(),
                     [&](const Placed& a, const Placed& b) { return key(a) < key(b); });
    for (const Placed& p : list) {
      const std::string help = p.s.cvar != nullptr ? help_comment(*p.s.cvar) : "";
      std::string own = p.item->beside;  // less the help a pretty writer put there before
      if (!help.empty() && own.starts_with(help)) own = trim(own.substr(help.size()));
      Row l{setting_line(p.s, pad),
             help.empty() ? own : (own.empty() ? help : help + " " + own),
             kept_above(*p.item, p.s.cvar, true)};
      if (l.head.size() <= kCommentColumnMost) column = std::max(column, l.head.size());
      rows[rank].push_back(std::move(l));
    }
  }
  for (const auto& [rank, list] : rows) {
    out += (out.empty() ? "" : "\n") + banner(titles[rank]) + "\n";
    for (const Row& l : list) {
      put_comments(out, l.above);
      std::string line = l.head;
      if (!l.comment.empty()) {
        line.append(line.size() < column ? column - line.size() : 0, ' ');
        line += " " + l.comment;
      }
      out += line + "\n";
    }
  }
  const auto group = [&](std::string_view title, std::vector<Placed>& list, bool by_key) {
    if (list.empty()) return;
    if (by_key) {
      std::stable_sort(list.begin(), list.end(), [](const Placed& a, const Placed& b) {
        const auto wa = tokenize(a.item->text), wb = tokenize(b.item->text);
        return lower(wa.size() > 1 ? wa[1] : "") < lower(wb.size() > 1 ? wb[1] : "");
      });
    }
    out += (out.empty() ? "" : "\n") + banner(title) + "\n";
    for (const Placed& p : list) {
      put_comments(out, kept_above(*p.item, nullptr, true));
      out += with_beside(p.item->text, *p.item) + "\n";
    }
  };
  group("Binds", binds, true);
  group("Aliases", aliases, false);
  group("Commands", others, false);
  std::vector<std::string> kept;
  for (const std::string& line : tail)
    if (!line.empty() && !banner_line(line)) kept.push_back(line);
  if (!kept.empty()) out += "\n";
  put_comments(out, kept);
  return out;
}

}  // namespace ge::console
