// The console's machinery: the buffer and its run, dispatch, aliases, files, keys, output,
// problems and completion. The built-in commands are registered from their own files.
//
// ge:summary The console's definitions: the buffer, dispatch, files, keys, problems, completion
// ge:keywords console run dispatch exec_file append_command_line key suggest edit-distance
// ge:skill console
// ge:test tests/console_console_test.cpp
#include "console/console.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "console/lex.hpp"

namespace ge::console {

const std::string& Args::operator[](size_t i) const {
  static const std::string kNone;
  return i < words.size() ? words[i] : kNone;
}

std::string Args::rest(size_t i) const { return rest_after(raw, i + 1); }

Console::Console(bool is_server) : cvars_(is_server) {
  register_builtin_commands(*this);
  register_script(*this);
  register_bind_commands(*this);
}

Console::~Console() = default;

void Console::add_command(std::string_view name, std::string_view help, CommandFn fn,
                          uint32_t flags) {
  commands_[lower(name)] = Command{std::string(help), std::move(fn), flags};
}

bool Console::has_command(std::string_view name) const { return commands_.count(lower(name)) != 0; }

void Console::add_macro(std::string_view name, std::function<std::string()> fn) {
  macros_[lower(name)] = std::move(fn);
}

Button& Console::add_button(std::string_view name, std::string_view help) {
  const std::string n = lower(name);
  auto& slot = buttons_[n];
  if (!slot) slot = std::make_unique<Button>();
  Button* b = slot.get();
  add_command("+" + n, help, [b](Console&, const Args&) {
    ++b->held;
    b->pressed = true;
  });
  add_command("-" + n, help, [b](Console&, const Args&) {
    if (b->held > 0) --b->held;
  });
  return *b;
}

void Console::set_alias(std::string_view name, std::string_view body, bool temp) {
  aliases_[lower(name)] = Alias{std::string(body), temp};
}

bool Console::remove_alias(std::string_view name) { return aliases_.erase(lower(name)) > 0; }

const Origin* Console::created_at(std::string_view name) const {
  const auto it = created_.find(lower(name));
  return it == created_.end() ? nullptr : &it->second;
}

void Console::note_created(std::string_view name, const Origin& where) {
  created_.emplace(lower(name), where);
}

// ---- the buffer ----

void Console::append(std::string_view text, Source source, const Origin& where) {
  for (Line& l : split_commands(text)) {
    Origin o = where;
    if (!o.file.empty()) o.line = where.line + l.line - 1;
    buffer_.push_back({std::move(l.text), source, std::move(o), 0, {}});
  }
}

void Console::queue_front(std::string_view text, Source source, const Origin& where, int depth,
                          const std::string& via, bool keep_lines) {
  std::vector<Line> lines = split_commands(text);
  for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
    Origin o = where;
    if (keep_lines) o.line = it->line;
    buffer_.push_front({std::move(it->text), source, std::move(o), depth, via});
  }
}

void Console::insert(std::string_view text, Source source, const Origin& where) {
  queue_front(text, source, where, current_depth_, current_via_, false);
}

int Console::run() {
  int n = 0;
  waiting_ = false;
  for (Entry& e : buffer_) e.depth = 0;  // a loop across frames waits, so it is not runaway
  while (!buffer_.empty() && !waiting_) {
    if (n >= kMaxPerRun) {
      error("the command buffer ran " + std::to_string(kMaxPerRun) +
                " commands in one frame and stopped",
            "an alias loop with no `wait` in it: break it, or add a wait");
      buffer_.clear();
      break;
    }
    Entry e = std::move(buffer_.front());
    buffer_.pop_front();
    dispatch(e);
    ++n;
  }
  return n;
}

int Console::execute(std::string_view text, Source source) {
  append(text, source);
  int n = 0;
  int frames = 0;
  while (pending() && frames++ < kMaxFrames) n += run();
  return n;
}

void Console::run_now(std::string_view command, Source source) {
  Entry e{std::string(command), source, {}, current_depth_, current_via_};
  dispatch(e);
}

std::string Console::expand_text(std::string_view text) const {
  return expand(text, [this](std::string_view name, std::string& out) {
    const std::string key = lower(name);
    const auto m = macros_.find(key);
    if (m != macros_.end()) {
      out = m->second();
      return true;
    }
    const Cvar* c = cvars_.find(key);
    if (c == nullptr) return false;
    out = c->value;
    return true;
  });
}

