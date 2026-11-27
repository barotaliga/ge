// The drop-down console's model, defined: its cvars and commands, the slide, the scrollback, the
// input line's editing, completion, history and the submit rule.
//
// ge:summary The drop-down console's definitions: slide, scrollback, editing, the submit rule
// ge:keywords console view submit complete history scroll notify say whisper toggleconsole
// ge:skill console
// ge:test tests/console_view_test.cpp
#include "console/view.hpp"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>

#include "console/lex.hpp"
#include "console/tab_list.hpp"
#include "console/words.hpp"

namespace ge::console {

namespace {

// The local time now in `format` (strftime's words); "" when the format is empty or writes more
// than a console line's worth.
std::string local_stamp(const std::string& format) {
  if (format.empty()) return {};
  const std::time_t now = std::time(nullptr);
  std::tm local{};
#if defined(_WIN32)
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  char out[128];
  const size_t n = std::strftime(out, sizeof out, format.c_str(), &local);
  return std::string(out, n);
}

}  // namespace

std::string_view line_kind_name(LineKind k) {
  switch (k) {
    case LineKind::output:
      return "output";
    case LineKind::command:
      return "command";
    case LineKind::chat:
      return "chat";
    case LineKind::team:
      return "team";
    case LineKind::whisper:
      return "whisper";
    case LineKind::nearby:
      return "nearby";
    case LineKind::server:
      return "server";
    case LineKind::event:
      return "event";
  }
  return "output";
}

ConsoleView::ConsoleView(Console& c) : c_(c) {
  Cvars& v = c.cvars();
  v.bind({.name = "con_height",
          .fallback = "0.5",
          .flags = kArchive,
          .help = "the share of the screen the console covers when down",
          .min = 0.1,
          .max = 1},
         &height_);
  v.bind({.name = "con_speed",
          .fallback = "3",
          .flags = kArchive,
          .help = "how fast the console slides, screens a second (0: at once)",
          .min = 0,
          .max = 100},
         &speed_);
  v.bind({.name = "con_lines",
          .fallback = "1024",
          .flags = kArchive,
          .help = "the lines of scrollback kept",
          .kind = CvarKind::integer,
          .min = 64,
          .max = 65536},
         &keep_);
  v.bind(
      {.name = "con_notify",
       .fallback = "1",
       .flags = kArchive,
       .help = "whether the newest lines show at the top of the screen while the console is closed",
       .kind = CvarKind::boolean},
      &notify_on_);
  v.bind({.name = "con_clearnotify",
          .fallback = "1",
          .flags = kArchive,
          .help = "whether opening or closing the console lets the notify lines so far go",
          .kind = CvarKind::boolean},
         &clear_notify_);
  v.bind({.name = "con_notifylines",
          .fallback = "4",
          .flags = kArchive,
          .help = "the newest lines shown at the top while the console is up (0: none)",
          .kind = CvarKind::integer,
          .min = 0,
          .max = 32},
         &notify_lines_);
  v.bind({.name = "con_notifytime",
          .fallback = "4",
          .flags = kArchive,
          .help = "how long a notify line stays, s",
          .min = 0,
          .max = 60},
         &notify_time_);
  v.bind({.name = "con_chat",
          .fallback = "0",
          .help = "what a line typed without a / is: a command, or said",
          .kind = CvarKind::integer,
          .min = 0,
          .max = 3,
          .values = "0: a command (the default); 1: said to everyone (a multiplayer game sets "
                    "it); 2: a command when its first word is a command, alias or setting, else "
                    "said; 3: said to the team"},
         &chat_default_);
  v.bind({.name = "con_suggest",
          .fallback = "8",
          .flags = kArchive,
          .help = "the suggestions listed under the input line while a command is typed (0: none)",
          .kind = CvarKind::integer,
          .min = 0,
          .max = 32},
         &suggest_);
  v.bind({.name = "con_bgtop",
          .fallback = "10141c",
          .flags = kArchive,
          .help = "the panel's colour at its top, rrggbb",
          .kind = CvarKind::text},
         &bg_top_);
  v.bind({.name = "con_bgbottom",
          .fallback = "10141c",
          .flags = kArchive,
          .help = "the panel's colour at its bottom, rrggbb (con_bgtop's: a flat fill)",
          .kind = CvarKind::text},
         &bg_bottom_);
  v.bind({.name = "con_bgalpha",
          .fallback = "0.85",
          .flags = kArchive,
          .help = "how much of the game the panel hides, 0 (none) to 1 (all)",
          .min = 0,
          .max = 1},
         &bg_alpha_);
  v.bind({.name = "con_bgimage",
          .fallback = "",
          .flags = kArchive,
          .help = "a picture drawn over the gradient, a path in the game's files (\"\": none)",
          .kind = CvarKind::text},
         &bg_image_);
  v.bind({.name = "con_font",
          .fallback = "mono",
          .flags = kArchive,
          .help = "the charset the console writes in: terminal, matrix, segment, mono, typewriter, "
                  "square, stencil, gothic, rounded or marker (\"\": the UI font)",
          .kind = CvarKind::text},
         &font_);
  v.bind({.name = "con_scale",
          .fallback = "0",
          .flags = kArchive,
          .help = "how large the console's text is",
          .min = 0,
          .max = 4,
          .values = "0: its own size (the default); 0.5..4: that many times it"},
         &scale_);
  v.bind({.name = "con_clock",
          .fallback = "0",
          .flags = kArchive,
          .help = "whether the console shows the time of day in its corner",
          .kind = CvarKind::boolean,
          .values = "0: no clock (the default); 1: the local hours and minutes"},
         &clock_on_);
  v.bind({.name = "con_history",
          .fallback = "128",
          .flags = kArchive,
          .help = "how many typed lines Up and Down walk back through",
          .kind = CvarKind::integer,
          .min = 0,
          .max = 4096,
          .values = "0: only the last line, none kept across runs; 1..4096: that many, the "
                    "oldest dropped first, kept across runs (128 the default)"},
         &history_max_);
  v.bind({.name = "con_scroll",
          .fallback = "0",
          .flags = kArchive,
          .help = "what brings the console back to its newest lines once scrolled back",
          .kind = CvarKind::integer,
          .min = 0,
          .max = 3,
          .values = "0: only Page Down or a line typed and run (the default); 1: a new line; 2: "
                    "a key typed; 3: both"},
         &scroll_back_);
  if (v.find("name") == nullptr) {
    v.add({.name = "name",
           .fallback = "player",
           .flags = kArchive | kUserInfo,
           .help = "the name others see",
           .kind = CvarKind::text});
  }

  // The key under the tilde opens and closes it, in the game and in the console itself, and so
  // does the ISO key: a Mac that does not recognise its ISO keyboard reports the key under Escape
  // (§ on a Nordic layout) as that key.
  for (BindMode m : {BindMode::game, BindMode::console})
    for (const char* key : {"`", "nonus"})
      if (c.binds().get(m, key).empty()) c.binds().set(m, key, "toggleconsole");

  c.set_sink([this](std::string_view line) { add(LineKind::output, line); });
  c.add_command("toggleconsole", "toggleconsole: the console up or down",
                [this](Console&, const Args&) { toggle(); });
  c.add_command("clear", "clear: empty the console's scrollback",
                [this](Console&, const Args&) { clear(); });
  c.add_command("clearnotify", "clearnotify: the lines at the top of the screen taken away",
                [this](Console&, const Args&) { clear_notify(); });
  c.add_command(
      "remotemode",
      "remotemode [address [password]]: lines typed go to a server's console (rcon); again to "
      "stop",
      [this](Console& con, const Args& a) {
        if (remote_ && a.size() < 2) {
          remote_ = false;
          con.print("Remote mode off.");
          return;
        }
        if (!con.has_command("rcon")) {
          con.print("There is no remote console in this game.");
          return;
        }
        const ge::console::Cvar* to = con.cvars().find("rcon_address");
        if (a.size() < 2 && (to == nullptr || to->value.empty())) {  // no server to send to
          con.print("Usage: " + a[0] + " <address> <password>");
          return;
        }
        if (a.size() > 1 && con.cvars().find("rcon_address") != nullptr)
          con.cvars().set("rcon_address", a[1], Source::local);
        if (a.size() > 2 && con.cvars().find("rcon_password") != nullptr)
          con.cvars().set("rcon_password", a[2], Source::local);
        remote_ = true;
        con.print("Remote mode on: what you type goes to the server. Type remotemode to stop.");
      });
  c.add_command("condump", "condump [name]: the console's lines written to a text file",
                [this](Console& con, const Args& a) {
                  const std::string name = a.size() > 1 ? a[1] : "condump";
                  std::string why;
                  if (dump(name, why))
                    con.print("Dumped console text to " + why + ".");
                  else
                    con.print(why);
                });
}

int ConsoleView::chat_mode() const {
  if (game_chat < 0) return chat_default_;
  return game_chat == 2 ? 3 : std::min(game_chat, 1);
}

bool ConsoleView::dump(std::string_view name, std::string& why) const {
  if (dump_folder_.empty()) {
    why = "The console can't be written to a file here.";
    return false;
  }
  if (name.empty() || name.find_first_of("/\\:") != std::string_view::npos ||
      name.find("..") != std::string_view::npos) {
    why = "Give the file a plain name, without a folder.";
    return false;
  }
  std::string file(name);
  if (!file.ends_with(".txt")) file += ".txt";
  const std::filesystem::path path = std::filesystem::path(dump_folder_) / file;
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  std::ofstream out(path, std::ios::binary);
  for (const ViewLine& l : lines_) out << l.stamp << l.text << '\n';
  out.close();
  if (!out) {
    why = "Couldn't write " + path.string() + ".";
    return false;
  }
  why = path.string();
  return true;
}

bool parse_rgb(std::string_view text, Rgb& out) {
  if (!text.empty() && text[0] == '#') text.remove_prefix(1);
  if (text.size() != 6) return false;
  uint8_t ch[3] = {};
  for (int i = 0; i < 6; ++i) {
    const char x = text[static_cast<size_t>(i)];
    int d = -1;
    if (x >= '0' && x <= '9') d = x - '0';
    if (x >= 'a' && x <= 'f') d = x - 'a' + 10;
    if (x >= 'A' && x <= 'F') d = x - 'A' + 10;
    if (d < 0) return false;
    ch[i / 2] = static_cast<uint8_t>(ch[i / 2] * 16 + d);
  }
  out = {ch[0], ch[1], ch[2]};
  return true;
}

Background ConsoleView::background() const {
  Background b;
  parse_rgb(bg_top_, b.top);
  parse_rgb(bg_bottom_, b.bottom);
  b.alpha = bg_alpha_;
  b.image = bg_image_;
  return b;
}

double ConsoleView::cover() const { return std::max(drop_ * height_, over_); }

void ConsoleView::set_full(bool full) {
  full_ = full;
  if (full) over_ = 1;
}

void ConsoleView::frame(double dt_s) {
  clock_ += dt_s;
  const double target = open_ ? 1.0 : 0.0;
  if (speed_ <= 0) {
    drop_ = target;
    over_ = full_ ? 1 : 0;
    return;
  }
  // Let go of the whole screen: it slides up at con_speed, the drop meeting it on the way.
  if (!full_) over_ = std::max(0.0, over_ - dt_s * speed_);
  // con_speed is screens a second; the drop is in shares of con_height.
  const double step = dt_s * speed_ / std::max(height_, 0.1);
  drop_ = drop_ < target ? std::min(target, drop_ + step) : std::max(target, drop_ - step);
}

void ConsoleView::add(LineKind kind, std::string_view text) {
  // A long line arrives as one; a line with newlines as several. A game's print ends in its
  // newline ("You got the shells\n"): that newline ends the line, it opens no empty one after it
  // (so a print of "\n" alone is one empty line).
  if (!text.empty() && text.back() == '\n') text.remove_suffix(1);
  const std::string stamp = stamps ? local_stamp(stamp_format) : std::string();
  size_t at = 0;
  while (at <= text.size()) {
    const size_t nl = text.find('\n', at);
    const std::string_view part =
        text.substr(at, nl == std::string_view::npos ? text.npos : nl - at);
    lines_.push_back({kind, std::string(part), clock_, added_++, stamp});
    if (kind != LineKind::output && kind != LineKind::command) c_.fire_triggers(part);
    if (nl == std::string_view::npos) break;
    at = nl + 1;
  }
  while (static_cast<int32_t>(lines_.size()) > keep_) lines_.pop_front();
  if ((scroll_back_ & 1) != 0) scroll_ = 0;  // con_scroll 1: a new line brings the newest back
}

bool ConsoleView::rewrite(uint64_t n, std::string_view text) {
  if (lines_.empty() || n < lines_.front().n || n > lines_.back().n) return false;
  lines_[static_cast<size_t>(n - lines_.front().n)].text = std::string(text);
  return true;
}

bool ConsoleView::has_selection() const {
  return selecting_ &&
         (select_from_.line != select_to_.line || select_from_.column != select_to_.column);
}

std::string ConsoleView::selected_text() const {
  std::string out;
  if (!has_selection() || lines_.empty()) return out;
  const TextSpot a = selection_first(), b = selection_last();
  for (const ViewLine& l : lines_) {
    if (l.n < a.line || l.n > b.line) continue;
    const size_t from = l.n == a.line ? std::min(a.column, l.text.size()) : 0;
    const size_t to = l.n == b.line ? std::min(b.column, l.text.size()) : l.text.size();
    if (l.n != a.line) out += '\n';
    if (to > from) out.append(l.text, from, to - from);
  }
  return out;
}

std::vector<const ViewLine*> ConsoleView::page(int rows) const {
  std::vector<const ViewLine*> out;
  const int n = static_cast<int>(lines_.size());
  const int end = std::max(0, n - scroll_);
  for (int i = std::max(0, end - rows); i < end; ++i)
    out.push_back(&lines_[static_cast<size_t>(i)]);
  return out;
}

void ConsoleView::set_open(bool open) {
  if (open != open_ && clear_notify_) notify_from_ = added_;
  open_ = open;
}

std::vector<const ViewLine*> ConsoleView::notify() const {
  std::vector<const ViewLine*> out;
  if (!notify_on_ || host_notify_) return out;
  for (auto it = lines_.rbegin(); it != lines_.rend(); ++it) {
    if (static_cast<int32_t>(out.size()) >= notify_lines_) break;
    if (clock_ - it->time_s > notify_time_ || it->n < notify_from_) break;
    if (it->kind == LineKind::command) continue;
    out.push_back(&*it);
  }
  std::reverse(out.begin(), out.end());
  return out;
}

void ConsoleView::type(std::string_view text) {
  if (chatting_) {
    for (char ch : text) {
      if (ch == '\n' || ch == '\r') continue;
      chat_input_.insert(chat_cursor_++, 1, ch);
    }
    return;
  }
  if ((scroll_back_ & 2) != 0 && !text.empty()) scroll_ = 0;  // con_scroll 2: a key typed
  for (char ch : text) {
    if (ch == '\n' || ch == '\r' || ch == '`') continue;  // the toggle key types nothing
    input_.insert(cursor_++, 1, ch);
  }
  selected_ = -1;
  history_at_ = -1;  // a recalled line edited is the line typed: its list shows again
}

void ConsoleView::key(EditKey k) {
  if (chatting_) {
    chat_key(k);
    return;
  }
  if ((scroll_back_ & 2) != 0 && k != EditKey::page_up && k != EditKey::page_down) scroll_ = 0;
  switch (k) {
    case EditKey::enter: {
      // Enter on a selected cvar fills it in for its value; on anything else it runs it.
      if (selected_ >= 0) {
        const std::vector<Suggestion> s = suggestions();
        const bool cvar = selected_ < static_cast<int>(s.size()) &&
                          s[static_cast<size_t>(selected_)].kind == Suggestion::Kind::cvar;
        accept();
        if (cvar) break;
      }
      submit();
      break;
    }
    case EditKey::backspace:
      if (cursor_ > 0) input_.erase(--cursor_, 1);
      selected_ = -1;
      history_at_ = -1;
      break;
    case EditKey::del:
      if (cursor_ < input_.size()) input_.erase(cursor_, 1);
      selected_ = -1;
      history_at_ = -1;
      break;
    case EditKey::left:
      if (cursor_ > 0) --cursor_;
      break;
    case EditKey::right:
      if (cursor_ < input_.size()) ++cursor_;
      break;
    case EditKey::home:
      cursor_ = 0;
      break;
    case EditKey::end:
      cursor_ = input_.size();
      break;
    case EditKey::tab:
      if (list_mode >= 0 && selected_ < 0)  // a row the arrows chose: Tab takes it in any mode
        tab_list();
      else
        tab();
      break;
    case EditKey::up:
    case EditKey::down: {
      // Over a list of suggestions Down steps into it and down it (stopping at its last row), Up
      // back up it and, from its first row, out to the typed line to go on typing. With no row
      // selected, Up walks the history as it does with no list.
      const int n = static_cast<int>(suggestions().size());
      if (n > 0 && (k == EditKey::down || selected_ >= 0)) {
        if (k == EditKey::down)
          selected_ = std::min(selected_ + 1, n - 1);
        else
          --selected_;
        break;
      }
      selected_ = -1;
      if (history_.empty()) break;
      if (history_at_ < 0) typed_ = input_;
      const int last = static_cast<int>(history_.size()) - 1;
      if (k == EditKey::up) {
        history_at_ = history_at_ < 0 ? last : std::max(0, history_at_ - 1);
      } else if (history_at_ >= 0) {
        history_at_ = history_at_ >= last ? -1 : history_at_ + 1;
      }
      input_ = history_at_ < 0 ? typed_ : history_[static_cast<size_t>(history_at_)];
      cursor_ = input_.size();
      break;
    }
    case EditKey::page_up:
      scroll_ = std::min(scroll_ + 8, std::max(0, static_cast<int>(lines_.size()) - 1));
      break;
    case EditKey::page_down:
      scroll_ = std::max(0, scroll_ - 8);
      break;
    case EditKey::escape:
      // Text typed: Escape clears it first, and closes the console the next time (the console's
      // own key closes it and keeps the text).
      if (!input_.empty()) {
        input_.clear();
        cursor_ = 0;
        history_at_ = -1;
        selected_ = -1;
      } else {
        set_open(false);
      }
      break;
  }
}

void ConsoleView::submit() {
  const std::string line = input_;
  input_.clear();
  cursor_ = 0;
  history_at_ = -1;
  selected_ = -1;
  scroll_ = 0;
  if (line.find_first_not_of(" \t") == std::string::npos) return;
  remember(line);
  if (line.starts_with("//")) {  // a note to oneself: shown, never run or said
    add(LineKind::command, line);
    return;
  }
  const bool slash = line[0] == '/' || line[0] == '\\';
  if (remote_ && !slash) {  // remotemode: the server's console
    add(LineKind::command, "# " + line);
    c_.append("rcon " + line, Source::local, {"console", 0});
    return;
  }
  const int mode = chat_mode();
  if (slash || mode == 0 || (mode == 2 && names_something(line))) {
    const std::string cmd = slash ? line.substr(1) : line;
    const std::vector<std::string> words = tokenize(cmd);
    if (!words.empty()) ++uses_[lower(words[0])];
    add(LineKind::command, "] " + cmd);
    c_.append(cmd, Source::local, {"console", 0});
    return;
  }
  say(mode == 3 ? ChatTo::team : ChatTo::all, "", line);
}

bool ConsoleView::names_something(std::string_view line) const {
  const std::vector<std::string> words = tokenize(line);
  if (words.empty()) return false;
  const std::string w = lower(words[0]);
  return c_.has_command(w) || c_.aliases().count(w) != 0 || c_.cvars().find(w) != nullptr;
}

std::string ConsoleView::head() const {
  // Suggestions follow the command word: after a `/` or `\`, or bare when typed text may be a
  // command.
  const bool slash = !input_.empty() && (input_[0] == '/' || input_[0] == '\\');
  if (!slash && (remote_ || chat_mode() == 1 || chat_mode() == 3)) return "";
  std::string h = input_.substr(slash ? 1 : 0);
  if (h.empty() || h.find_first_of(" \t;") != std::string::npos) return "";
  return h;
}

namespace {

// Whether value `v` starts with the typed `part`, any case, a leading quote on either let be.
bool value_starts(std::string_view v, std::string_view part) {
  if (!v.empty() && v[0] == '"') v.remove_prefix(1);
  if (!part.empty() && part[0] == '"') part.remove_prefix(1);
  return lower(v).starts_with(lower(part));
}

std::string joined(const std::vector<std::string>& words, std::string_view between) {
  std::string out;
  for (const std::string& w : words) {
    if (!out.empty()) out += between;
    out += w;
  }
  return out;
}

// "alias a, b": the game spellings of cvar or command `name` ("" none).
std::string spellings_of(const Console& c, std::string_view name) {
  std::vector<std::string> names;
  for (const Synonym& s : c.cvars().synonyms())
    if (s.part < 0 && s.target != nullptr && s.target->name == name) names.push_back(s.name);
  for (const auto& [syn, target] : c.command_synonyms())
    if (target == name) names.push_back(syn);
  return names.empty() ? std::string() : "alias " + joined(names, ", ");
}

}  // namespace

bool ConsoleView::value_typed(const Cvar*& cvar, std::vector<std::string>& choices,
                              std::string& part, size_t& at) const {
  const bool slash = !input_.empty() && (input_[0] == '/' || input_[0] == '\\');
  if (!slash && (remote_ || chat_mode() == 1 || chat_mode() == 3)) return false;
  const size_t from = slash ? 1 : 0;
  const size_t space = input_.find(' ', from);
  if (space == std::string::npos || space == from) return false;
  const std::string name = input_.substr(from, space - from);
  const std::string rest = input_.substr(space + 1);
  if (name.find_first_of("\t;") != std::string::npos || rest.find(';') != std::string::npos)
    return false;
  cvar = c_.cvars().find(name);
  if (cvar != nullptr) {
    if (rest.find_first_of(" \t") != std::string::npos) return false;
    choices = value_choices(*cvar);
  } else {  // a word's own values (an item's name), quoted when it has a space
    choices = c_.arg_values(name);
    for (std::string& v : choices)
      if (v.find(' ') != std::string::npos) v.insert(0, 1, '"').push_back('"');
  }
  if (choices.empty()) return false;
  part = rest;
  at = space + 1;
  return true;
}

size_t ConsoleView::suggest_at() const {
  const Cvar* cvar = nullptr;
  std::vector<std::string> choices;
  std::string part;
  size_t at = 0;
  if (value_typed(cvar, choices, part, at)) return at;
  return !input_.empty() && (input_[0] == '/' || input_[0] == '\\') ? 1 : 0;
}

std::vector<Suggestion> ConsoleView::suggestions() const {
  std::vector<Suggestion> out = matches();
  if (suggest_ <= 0) out.clear();
  if (static_cast<int32_t>(out.size()) > suggest_) out.resize(static_cast<size_t>(suggest_));
  return out;
}

std::vector<Suggestion> ConsoleView::matches() const {
  std::vector<Suggestion> out;
  if (history_at_ >= 0) return out;  // a recalled line: the arrows walk on
  const Cvar* typed = nullptr;
  std::vector<std::string> choices;
  std::string part;
  size_t at = 0;
  if (value_typed(typed, choices, part, at)) {  // the values, in their own order
    for (const std::string& v : choices) {
      if (!value_starts(v, part)) continue;
      Suggestion s;
      s.name = v;
      s.kind = Suggestion::Kind::value;
      if (typed != nullptr) {
        s.detail = value_meaning(*typed, v);
        if (v == typed->value) s.detail = s.detail.empty() ? "now" : s.detail + " (now)";
      }
      out.push_back(std::move(s));
    }
    return out;
  }
  const std::string h = head();
  if (h.empty()) return out;
  const auto described = [&](Suggestion& s, const Cvar& v) {
    s.kind = Suggestion::Kind::cvar;
    s.detail = v.value;
    s.choices = joined(value_choices(v), ", ");
  };
  for (const std::string& name : c_.complete(h)) {
    Suggestion s;
    s.name = name;
    if (const Cvar* v = c_.cvars().find(name)) {
      described(s, *v);
    } else if (const auto a = c_.aliases().find(name); a != c_.aliases().end()) {
      s.kind = Suggestion::Kind::alias;
      s.detail = a->second.body;
    } else if (const auto m = c_.commands().find(name); m != c_.commands().end()) {
      s.detail = m->second.help;
    }
    if (s.kind != Suggestion::Kind::alias) s.note = spellings_of(c_, name);
    out.push_back(std::move(s));
  }
  // A game's spellings: its name for one of ours, said so.
  const std::string p = lower(h);
  const auto listed = [&out](const std::string& name) {
    return std::any_of(out.begin(), out.end(), [&](const Suggestion& s) { return s.name == name; });
  };
  for (const Synonym& syn : c_.cvars().synonyms()) {
    if (syn.part >= 0 || syn.target == nullptr || syn.target->retired ||
        syn.name.compare(0, p.size(), p) != 0 || listed(syn.name))
      continue;
    Suggestion s;
    s.name = syn.name;
    described(s, *syn.target);
    s.note = "alias of " + syn.target->name;
    out.push_back(std::move(s));
  }
  for (const auto& [syn, target] : c_.command_synonyms()) {
    if (syn.compare(0, p.size(), p) != 0 || listed(syn)) continue;
    Suggestion s;
    s.name = syn;
    if (const auto m = c_.commands().find(target); m != c_.commands().end())
      s.detail = m->second.help;
    s.note = "alias of " + target;
    out.push_back(std::move(s));
  }
  for (Suggestion& s : out) {
    const auto u = uses_.find(s.name);
    s.uses = u == uses_.end() ? 0 : u->second;
  }
  std::stable_sort(out.begin(), out.end(), [](const Suggestion& a, const Suggestion& b) {
    if (a.uses != b.uses) return a.uses > b.uses;
    if (a.name.size() != b.name.size()) return a.name.size() < b.name.size();
    return a.name < b.name;
  });
  return out;
}

std::string ConsoleView::shared() const {
  const std::vector<Suggestion> all = matches();
  if (all.empty()) return "";
  std::string common = all[0].name;
  for (const Suggestion& m : all) {
    size_t n = 0;
    while (n < common.size() && n < m.name.size() &&
           std::tolower(static_cast<unsigned char>(common[n])) ==
               std::tolower(static_cast<unsigned char>(m.name[n])))
      ++n;
    common.resize(n);
  }
  return common;
}

std::string ConsoleView::hint() const {
  const std::vector<Suggestion> s = suggestions();
  if (s.empty()) return "";
  const size_t typed = input_.size() - std::min(input_.size(), suggest_at());
  // What Tab adds: the selected row's rest, else the rest every match shares.
  const std::string name =
      selected_ >= 0
          ? s[static_cast<size_t>(std::min<int>(selected_, static_cast<int>(s.size()) - 1))].name
          : shared();
  return name.size() > typed ? name.substr(typed) : "";
}

void ConsoleView::tab() {
  const std::vector<Suggestion> all = matches();
  if (all.empty()) return;
  if (selected_ >= 0 || all.size() == 1) {  // a row chosen, or the one match: taken
    accept();
    return;
  }
  // Several: the word completed to what every match shares, the list kept open; with nothing
  // more to add, Tab steps into the list (Tab again takes the row, Down walks on).
  const std::string common = shared();
  const size_t at = std::min(input_.size(), suggest_at());
  if (common.size() > input_.size() - at) {
    input_ = input_.substr(0, at) + common;
    cursor_ = input_.size();
  } else {
    selected_ = 0;
  }
}

bool ConsoleView::accept() {
  const std::vector<Suggestion> s = suggestions();
  if (s.empty()) return false;
  const Suggestion& pick = s[static_cast<size_t>(std::max(selected_, 0))];
  if (pick.kind == Suggestion::Kind::value) {
    input_ = input_.substr(0, suggest_at()) + pick.name;
  } else {
    const bool slash = input_[0] == '/';
    input_ = (slash ? "/" : "") + pick.name + " ";
  }
  cursor_ = input_.size();
  selected_ = -1;
  return true;
}

void ConsoleView::tab_list() {
  cursor_ = input_.size();
  const TabList t =
      ge::console::tab_list(c_, input_, head(), list_mode, list_threshold, tab_asked_);
  input_ = t.input;
  cursor_ = input_.size();
  if (!t.echo.empty()) add(LineKind::command, t.echo);
  for (const std::string& line : t.lines) add(LineKind::output, line);
}

void ConsoleView::say(ChatTo to, std::string_view nick, std::string_view text) {
  // `say "a b"` says a b: the quotes round the whole line are the command's, not the message's.
  if (text.size() >= 2 && text.front() == '"' && text.back() == '"')
    text = text.substr(1, text.size() - 2);
  if (text.empty()) return;
  if (chat_) {
    chat_(to, nick, text);
    return;
  }
  const Cvar* name = c_.cvars().find("name");
  const std::string me = name != nullptr ? name->value : "player";
  switch (to) {
    case ChatTo::all:
      add(LineKind::chat, me + ": " + std::string(text));
      break;
    case ChatTo::team:
      add(LineKind::team, "(" + me + "): " + std::string(text));
      break;
    case ChatTo::nearby:
      add(LineKind::nearby, me + " (nearby): " + std::string(text));
      break;
    case ChatTo::one:
      add(LineKind::whisper, "[to " + std::string(nick) + "]: " + std::string(text));
      break;
  }
}

void ConsoleView::open_chat(ChatTo to) {
  chatting_ = true;
  chat_to_ = to;
  chat_input_.clear();
  chat_cursor_ = 0;
}

void ConsoleView::close_chat() {
  chatting_ = false;
  chat_input_.clear();
  chat_cursor_ = 0;
}

std::string_view ConsoleView::chat_prompt() const {
  switch (chat_to_) {
    case ChatTo::team:
      return "Team:";
    case ChatTo::nearby:
      return "Nearby:";
    case ChatTo::one:
      return "Whisper:";
    case ChatTo::all:
      break;
  }
  return "Say:";
}

void ConsoleView::chat_key(EditKey k) {
  switch (k) {
    case EditKey::enter: {
      const std::string line = chat_input_;
      const ChatTo to = chat_to_;
      close_chat();
      if (line.find_first_not_of(" \t") != std::string::npos) say(to, "", line);
      break;
    }
    case EditKey::escape:
      close_chat();
      break;
    case EditKey::backspace:
      if (chat_cursor_ > 0) chat_input_.erase(--chat_cursor_, 1);
      break;
    case EditKey::del:
      if (chat_cursor_ < chat_input_.size()) chat_input_.erase(chat_cursor_, 1);
      break;
    case EditKey::left:
      if (chat_cursor_ > 0) --chat_cursor_;
      break;
    case EditKey::right:
      if (chat_cursor_ < chat_input_.size()) ++chat_cursor_;
      break;
    case EditKey::home:
      chat_cursor_ = 0;
      break;
    case EditKey::end:
      chat_cursor_ = chat_input_.size();
      break;
    default:  // Tab, the arrows up and down, the pages: nothing on one line
      break;
  }
}

void ConsoleView::remember(const std::string& line) {
  if (line.find_first_not_of(" \t") == std::string::npos) return;
  if (history_.empty() || history_.back() != line) history_.push_back(line);
  if (history_.size() > static_cast<size_t>(std::max(1, history_max_)))  // con_history
    history_.erase(history_.begin(),
                   history_.end() - static_cast<std::ptrdiff_t>(std::max(1, history_max_)));
}

void ConsoleView::load_history() {
  std::string text;
  if (history_max_ <= 0 || !c_.read_file(kHistoryFile, text)) return;
  for (size_t at = 0; at < text.size();) {
    size_t end = text.find('\n', at);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(at, end - at);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    remember(line);
    at = end + 1;
  }
}

bool ConsoleView::save_history() const {
  if (history_max_ <= 0) return true;
  std::string text;
  for (const std::string& line : history_) text += line + "\n";
  return c_.write_file(kHistoryFile, text);
}

}  // namespace ge::console
