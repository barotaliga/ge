// The console's machinery: the buffer and its run, dispatch, aliases, files, keys, output,
// problems and completion. The built-in commands are registered from their own files.
//
// ge:summary The console's definitions: the buffer, dispatch, files, keys, problems, completion
// ge:keywords console run dispatch exec_file append_command_line key suggest edit-distance
// ge:skill console
// ge:test tests/console_console_test.cpp
#include "console/console.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <utility>

#include "console/cfg.hpp"
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
  register_pattern_commands(*this);
  register_more_commands(*this);
  register_bind_commands(*this);
}

Console::~Console() = default;

void Console::add_command(std::string_view name, std::string_view help, CommandFn fn,
                          uint32_t flags) {
  commands_[lower(name)] = Command{std::string(help), std::move(fn), flags, cvars_.layer()};
}

bool Console::has_command(std::string_view name) const {
  return commands_.count(lower(name)) != 0 || command_synonyms_.count(lower(name)) != 0;
}

bool Console::remove_command(std::string_view name) { return commands_.erase(lower(name)) != 0; }

void Console::add_macro(std::string_view name, std::function<std::string()> fn, MacroKind kind) {
  std::string key = lower(name);
  if (kind == MacroKind::team)
    team_macros_.insert(key);
  else
    team_macros_.erase(key);
  macros_[std::move(key)] = std::move(fn);
}

bool Console::is_team_macro(std::string_view name) const {
  return team_macros_.count(lower(name)) != 0;
}

void Console::limit_team_macros(TeamMacroLimit limit) {
  for (std::string& a : limit.allowed) a = lower(a);
  macro_limit_ = std::move(limit);
}

const TeamMacroLimit* Console::team_macro_limit() const {
  if (!macro_limit_) return nullptr;
  return !macro_limit_->when || macro_limit_->when() ? &*macro_limit_ : nullptr;
}

void Console::quiet_triggers(TriggerQuiet quiet) { trigger_quiet_ = std::move(quiet); }

bool Console::triggers_quiet() const {
  return trigger_quiet_ && (!trigger_quiet_->when || trigger_quiet_->when());
}

void Console::limit_waits(std::string owner, int n, std::string why) {
  wait_owner_ = std::move(owner);
  wait_limit_ = n;
  wait_why_ = std::move(why);
  waits_in_row_ = 0;
}

std::vector<std::string> Console::macro_names() const {
  std::vector<std::string> out;
  out.reserve(macros_.size());
  for (const auto& [name, fn] : macros_) out.push_back(name);
  return out;
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
    buffer_.push_front({std::move(it->text), source, std::move(o), depth, via, held_by_team()});
  }
}

void Console::insert(std::string_view text, Source source, const Origin& where) {
  queue_front(text, source, where, current_depth_, current_via_, false);
}

void Console::sleep(double seconds) {
  waiting_ = true;
  if (clock_) wake_s_ = clock_() + (seconds > 0 ? seconds : 0);
}

int Console::run() {
  int n = 0;
  if (sleeping()) return 0;  // a recipe's `sleep`: nothing runs until the host's time passes it
  waiting_ = false;
  for (Entry& e : buffer_) e.depth = 0;  // a loop across frames waits, so it is not runaway
  while (!buffer_.empty() && !waiting_) {
    if (n >= kMaxPerRun) {
      error("the command buffer ran " + std::to_string(kMaxPerRun) +
                " commands in one frame and stopped",
            "an alias loops with no wait in it: break the loop, or add a wait");
      buffer_.clear();
      break;
    }
    Entry e = std::move(buffer_.front());
    buffer_.pop_front();
    // A line from a file is a config's (cvar.hpp § SCOPES: a game's spelling scopes its value).
    cvars_.set_reading_config(!e.where.file.empty());
    dispatch(e);
    cvars_.set_reading_config(false);
    ++n;
  }
  // A ruleset's wait limit: a script may not spread itself over frames past it.
  waits_in_row_ = waiting_ && !buffer_.empty() ? waits_in_row_ + 1 : 0;
  if (wait_limit_ > 0 && waits_in_row_ > wait_limit_) {
    buffer_.clear();
    waits_in_row_ = 0;
    print(wait_why_);
  }
  return n;
}

void Console::past_sleep(const std::function<void()>& work) {
  std::deque<Entry> held = std::exchange(buffer_, {});
  const double wake = std::exchange(wake_s_, -1e300);
  work();
  for (Entry& e : held) buffer_.push_back(std::move(e));  // after what the work left, if any
  wake_s_ = std::max(wake, wake_s_);
}