void Console::dispatch(Entry& e) {
  if (e.depth > kMaxDepth) {
    Args where;
    where.where = e.where;
    current_ = &where;
    error("'" + e.via + "' calls itself more than " + std::to_string(kMaxDepth) +
              " times deep and was stopped",
          "break the loop in '" + e.via + "', or put a `wait` in it to run once a frame");
    current_ = nullptr;
    // Drop what the runaway queued at this depth or deeper.
    while (!buffer_.empty() && buffer_.front().depth > kMaxDepth) buffer_.pop_front();
    return;
  }
  std::vector<std::string> first = tokenize(e.text);
  if (first.empty()) return;
  const std::string name = lower(first[0]);
  const auto cmd = commands_.find(name);
  const bool expand_it = cmd == commands_.end() || (cmd->second.flags & kNoExpand) == 0;

  Args args;
  args.raw = expand_it ? expand_text(e.text) : e.text;
  args.words = expand_it ? tokenize(args.raw) : std::move(first);
  if (args.words.empty()) return;
  args.source = e.source;
  args.where = e.where;

  const Args* outer = current_;
  const int outer_depth = current_depth_;
  const std::string outer_via = current_via_;
  current_ = &args;
  current_depth_ = e.depth;
  current_via_ = e.via;

  if (cmd != commands_.end()) {
    if (e.source == Source::server && (cmd->second.flags & kStuffable) == 0) {
      error("the server may not run '" + name + "' here",
            "nothing to do: a server's stuffed text runs only the commands a game allows");
    } else {
      cmd->second.fn(*this, args);
    }
  } else if (const auto al = aliases_.find(name); al != aliases_.end()) {
    if (e.source == Source::server) {
      error("the server may not run the alias '" + name + "'",
            "nothing to do: a server's stuffed text runs no player alias");
    } else {
      // The body runs before what followed the call, one level deeper.
      const std::string body = substitute_args(al->second.body, args.words);
      queue_front(body, e.source, e.where, e.depth + 1, name, false);
    }
  } else if (Cvar* c = cvars_.find(name)) {
    if (args.size() == 1) {
      print("\"" + c->name + "\" is \"" + c->value + "\"" +
            (c->is_default() ? std::string(" (default)") : " (default \"" + c->fallback + "\")") +
            (c->has_latched ? " (latched \"" + c->latched + "\")" : std::string()));
    } else {
      const SetResult r = cvars_.set(*c, args[1], e.source);
      if (r == SetResult::latched)
        print(c->name + " will be \"" + c->latched + "\" after a restart (it is latched)");
      const std::string why = set_result_text(r, c->name);
      if (!why.empty()) error(why, "");
    }
  } else {
    const std::string near = suggest(name);
    error("unknown command '" + name + "'",
          near.empty() ? "see `cmdlist` and `cvarlist`" : "did you mean '" + near + "'?");
  }
  current_ = outer;
  current_depth_ = outer_depth;
  current_via_ = outer_via;
}

// ---- files ----

void Console::set_search_path(std::vector<std::filesystem::path> dirs) {
  search_ = std::move(dirs);
}

void Console::set_files(Reader read, Writer write) {
  reader_ = std::move(read);
  writer_ = std::move(write);
}

void Console::set_fallback_reader(Reader read) { fallback_ = std::move(read); }

bool Console::read_file(std::string_view name, std::string& text) const {
  if (reader_) return reader_(name, text);
  std::vector<std::filesystem::path> dirs = search_;
  if (dirs.empty()) dirs.emplace_back(".");
  for (const auto& d : dirs) {
    std::ifstream in(d / std::filesystem::path(std::string(name)), std::ios::binary);
    if (!in) continue;
    std::ostringstream ss;
    ss << in.rdbuf();
    text = ss.str();
    return true;
  }
  return fallback_ && fallback_(name, text);
}

bool Console::write_file(std::string_view name, std::string_view text) const {
  if (writer_) return writer_(name, text);
  const std::filesystem::path dir = search_.empty() ? std::filesystem::path(".") : search_[0];
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  std::ofstream out(dir / std::filesystem::path(std::string(name)), std::ios::binary);
  if (!out) return false;
  out << text;
  return static_cast<bool>(out);
}

