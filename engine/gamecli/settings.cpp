// The game CLI's settings (commands.hpp): the player's config.cfg read and written as the game
// reads and writes it, every setting described by the game's catalog (console/catalog.hpp), and
// the dedicated server's settings worked out in layers by the server's own code.
//
// THE PLAYER'S CONFIG is read on a console of the CLI's own, with the game's settings registered
// on it (those the CLI links itself, `Spec::add_cvars`, then the catalog the game wrote), so a
// value is clamped, refused by tier and saved exactly as the game would (the same `save_text`).
// Only config.cfg is read and written back: autoexec.cfg is the player's own file, never written,
// and a setting it also sets is named, since it wins at the game's start.
//
// ge:summary The game CLI's settings: config.cfg through the catalog, server.cfg by its layers
// ge:keywords gamecli settings settings-list settings-get settings-set settings-reset config-cfg
// ge:keywords autoexec catalog server-settings server-set server-cfg explain tier
// ge:skill agent-interface console
// ge:test tests/gamecli_settings_test.cpp
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>

#include "console/catalog.hpp"
#include "console/cfg.hpp"
#include "console/cfg_tidy.hpp"
#include "console/console.hpp"
#include "core/json_write.hpp"
#include "core/num_text.hpp"
#include "gamecli/commands.hpp"
#include "server/config.hpp"
#include "server/main.hpp"

