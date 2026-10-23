// The drop-down console's model, defined: its cvars and commands, the slide, the scrollback, the
// input line's editing, completion, history and the submit rule.
//
// ge:summary The drop-down console's definitions: slide, scrollback, editing, the submit rule
// ge:keywords console view submit complete history scroll notify say whisper toggleconsole
// ge:skill console
// ge:test tests/console_view_test.cpp
#include "console/view.hpp"

#include <algorithm>

#include "console/lex.hpp"

namespace ge::console {

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
          .help = "1: a line typed without a / is said to everyone (a multiplayer game sets it)",
          .kind = CvarKind::boolean},
         &chat_default_);
  if (v.find("name") == nullptr) {
    v.add({.name = "name",
           .fallback = "player",
           .flags = kArchive | kUserInfo,
           .help = "the name others see",
           .kind = CvarKind::text});
  }

  // The key under the tilde opens and closes it, in the game and in the console itself.
  for (BindMode m : {BindMode::game, BindMode::console})
    if (c.binds().get(m, "`").empty()) c.binds().set(m, "`", "toggleconsole");

  c.set_sink([this](std::string_view line) { add(LineKind::output, line); });
  c.add_command("toggleconsole", "toggleconsole: the console up or down",
                [this](Console&, const Args&) { toggle(); });
  c.add_command("clear", "clear: empty the console's scrollback",
                [this](Console&, const Args&) { clear(); });
  c.add_command(
      "say", "say <text>: a chat line to everyone",
      [this](Console&, const Args& a) { say(ChatTo::all, "", a.rest(0)); }, kStuffable);
  c.add_command(
      "say_team", "say_team <text>: a chat line to your team",
      [this](Console&, const Args& a) { say(ChatTo::team, "", a.rest(0)); }, kStuffable);
  c.add_command(
      "say_prox", "say_prox <text>: a chat line to the players near you",
      [this](Console&, const Args& a) { say(ChatTo::nearby, "", a.rest(0)); }, kStuffable);
  c.add_command("whisper", "whisper <nick> <text>: a chat line to one player",
                [this](Console& con, const Args& a) {
                  if (a.size() < 3) {
                    con.error("whisper needs a nick and a text", "write `whisper <nick> <text>`");
                    return;
                  }
                  say(ChatTo::one, a[1], a.rest(1));
                });
}

double ConsoleView::cover() const { return drop_ * height_; }

void ConsoleView::frame(double dt_s) {
  clock_ += dt_s;
  const double target = open_ ? 1.0 : 0.0;
  if (speed_ <= 0) {
    drop_ = target;
    return;
  }
  // con_speed is screens a second; the drop is in shares of con_height.
  const double step = dt_s * speed_ / std::max(height_, 0.1);
  drop_ = drop_ < target ? std::min(target, drop_ + step) : std::max(target, drop_ - step);
}

void ConsoleView::add(LineKind kind, std::string_view text) {
  // A long line arrives as one; a line with newlines as several.
  size_t at = 0;
  while (at <= text.size()) {
    const size_t nl = text.find('\n', at);
    const std::string_view part =
        text.substr(at, nl == std::string_view::npos ? text.npos : nl - at);
    lines_.push_back({kind, std::string(part), clock_});
    if (kind != LineKind::output && kind != LineKind::command) c_.fire_triggers(part);
    if (nl == std::string_view::npos) break;
    at = nl + 1;
  }
  while (static_cast<int32_t>(lines_.size()) > keep_) lines_.pop_front();
}

std::vector<const ViewLine*> ConsoleView::page(int rows) const {
  std::vector<const ViewLine*> out;
  const int n = static_cast<int>(lines_.size());
  const int end = std::max(0, n - scroll_);
  for (int i = std::max(0, end - rows); i < end; ++i)
    out.push_back(&lines_[static_cast<size_t>(i)]);
  return out;
}

std::vector<const ViewLine*> ConsoleView::notify() const {
  std::vector<const ViewLine*> out;
  for (auto it = lines_.rbegin(); it != lines_.rend(); ++it) {
    if (static_cast<int32_t>(out.size()) >= notify_lines_) break;
    if (clock_ - it->time_s > notify_time_) break;
    if (it->kind == LineKind::command) continue;
    out.push_back(&*it);
  }
  std::reverse(out.begin(), out.end());
  return out;
}

void ConsoleView::type(std::string_view text) {
  for (char ch : text) {
    if (ch == '\n' || ch == '\r' || ch == '`') continue;  // the toggle key types nothing
    input_.insert(cursor_++, 1, ch);
  }
  tabbed_ = false;
}

void ConsoleView::key(EditKey k) {
  const bool was_tab = tabbed_;
  tabbed_ = false;
  switch (k) {
    case EditKey::enter:
      submit();
      break;
    case EditKey::backspace:
      if (cursor_ > 0) input_.erase(--cursor_, 1);
      break;
    case EditKey::del:
      if (cursor_ < input_.size()) input_.erase(cursor_, 1);
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
      tabbed_ = was_tab;
      complete();
      tabbed_ = true;
      break;
    case EditKey::up:
    case EditKey::down: {
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
      open_ = false;
      break;
  }
}

void ConsoleView::submit() {
  const std::string line = input_;
  input_.clear();
  cursor_ = 0;
  history_at_ = -1;
  scroll_ = 0;
  if (line.find_first_not_of(" \t") == std::string::npos) return;
  if (history_.empty() || history_.back() != line) history_.push_back(line);
  const bool slash = line[0] == '/';
  if (slash || !chat_default_) {
    const std::string cmd = slash ? line.substr(1) : line;
    add(LineKind::command, "] " + cmd);
    c_.append(cmd, Source::local, {"console", 0});
    return;
  }
  say(ChatTo::all, "", line);
}

void ConsoleView::complete() {
  // The word under completion is the command: the text before the first blank.
  const bool slash = !input_.empty() && input_[0] == '/';
  const std::string head = input_.substr(slash ? 1 : 0);
  if (head.find(' ') != std::string::npos) return;
  const std::vector<std::string> all = c_.complete(head);
  if (all.empty()) return;
  if (all.size() == 1) {
    input_ = (slash ? "/" : "") + all[0] + " ";
    cursor_ = input_.size();
    return;
  }
  // The longest prefix every match shares; a second Tab lists them.
  std::string common = all[0];
  for (const std::string& m : all) {
    size_t i = 0;
    while (i < common.size() && i < m.size() && common[i] == m[i]) ++i;
    common.resize(i);
  }
  input_ = (slash ? "/" : "") + common;
  cursor_ = input_.size();
  if (tabbed_) {
    std::string list;
    for (const std::string& m : all) list += (list.empty() ? "" : "  ") + m;
    add(LineKind::output, list);
  }
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
      add(LineKind::nearby, "{" + me + "}: " + std::string(text));
      break;
    case ChatTo::one:
      add(LineKind::whisper, "[to " + std::string(nick) + "]: " + std::string(text));
      break;
  }
}

}  // namespace ge::console