int Console::execute(std::string_view text, Source source) {
  if (sleeping()) {  // a recipe's `sleep` holds the buffer: the host's line runs now, past it
    int n = 0;
    past_sleep([&] { n = execute(text, source); });
    return n;
  }
  append(text, source);
  int n = 0;
  int frames = 0;
  while (pending() && frames++ < kMaxFrames) n += run();
  return n;
}

void Console::run_now(std::string_view command, Source source) {
  // Field by field: GCC 15 reads the braced form's `{}` origin as a string used uninitialized
  // (-Wmaybe-uninitialized), which -Werror stops.
  Entry e;
  e.text = std::string(command);
  e.source = source;
  e.depth = current_depth_;
  e.via = current_via_;
  e.team = held_by_team();
  dispatch(e);
}

std::string Console::expand_text(std::string_view text) const {
  return expand(text, [this](std::string_view name, std::string& out) {
    const std::string key = lower(name);
    const auto m = macros_.find(key);
    if (m != macros_.end()) {
      out = m->second();
      if (team_macros_.count(key) != 0) expanded_team_ = true;
    } else if (const Cvar* c = cvars_.find(key); c != nullptr) {
      out = c->value;
    } else {
      return false;
    }
    const Cvar* width = width_prefix_.empty() ? nullptr : cvars_.find(width_prefix_ + key);
    if (width != nullptr) {  // a column the player set: `tp_length_health 3`
      const int n = std::clamp(std::atoi(width->value.c_str()), 0, 40);
      const Cvar* align = cvars_.find(align_prefix_ + key);
      const char a =
          align == nullptr || align->value.empty()
              ? 'l'
              : static_cast<char>(std::tolower(static_cast<unsigned char>(align->value[0])));
      if (n > 0) out = fit_width(out, n, a);
    }
    return true;
  });
}

void Console::gate_command(std::string_view name, CommandGate gate) {
  gates_[lower(name)] = std::move(gate);
}

int Console::ungate(std::string_view owner) {
  int n = 0;
  if (macro_limit_ && macro_limit_->owner == owner) {
    macro_limit_.reset();
    ++n;
  }
  if (trigger_quiet_ && trigger_quiet_->owner == owner) {
    trigger_quiet_.reset();
    ++n;
  }
  if (wait_limit_ != 0 && wait_owner_ == owner) {
    limit_waits({}, 0, {});
    ++n;
  }
  for (auto it = gates_.begin(); it != gates_.end();)
    if (it->second.owner == owner) {
      it = gates_.erase(it);
      ++n;
    } else {
      ++it;
    }
  return n;
}

const CommandGate* Console::gate_of(std::string_view name) const {
  const auto g = gates_.find(lower(name));
  if (g == gates_.end()) return nullptr;
  return !g->second.when || g->second.when() ? &g->second : nullptr;
}