namespace ge::gamecli {

namespace fs = std::filesystem;
namespace con = ge::console;
using core::json_arr;
using core::json_bool;
using core::json_obj;
using core::json_str;

namespace {

bool read_text(const fs::path& p, std::string& out) {
  std::ifstream f(p, std::ios::binary);
  if (!f) return false;
  std::ostringstream s;
  s << f.rdbuf();
  out = s.str();
  return true;
}

// The player's settings as the game would read them: the catalog's cvars, then config.cfg.
struct Player {
  std::unique_ptr<con::Console> console = std::make_unique<con::Console>(false);
  std::vector<con::CatalogEntry> catalog;
  bool have_catalog = false;
  fs::path dir;
  bool have_config = false;
};

bool load_player(Ctx& ctx, Player& p, bool run_config = true) {
  if (!need_places(ctx)) return false;
  p.dir = ctx.places.settings;
  p.console->set_sink([](std::string_view) {});
  if (ctx.spec.add_cvars != nullptr) ctx.spec.add_cvars(*p.console);
  std::string text, error;
  if (read_text(p.dir / con::kCatalogFile, text)) {
    if (!con::read_catalog(text, p.catalog, error)) {
      refuse(ctx, 1, (p.dir / con::kCatalogFile).string() + ": " + error,
             "start " + std::string(ctx.spec.title) + " once and quit it");
      return false;
    }
    con::add_catalog(p.console->cvars(), p.catalog);
    p.have_catalog = true;
  }
  p.console->set_search_path({p.dir});
  p.have_config = fs::exists(p.dir / con::kConfigFile);
  if (p.have_config && run_config) p.console->exec_file(con::kConfigFile);
  p.console->run();
  return true;
}

const char* tier_of(uint32_t flags) {
  if ((flags & con::kReadOnly) != 0) return "read-only";
  if ((flags & con::kServer) != 0) return "server";
  if ((flags & con::kCreated) != 0) return "unknown";
  return "player";
}

std::string bound_json(double v) {
  constexpr double kInf = std::numeric_limits<double>::infinity();
  return v == kInf || v == -kInf ? "null" : core::num::to_string(v);
}

std::string row_json(const con::Cvar& v) {
  const bool ranged = v.kind == con::CvarKind::number || v.kind == con::CvarKind::integer;
  return json_obj({{"name", json_str(v.name)},
                   {"value", json_str(v.value)},
                   {"default", json_str(v.fallback)},
                   {"changed", json_bool(!v.is_default())},
                   {"kind", json_str(con::kind_name(v.kind))},
                   {"min", ranged ? bound_json(v.min) : "null"},
                   {"max", ranged ? bound_json(v.max) : "null"},
                   {"tier", json_str(tier_of(v.flags))},
                   {"flags", json_str(con::flags_text(v.flags))},
                   {"help", json_str(v.help)}});
}

std::string range_words(const con::Cvar& v) {
  if (v.kind == con::CvarKind::boolean) return "0 or 1";
  if (v.kind == con::CvarKind::text) return "text";
  constexpr double kInf = std::numeric_limits<double>::infinity();
  const std::string lo = v.min == -kInf ? "any" : core::num::to_string(v.min);
  const std::string hi = v.max == kInf ? "any" : core::num::to_string(v.max);
  return lo + ".." + hi;
}

std::string row_text(const con::Cvar& v) {
  return v.name + " = \"" + v.value + "\"" +
         (v.is_default() ? "" : "  (default \"" + v.fallback + "\")") +
         (v.help.empty() ? "" : "  " + v.help) + "\n";
}

// Whether autoexec.cfg sets `name` (it wins over config.cfg at the game's start).
bool autoexec_sets(const fs::path& dir, const std::string& name) {
  std::string text;
  if (!read_text(dir / con::kAutoexecFile, text)) return false;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream words(line);
    std::string a, b;
    words >> a >> b;
    if (a == name || ((a == "set" || a == "seta") && b == name)) return true;
  }
  return false;
}

std::string no_catalog_note(const Ctx& ctx) {
  return "no catalog in this settings folder, so names and ranges are not checked; start " +
         std::string(ctx.spec.title) + " once and quit it to write one";
}

// The cvar `name` for a change, or a refusal.
con::Cvar* settable(Ctx& ctx, Player& p, const std::string& name, int& rc) {
  con::Cvar* v = p.console->cvars().find(name);
  if (v == nullptr && p.have_catalog) {
    const std::string near = p.console->suggest(name);
    rc = refuse(ctx, 1,
                "no setting is called '" + name + "'" +
                    (near.empty() ? "" : "; the nearest is '" + near + "'"),
                std::string(ctx.spec.cli) + " settings list " + name.substr(0, 3) + " --json");
    return nullptr;
  }
  if (v != nullptr && (v->flags & con::kServer) != 0) {
    rc = refuse(ctx, 1, name + " is a server's setting, not the player's",
                std::string(ctx.spec.cli) + " server set " + name + " <value>");
    return nullptr;
  }
  if (v != nullptr && (v->flags & con::kReadOnly) != 0) {
    rc = refuse(ctx, 1, name + " is read-only: the game sets it",
                "leave it; " + std::string(ctx.spec.cli) + " settings get " + name + " says why");
    return nullptr;
  }
  return v;
}

int save_player(Ctx& ctx, Player& p, const std::string& name, const std::string& before) {
  p.console->cvars().apply_latched();
  std::error_code ec;
  fs::create_directories(p.dir, ec);
  if (!p.console->write_file(con::kConfigFile, p.console->save_text()))
    return refuse(ctx, 1, (p.dir / con::kConfigFile).string() + " cannot be written",
                  "check the folder's permissions, or pass --settings=<dir>");
  const con::Cvar* v = p.console->cvars().find(name);
  const std::string after = v ? v->value : "";
  std::string next = autoexec_sets(p.dir, name)
                         ? "autoexec.cfg sets " + name + " too and wins at the start: edit it there"
                         : "";
  std::string note = p.have_catalog ? "" : no_catalog_note(ctx);
  if (!p.have_catalog && v != nullptr && (v->flags & con::kCreated) != 0) {
    const std::string near = p.console->suggest(name);
    if (!near.empty() && near != name)
      note = "'" + name + "' is no setting the CLI knows (the nearest is '" + near + "'); " + note;
  }
  const std::string file = (p.dir / con::kConfigFile).string();
  return answer(ctx,
                json_obj({{"name", json_str(name)},
                          {"before", json_str(before)},
                          {"value", json_str(after)},
                          {"file", json_str(file)},
                          {"checked", json_bool(p.have_catalog)},
                          {"note", json_str(note)}}),
                name + ": \"" + before + "\" -> \"" + after + "\" in " + file +
                    (note.empty() ? "" : "\n" + note),
                "the game reads it at its next start (while it runs, type `" + name + " " + after +
                    "` in its console)",
                next);
}

}  // namespace

