// A server's settings in layers, defined: the layers run in order on the console, every set of a
// server cvar kept with its source, and the explanation.
//
// ge:summary The server's settings in layers, defined: load_server_cfg, explain
// ge:keywords load_server_cfg explain server-cfg layers history
// ge:skill net console
// ge:test tests/server_cfg_test.cpp
#include "server/config.hpp"

#include <algorithm>

namespace ge::server {

namespace {

namespace cn = ge::console;

// The console's queue run to the end (a cfg's `wait`s included), as startup does.
void drain(cn::Console& con) {
  for (int f = 0; f < cn::kMaxFrames && con.pending(); ++f) con.run();
}

// The file run as layer `layer` when it is there.
void run_file(cn::Console& con, ServerCfg& cfg, const std::string& name, Layer layer) {
  std::string text;
  if (!con.read_file(name, text)) return;
  cfg.layer = layer;
  cfg.read.push_back(name);
  con.exec_file(name);
  drain(con);
}

bool first_error(const cn::Console& con, std::string& error) {
  for (const cn::Problem& p : con.problems()) {
    if (p.severity != cn::Problem::Severity::error) continue;
    error = (p.where.file.empty() ? std::string("the command line")
                                  : p.where.file + ":" + std::to_string(p.where.line)) +
            ": " + p.what + (p.next.empty() ? "" : "; next: " + p.next);
    return true;
  }
  return false;
}

std::string in_quotes(std::string_view v) {
  std::string out = "\"";
  out.append(v).append("\"");
  return out;
}

}  // namespace

const char* layer_name(Layer layer) {
  switch (layer) {
    case Layer::fallback:
      return "fallback";
    case Layer::base:
      return "base";
    case Layer::mod:
      return "mod";
    case Layer::profile:
      return "profile";
    case Layer::command_line:
      return "command line";
    case Layer::run_time:
      return "run time";
  }
  return "?";
}

std::string where_text(const ValueSource& v) {
  if (!v.file.empty()) return v.line > 0 ? v.file + ":" + std::to_string(v.line) : v.file;
  if (v.layer == Layer::command_line) return "command line";
  if (v.layer == Layer::run_time) return "console";
  return layer_name(v.layer);
}

bool load_server_cfg(cn::Console& con, ServerCfg& cfg, const std::vector<std::string>& line,
                     std::string& error) {
  cfg.history.clear();
  cfg.read.clear();
  con.cvars().set_watch([&con, &cfg](const cn::Cvar& v, std::string_view value, cn::Source source) {
    if ((v.flags & cn::kServer) == 0 || source == cn::Source::server) return;
    ValueSource s;
    s.layer = cfg.layer;
    s.value = std::string(value);
    if (const cn::Args* a = con.current()) {
      s.file = a->where.file;
      s.line = a->where.line;
    }
    cfg.history[v.name].push_back(std::move(s));
  });
  // The mods are the command line's (`--mod`), set by code before the files are read.
  if (const cn::Cvar* mod = con.cvars().find("sv_mod"); mod && !mod->value.empty())
    cfg.history["sv_mod"].push_back({Layer::command_line, "--mod", 0, mod->value});
  con.clear_problems();
  run_file(con, cfg, cfg.base + "/server.cfg", Layer::base);
  if (first_error(con, error)) return false;
  for (const std::string& mod : cfg.mods) {
    run_file(con, cfg, mod + "/server.cfg", Layer::mod);
    if (first_error(con, error)) return false;
  }
  if (!cfg.profile.empty()) {
    // The winning copy: the highest directory that has one.
    std::vector<std::string> dirs(cfg.mods.rbegin(), cfg.mods.rend());
    dirs.push_back(cfg.base);
    const std::string file = cfg.profile + ".cfg";
    std::string found;
    std::string text;
    for (const std::string& d : dirs) {
      std::string path = d;
      path.append("/").append(file);
      if (con.read_file(path, text)) {
        found = std::move(path);
        break;
      }
    }
    if (found.empty()) {
      error = "profile '" + cfg.profile + "': no " + file + " in ";
      for (size_t i = 0; i < dirs.size(); i++) error.append(i ? ", " : "").append(dirs[i]);
      error += "; next: check the name, or write " + cfg.base + "/" + file;
      return false;
    }
    run_file(con, cfg, found, Layer::profile);
    if (first_error(con, error)) return false;
  }
  cfg.layer = Layer::command_line;
  con.append_command_line(line);
  drain(con);
  if (first_error(con, error)) return false;
  cfg.layer = Layer::run_time;
  return true;
}

std::string explain(const cn::Console& con, const ServerCfg& cfg, std::string_view prefix) {
  struct Row {
    std::string name, value, where, over;
  };
  std::vector<Row> rows;
  size_t name_w = 0, value_w = 0, where_w = 0;
  for (const cn::Cvar* v : con.cvars().sorted()) {
    if ((v->flags & cn::kServer) == 0) continue;
    if (v->name.compare(0, prefix.size(), prefix) != 0) continue;
    Row r;
    r.name = v->name;
    r.value = in_quotes(v->has_latched ? v->latched : v->value);
    const auto it = cfg.history.find(v->name);
    if (it == cfg.history.end() || it->second.empty()) {
      r.where = "fallback";
    } else {
      const std::vector<ValueSource>& h = it->second;
      r.where = where_text(h.back());
      // What it overrode: the newest earlier set from another place, else the fallback when the
      // value moved off it.
      const std::string here = r.where;
      auto prev = std::find_if(h.rbegin() + 1, h.rend(),
                               [&here](const ValueSource& s) { return where_text(s) != here; });
      if (prev != h.rend())
        r.over = "(over " + where_text(*prev) + " " + in_quotes(prev->value) + ")";
      else if (v->value != v->fallback)
        r.over = "(over the fallback " + in_quotes(v->fallback) + ")";
    }
    name_w = std::max(name_w, r.name.size());
    value_w = std::max(value_w, r.value.size());
    where_w = std::max(where_w, r.where.size());
    rows.push_back(std::move(r));
  }
  std::string out;
  for (const Row& r : rows) {
    std::string row = r.name + std::string(name_w - r.name.size() + 2, ' ') + r.value +
                      std::string(value_w - r.value.size() + 2, ' ') + r.where;
    if (!r.over.empty()) row += std::string(where_w - r.where.size() + 2, ' ') + r.over;
    out += row + "\n";
  }
  return out;
}

}  // namespace ge::server