void Console::dispatch(Entry& e) {
  if (e.depth > kMaxDepth) {
    Args where;
    where.where = e.where;
    current_ = &where;
    error("'" + e.via + "' calls itself more than " + std::to_string(kMaxDepth) +
              " times deep and was stopped",
          "break the loop in '" + e.via + "', or put a wait in it to run once a frame");
    current_ = nullptr;
    // Drop what the runaway queued at this depth or deeper.
    while (!buffer_.empty() && buffer_.front().depth > kMaxDepth) buffer_.pop_front();
    return;
  }
  std::vector<std::string> first = tokenize(e.text);
  if (first.empty()) return;
  std::string name = lower(first[0]);
  // A `+` or `-` marks only a button's press and release. On a plain command (or alias) it is
  // dropped: `+coopgame 2` runs `coopgame 2`, and the `-coopgame` a key bound to it sends on its
  // release runs nothing.
  if (name.size() > 1 && (name[0] == '+' || name[0] == '-') && !known_name(name) &&
      known_name(name.substr(1))) {
    if (name[0] == '-') return;
    e.text.erase(e.text.find('+'), 1);
    first[0].erase(0, 1);
    name.erase(0, 1);
  }
  // A game's own spelling of one of our commands (`add_command_synonym`): ours runs.
  if (const auto syn = command_synonyms_.find(name);
      syn != command_synonyms_.end() && commands_.count(name) == 0)
    name = syn->second.target;
  const auto cmd = commands_.find(name);
  const bool expand_it = cmd == commands_.end() || (cmd->second.flags & kNoExpand) == 0;

  Args args;
  const bool outer_expanded = expanded_team_;
  expanded_team_ = false;
  args.raw = expand_it ? expand_text(e.text) : e.text;
  const bool used_team = expanded_team_ || e.team;
  args.words = expand_it ? tokenize(args.raw) : std::move(first);
  if (args.words.empty()) {
    expanded_team_ = outer_expanded;
    return;
  }
  args.source = e.source;
  args.where = e.where;

  const Args* outer = current_;
  const int outer_depth = current_depth_;
  const std::string outer_via = current_via_;
  const bool outer_team = current_team_;
  current_ = &args;
  current_depth_ = e.depth;
  current_via_ = e.via;

  const TeamMacroLimit* limit =
      used_team && e.source != Source::server ? team_macro_limit() : nullptr;
  if (limit != nullptr) current_team_ = true;  // what it runs (`if`'s branch) is held too
  if (limit != nullptr &&
      std::find(limit->allowed.begin(), limit->allowed.end(), name) == limit->allowed.end()) {
    print(name + limit->why);  // a ruleset's refusal reads as the game's words
  } else if (cmd != commands_.end()) {
    if (e.source == Source::server && (cmd->second.flags & kStuffable) == 0) {
      error("the server may not run '" + name + "' here",
            "nothing to do: a server's stuffed text runs only the commands a game allows");
    } else if (const CommandGate* g = e.source != Source::server ? gate_of(name) : nullptr; g) {
      print(g->why);  // a ruleset's refusal reads as the game's words
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
      const SetResult r = cvars_.set(name, args[1], e.source);
      if (r == SetResult::latched)
        print(c->name + " will be \"" + c->latched + "\" after a restart (it is latched)");
      const std::string why = set_result_text(r, *c);
      if (!why.empty()) error(why, "");
    }
  } else if (const Synonym* mapped = cvars_.synonym(name);
             mapped != nullptr && !mapped->values.empty()) {
    // A game's spelling with values of its own: what it sets in ours said beside it, typed or
    // asked, so the player sees the setting it reaches ("r_drawhud 1 (scr_draw2d: 2)").
    if (args.size() == 1) {
      print("\"" + mapped->name + "\" is \"" + cvars_.synonym_value(*mapped) + "\" (" +
            mapped->target->name + ": " + mapped->target->value + "); it takes " +
            Cvars::mapped_words(*mapped));
    } else {
      const SetResult r = cvars_.set(name, args[1], e.source);
      const std::string why = set_result_text(r, mapped->name);
      if (!why.empty())
        error(why, "its values: " + Cvars::mapped_words(*mapped));
      else if (e.where.file.empty())
        print(mapped->name + " " + args[1] + " (" + mapped->target->name + ": " +
              cvars_.mapped_value(*mapped, args[1]) + ")");
    }
  } else if (const Synonym* syn = cvars_.synonym(name)) {  // a part of another setting
    if (args.size() == 1) {
      print("\"" + syn->name + "\" is \"" + cvars_.synonym_value(*syn) + "\" (part of " +
            syn->target->name + " \"" + syn->target->value + "\")");
    } else {
      const std::string why = set_result_text(cvars_.set(name, args[1], e.source), syn->name);
      if (!why.empty()) error(why, "");
    }
  } else if (std::string scope, base; cvars_.split_scoped(name, scope, base)) {
    // `<game>:<name> [value]`: that game's own value of a setting (cvar.hpp § SCOPES).
    const Cvar* v = cvars_.find(base);
    if (v == nullptr) {
      error(set_result_text(SetResult::unknown, base), "");
    } else if (args.size() == 1) {
      const auto it = v->scoped.find(scope);
      print(it == v->scoped.end()
                ? scope + " has no value of its own for " + v->name + " (it uses \"" +
                      (v->scoped.empty() ? v->value : v->common) + "\")"
                : scope + ":" + v->name + " is \"" + it->second + "\"");
    } else {
      std::string value = args[1];
      for (size_t i = 2; i < args.size(); ++i) value += " " + args[i];
      const std::string why =
          set_result_text(cvars_.set_scoped(scope, base, value, e.source), v->name);
      if (!why.empty()) error(why, "");
    }
  } else if (unknown_ && unknown_(args)) {
    // the host's own command
  } else {
    const std::string near = suggest(name);
    error("unknown command '" + name + "'",
          near.empty() ? "type cmdlist or cvarlist for the names" : "did you mean '" + near + "'?");
  }
  current_ = outer;
  current_depth_ = outer_depth;
  current_via_ = outer_via;
  current_team_ = outer_team;
  expanded_team_ = outer_expanded;
}