int cmd_settings_list(Ctx& ctx) {
  Player p;
  if (!load_player(ctx, p)) return 1;
  const std::string prefix = ctx.words.empty() ? "" : ctx.words[0];
  const std::string tier = ctx.flag("tier", "player");
  if (tier != "player" && tier != "server" && tier != "all")
    return refuse(ctx, 2, "--tier=" + tier + " is not player, server or all",
                  std::string(ctx.spec.cli) + " help settings list");
  const bool changed = ctx.has("changed");
  std::vector<std::string> rows;
  std::string text;
  int count = 0;
  for (const con::Cvar* v : p.console->cvars().sorted()) {
    if (!prefix.empty() && v->name.find(prefix) == std::string::npos) continue;
    const std::string t = tier_of(v->flags);
    if (tier == "player" && t != "player" && t != "unknown") continue;
    if (tier == "server" && t != "server") continue;
    if (changed && v->is_default()) continue;
    rows.push_back(row_json(*v));
    text += row_text(*v);
    count++;
  }
  const std::string note = p.have_catalog ? "" : no_catalog_note(ctx);
  return answer(
      ctx,
      json_obj({{"settings", json_arr(rows)},
                {"folder", json_str(p.dir.string())},
                {"config", json_bool(p.have_config)},
                {"checked", json_bool(p.have_catalog)},
                {"note", json_str(note)}}),
      text + (note.empty() ? "" : note + "\n"),
      std::to_string(count) + " settings" + (prefix.empty() ? "" : " matching '" + prefix + "'"),
      count == 0 && !prefix.empty()
          ? std::string(ctx.spec.cli) + " settings list --json lists every one"
          : "");
}

int cmd_settings_get(Ctx& ctx) {
  if (ctx.words.size() != 1)
    return refuse(ctx, 2, "settings get takes one name",
                  std::string(ctx.spec.cli) + " settings get sensitivity");
  Player p;
  if (!load_player(ctx, p)) return 1;
  const con::Cvar* v = p.console->cvars().find(ctx.words[0]);
  if (v == nullptr) {
    const std::string near = p.console->suggest(ctx.words[0]);
    return refuse(ctx, 1,
                  "no setting is called '" + ctx.words[0] + "'" +
                      (near.empty() ? "" : "; the nearest is '" + near + "'") +
                      (p.have_catalog ? "" : " (" + no_catalog_note(ctx) + ")"),
                  std::string(ctx.spec.cli) + " settings list " + ctx.words[0].substr(0, 3));
  }
  const bool autoexec = autoexec_sets(p.dir, v->name);
  return answer(
      ctx, row_json(*v),
      row_text(*v) + "  " + tier_of(v->flags) + ", " + con::kind_name(v->kind) + " " +
          range_words(*v),
      "", autoexec ? "autoexec.cfg sets " + v->name + " too and wins at the game's start" : "");
}

int cmd_settings_set(Ctx& ctx) {
  if (ctx.words.size() != 2)
    return refuse(ctx, 2, "settings set takes a name and a value (quote a value with spaces)",
                  std::string(ctx.spec.cli) + " settings set name \"Big Ben\"");
  Player p;
  if (!load_player(ctx, p)) return 1;
  int rc = 0;
  con::Cvar* v = settable(ctx, p, ctx.words[0], rc);
  if (v == nullptr && rc != 0) return rc;
  const std::string before = v ? v->value : "";
  const con::SetResult r = v ? p.console->cvars().set(*v, ctx.words[1], con::Source::local)
                             : p.console->cvars().set(ctx.words[0], ctx.words[1],
                                                      con::Source::local, true, con::kArchive);
  if (r != con::SetResult::changed && r != con::SetResult::same && r != con::SetResult::latched)
    return refuse(ctx, 1, con::set_result_text(r, ctx.words[0]),
                  std::string(ctx.spec.cli) + " settings get " + ctx.words[0]);
  return save_player(ctx, p, ctx.words[0], before);
}

int cmd_settings_reset(Ctx& ctx) {
  if (ctx.words.size() != 1)
    return refuse(ctx, 2, "settings reset takes one name",
                  std::string(ctx.spec.cli) + " settings reset sensitivity");
  Player p;
  if (!load_player(ctx, p)) return 1;
  int rc = 0;
  con::Cvar* v = settable(ctx, p, ctx.words[0], rc);
  if (v == nullptr)
    return rc != 0 ? rc
                   : refuse(ctx, 1, "'" + ctx.words[0] + "' is not in the config",
                            std::string(ctx.spec.cli) + " settings list --changed");
  const std::string before = v->value;
  p.console->cvars().reset(*v, con::Source::local);
  return save_player(ctx, p, v->name, before);
}