bool Console::exec_file(std::string_view name, Source source, const Origin& from) {
  std::string file(name);
  if (std::filesystem::path(file).extension().empty()) file += ".cfg";
  std::string text;
  if (!read_file(file, text)) {
    Args where;
    where.where = from;
    const Args* outer = current_;
    if (current_ == nullptr) current_ = &where;
    error("couldn't exec " + file + ": not found",
          search_.empty() ? "check the name; files are read from the current directory"
                          : "check the name; files are read from " + search_[0].string());
    current_ = outer;
    return false;
  }
  queue_front(text, source, Origin{file, 1}, current_depth_ + 1, file, true);
  return true;
}

int Console::append_command_line(const std::vector<std::string>& args) {
  int n = 0;
  std::string cur;
  auto flush = [&] {
    if (!cur.empty()) {
      append(cur, Source::local, Origin{"command line", 0});
      ++n;
    }
    cur.clear();
  };
  for (const std::string& a : args) {
    if (!a.empty() && a[0] == '+' && a.size() > 1) {
      flush();
      cur = a.substr(1);
    } else if (!cur.empty()) {
      cur += " " + quote(a);
    }
  }
  flush();
  return n;
}

// ---- output and problems ----

void Console::set_sink(std::function<void(std::string_view)> sink) { sink_ = std::move(sink); }

void Console::print(std::string_view line) {
  if (sink_) {
    sink_(line);
  } else {
    output_.emplace_back(line);
  }
}

void Console::message(std::string_view line) {
  print(line);
  fire_triggers(line);
}

void Console::fire_triggers(std::string_view line) {
  for (const Trigger& t : triggers_) {
    if (line.find(t.match) == std::string_view::npos) continue;
    queue_front(t.alias, Source::local, {}, current_depth_ + 1, t.alias, false);
  }
}

void Console::error(std::string_view what, std::string_view next, Problem::Severity severity) {
  Problem p;
  p.severity = severity;
  if (current_ != nullptr) p.where = current_->where;
  p.what = std::string(what);
  p.next = std::string(next);
  std::string line;
  if (!p.where.file.empty()) line = p.where.file + ":" + std::to_string(p.where.line) + ": ";
  line += p.what;
  if (!p.next.empty()) line += " (" + p.next + ")";
  print(line);
  problems_.push_back(std::move(p));
}

// ---- keys ----

void Console::key(std::string_view name, bool down, BindMode mode) {
  std::string k;
  if (!normalize_key(name, k)) return;
  if (!down) {
    const auto it = held_.find(k);
    if (it == held_.end()) return;
    std::string up = it->second;
    up[0] = '-';
    held_.erase(it);
    append(up + " " + k);
    return;
  }
  std::string cmd = binds_.get(mode, k);
  if (cmd.empty()) {
    const size_t plus = k.rfind('+');
    if (plus != std::string::npos && plus + 1 < k.size())
      cmd = binds_.get(mode, k.substr(plus + 1));
  }
  if (cmd.empty()) return;
  if (cmd[0] == '+') {
    if (held_.count(k) != 0) return;  // a repeat of a held key
    const std::vector<std::string> w = tokenize(cmd);
    held_[k] = w.empty() ? cmd : w[0];
    append(cmd + " " + k);
    return;
  }
  append(cmd);
}

// ---- completion ----

std::vector<std::string> Console::complete(std::string_view prefix) const {
  const std::string p = lower(prefix);
  std::vector<std::string> out;
  auto take = [&](const std::string& n) {
    if (n.compare(0, p.size(), p) == 0) out.push_back(n);
  };
  for (const auto& [n, c] : commands_) take(n);
  for (const auto& [n, a] : aliases_) take(n);
  for (const Cvar* c : cvars_.sorted()) take(c->name);
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

namespace {

size_t edit_distance(std::string_view a, std::string_view b) {
  std::vector<size_t> row(b.size() + 1);
  for (size_t j = 0; j <= b.size(); ++j) row[j] = j;
  for (size_t i = 1; i <= a.size(); ++i) {
    size_t prev = row[0];
    row[0] = i;
    for (size_t j = 1; j <= b.size(); ++j) {
      const size_t cur = row[j];
      row[j] = std::min({row[j] + 1, row[j - 1] + 1, prev + (a[i - 1] == b[j - 1] ? 0 : 1)});
      prev = cur;
    }
  }
  return row[b.size()];
}

}  // namespace

std::string Console::suggest(std::string_view name) const {
  const std::string n = lower(name);
  std::string best;
  size_t best_d = 3;
  for (const std::string& c : complete("")) {
    if (c == n) continue;
    const size_t d = edit_distance(n, c);
    if (d < best_d) {
      best_d = d;
      best = c;
    }
  }
  return best;
}

}  // namespace ge::console