bool Console::held_by_team() const {
  // A command that expands its own words (`if`) and met a team macro holds what it runs too.
  return current_team_ || (current_ != nullptr && expanded_team_ &&
                           current_->source != Source::server && team_macro_limit() != nullptr);
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
  if (!config_.empty() && name == kConfigFile) {
    std::ifstream in(config_, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    text = ss.str();
    return true;
  }
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
  if (!config_.empty() && name == kConfigFile) {
    std::error_code ec;
    if (config_.has_parent_path()) std::filesystem::create_directories(config_.parent_path(), ec);
    std::ofstream out(config_, std::ios::binary);
    if (!out) return false;
    out << text;
    return static_cast<bool>(out);
  }
  if (writer_) return writer_(name, text);
  const std::filesystem::path dir = search_.empty() ? std::filesystem::path(".") : search_[0];
  std::error_code ec;
  const std::filesystem::path path = dir / std::filesystem::path(std::string(name));
  std::filesystem::create_directories(path.parent_path(), ec);  // a folder in the name made too
  std::ofstream out(path, std::ios::binary);
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
  // The loaded game's own values beside it (name.<game>.cfg, cfg_save's pair) stand over it.
  if (const std::string own = game_file(file, cvars_.scope()); !own.empty()) {
    std::string mine;
    if (read_file(own, mine))
      queue_front(mine, source, Origin{own, 1}, current_depth_ + 1, own, true);
  }
  queue_front(text, source, Origin{file, 1}, current_depth_ + 1, file, true);
  return true;
}

std::string game_file(std::string_view file, std::string_view scope) {
  if (scope.empty()) return {};
  const std::filesystem::path p{std::string(file)};
  const std::string tail = "." + std::string(scope);
  const std::string stem = p.stem().string();
  if (stem.size() >= tail.size() && stem.compare(stem.size() - tail.size(), tail.size(), tail) == 0)
    return {};  // already the game's own file
  return (p.parent_path() / (stem + tail + p.extension().string())).string();
}

bool Console::known_name(const std::string& name) const {
  return commands_.count(name) != 0 || aliases_.count(name) != 0 ||
         command_synonyms_.count(name) != 0;
}

void Console::add_command_synonym(std::string_view name, std::string_view target) {
  command_synonyms_[lower(name)] = {lower(target), cvars_.layer()};
}

std::vector<std::pair<std::string, std::string>> Console::command_synonyms() const {
  std::vector<std::pair<std::string, std::string>> out;
  out.reserve(command_synonyms_.size());
  for (const auto& [name, s] : command_synonyms_) out.emplace_back(name, s.target);
  return out;
}

int Console::drop_layer(std::string_view layer) {
  const std::string key = lower(layer);
  const size_t names = command_synonyms_.size();
  std::erase_if(command_synonyms_, [&key](const auto& kv) { return kv.second.layer == key; });
  return cvars_.drop_layer(key) + static_cast<int>(names - command_synonyms_.size());
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
    } else {
      cur = a;  // a command with no `+` in front: its line runs to the next `+`
    }
  }
  flush();
  return n;
}

// ---- output and problems ----

void Console::set_sink(std::function<void(std::string_view)> sink) { sink_ = std::move(sink); }

void Console::print(std::string_view line) {
  if (tap_) tap_(line);
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

void Console::fire_triggers(std::string_view line) { fire_triggers(line, -1, false); }

void Console::fire_triggers(std::string_view line, int level, bool team) {
  if (triggers_quiet()) return;  // a ruleset's match: no line runs a player's script
  // The newest trigger first, as each queues its alias at the front.
  for (const Trigger& t : triggers_) {
    if (t.match.empty() || line.find(t.match) == std::string_view::npos) continue;
    if (level >= 0 && t.level != level &&
        !(t.level == kTriggerTeam && level == kTriggerChat && team))
      continue;
    if (aliases_.count(t.alias) == 0 && level >= 0) {
      print("trigger \"" + t.alias + "\" has no matching alias");
      continue;
    }
    trigger_match_ = std::string(line.substr(0, 255));
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

void Console::set_arg_values(std::string_view word, ArgValues values) {
  if (values)
    arg_values_[lower(word)] = std::move(values);
  else
    arg_values_.erase(lower(word));
}

std::vector<std::string> Console::arg_values(std::string_view word) const {
  const auto it = arg_values_.find(lower(word));
  return it != arg_values_.end() ? it->second() : std::vector<std::string>();
}

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