namespace {

// The dedicated server's settings as its process works them out from the install.
struct ServerSide {
  std::unique_ptr<con::Console> console = std::make_unique<con::Console>(true);
  server::ServerConfig config;
  server::ServerCfg cfg;
};

bool load_server(Ctx& ctx, ServerSide& s) {
  if (ctx.spec.server_game == nullptr) {
    refuse(ctx, 1, std::string(ctx.spec.title) + " has no dedicated server",
           std::string(ctx.spec.cli) + " help");
    return false;
  }
  if (!need_places(ctx)) return false;
  s.console->set_sink([](std::string_view) {});
  std::string error;
  if (!server::server_configure(*s.console, {"--root=" + ctx.places.install}, s.config, s.cfg,
                                *ctx.spec.server_game, error)) {
    refuse(ctx, 1, "the server's settings do not read: " + error,
           "fix the line it names, then " + std::string(ctx.spec.cli) + " server settings");
    return false;
  }
  return true;
}

fs::path server_cfg_path(const Ctx& ctx) {
  return fs::path(ctx.places.install) / ctx.spec.server_game->base_dir / con::kServerFile;
}

}  // namespace

int cmd_server_settings(Ctx& ctx) {
  ServerSide s;
  if (!load_server(ctx, s)) return 1;
  const std::string prefix = ctx.words.empty() ? "" : ctx.words[0];
  std::vector<std::string> rows;
  std::string text;
  for (const con::Cvar* v : s.console->cvars().sorted()) {
    if ((v->flags & con::kServer) == 0) continue;
    if (!prefix.empty() && v->name.find(prefix) == std::string::npos) continue;
    const auto h = s.cfg.history.find(v->name);
    const std::string from = h == s.cfg.history.end() || h->second.empty()
                                 ? "default"
                                 : server::where_text(h->second.back());
    rows.push_back(json_obj({{"name", json_str(v->name)},
                             {"value", json_str(v->value)},
                             {"default", json_str(v->fallback)},
                             {"from", json_str(from)},
                             {"help", json_str(v->help)}}));
    text += v->name + " = \"" + v->value + "\"  " + from + "\n";
  }
  const std::string file = server_cfg_path(ctx).string();
  return answer(ctx,
                json_obj({{"settings", json_arr(rows)},
                          {"server_cfg", json_str(file)},
                          {"read", core::json_strs(s.cfg.read)}}),
                text, std::to_string(rows.size()) + " server settings; the base file is " + file,
                std::string(ctx.spec.cli) + " server set <name> <value> changes one");
}

int cmd_server_set(Ctx& ctx) {
  if (ctx.words.size() != 2)
    return refuse(ctx, 2, "server set takes a name and a value",
                  std::string(ctx.spec.cli) + " server set sv_hostname \"Friday frags\"");
  ServerSide s;
  if (!load_server(ctx, s)) return 1;
  const std::string& name = ctx.words[0];
  const std::string& value = ctx.words[1];
  con::Cvar* v = s.console->cvars().find(name);
  if (v == nullptr || (v->flags & con::kServer) == 0) {
    const std::string near = s.console->suggest(name);
    return refuse(ctx, 1,
                  "'" + name + "' is no server setting" +
                      (near.empty() ? "" : "; the nearest is '" + near + "'"),
                  std::string(ctx.spec.cli) + " server settings");
  }
  if (value.find_first_of("\"\n\r") != std::string::npos)
    return refuse(ctx, 2, "a value may not hold a quote or a line break", "write it without one");
  const std::string before = v->value;
  const con::SetResult r = s.console->cvars().set(*v, value, con::Source::code);
  if (r != con::SetResult::changed && r != con::SetResult::same && r != con::SetResult::latched)
    return refuse(ctx, 1, con::set_result_text(r, name),
                  std::string(ctx.spec.cli) + " server settings");
  // The line that sets it rewritten in place (`name value`, `set name value`), else one added.
  const fs::path file = server_cfg_path(ctx);
  std::string text;
  read_text(file, text);
  const std::string line = name + " \"" + v->value + "\"";
  std::istringstream in(text);
  std::string out, l;
  bool done = false;
  while (std::getline(in, l)) {
    std::istringstream words(l);
    std::string a, b;
    words >> a >> b;
    if (a == name || ((a == "set" || a == "seta") && b == name)) {
      if (!done) out += line + "\n";
      done = true;
      continue;
    }
    out += l + "\n";
  }
  if (!done) out += line + "\n";
  std::error_code ec;
  fs::create_directories(file.parent_path(), ec);
  std::ofstream f(file, std::ios::binary);
  if (!(f << out))
    return refuse(ctx, 1, file.string() + " cannot be written", "check the folder's permissions");
  return answer(ctx,
                json_obj({{"name", json_str(name)},
                          {"before", json_str(before)},
                          {"value", json_str(v->value)},
                          {"file", json_str(file.string())}}),
                name + ": \"" + before + "\" -> \"" + v->value + "\" in " + file.string(),
                "a dedicated server reads it when it starts",
                std::string(ctx.spec.cli) +
                    " host stop <port>, then host --dedicated, restarts one "
                    "this CLI runs");
}

namespace {

// A cfg checked against `console`'s settings: each finding a line, exit 1 when there is one.
int check_file(Ctx& ctx, const con::Console& console, const fs::path& file,
               const std::string& again) {
  std::string text;
  if (!read_text(file, text))
    return refuse(ctx, 1, "no cfg file at " + file.string(), "check the path");
  const std::vector<con::CfgFinding> found = con::check_cfg(console, text);
  std::vector<std::string> rows;
  std::string lines;
  for (const con::CfgFinding& f : found) {
    rows.push_back(json_obj({{"line", std::to_string(f.line)},
                             {"what", json_str(f.what)},
                             {"next", json_str(f.next)}}));
    lines += file.string() + ":" + std::to_string(f.line) + ": " + f.what + " (" + f.next + ")\n";
  }
  const int rc = answer(
      ctx, json_obj({{"file", json_str(file.string())}, {"findings", json_arr(rows)}}), lines,
      found.empty() ? file.string() + " sets only settings this game has, each to one of its values"
                    : std::to_string(found.size()) + " findings in " + file.string(),
      found.empty() ? "" : "fix each line, then " + again);
  return found.empty() ? rc : 1;
}

// A cfg written back tidy (--comments, --pretty; --defaults adds every setting of `flags` the file
// leaves alone), printed or with --write in place.
int format_file(Ctx& ctx, const con::Console& console, const fs::path& file, uint32_t flags) {
  std::string text;
  if (!read_text(file, text))
    return refuse(ctx, 1, "no cfg file at " + file.string(), "check the path");
  if (ctx.has("defaults") && !ctx.has("pretty"))
    return refuse(ctx, 2, "--defaults writes every setting into a pretty cfg's sections",
                  "add --pretty");
  const std::string tidy =
      con::format_cfg(console, text,
                      {.comments = ctx.has("comments"),
                       .pretty = ctx.has("pretty"),
                       .defaults = ctx.has("defaults") ? flags : 0u});
  if (ctx.has("write") && tidy != text) {
    std::ofstream f(file, std::ios::binary);
    if (!(f << tidy))
      return refuse(ctx, 1, file.string() + " cannot be written", "check the folder's permissions");
  }
  return answer(ctx,
                json_obj({{"file", json_str(file.string())},
                          {"changed", json_bool(tidy != text)},
                          {"written", json_bool(ctx.has("write") && tidy != text)},
                          {"text", json_str(tidy)}}),
                ctx.has("write") ? "" : tidy,
                tidy == text ? file.string() + " is already tidy"
                             : (ctx.has("write") ? file.string() + " rewritten tidy"
                                                 : file.string() + " tidied (not written)"),
                tidy != text && !ctx.has("write") ? "--write rewrites the file in place" : "");
}

}  // namespace

int cmd_settings_check(Ctx& ctx) {
  Player p;
  if (!load_player(ctx, p, false)) return 1;
  const fs::path file = ctx.words.empty() ? p.dir / con::kConfigFile : fs::path(ctx.words[0]);
  return check_file(ctx, *p.console, file, std::string(ctx.spec.cli) + " settings check");
}

int cmd_settings_format(Ctx& ctx) {
  Player p;
  if (!load_player(ctx, p, false)) return 1;
  const fs::path file = ctx.words.empty() ? p.dir / con::kConfigFile : fs::path(ctx.words[0]);
  return format_file(ctx, *p.console, file, con::kArchive);
}

int cmd_server_check(Ctx& ctx) {
  ServerSide s;
  if (!load_server(ctx, s)) return 1;
  const fs::path file = ctx.words.empty() ? server_cfg_path(ctx) : fs::path(ctx.words[0]);
  return check_file(ctx, *s.console, file, std::string(ctx.spec.cli) + " server check");
}

int cmd_server_format(Ctx& ctx) {
  ServerSide s;
  if (!load_server(ctx, s)) return 1;
  const fs::path file = ctx.words.empty() ? server_cfg_path(ctx) : fs::path(ctx.words[0]);
  return format_file(ctx, *s.console, file, con::kServer);
}

}  // namespace ge::gamecli
