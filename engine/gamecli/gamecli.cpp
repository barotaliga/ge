// The game CLI's frame (gamecli.hpp): the command line read, the table, the answer, help, the
// catalog, the skill and the places.
//
// ge:summary The game CLI's frame: parsing, the engine's table, answers, help, catalog and skill
// ge:keywords gamecli run main parse help_text catalog_json skill_text answer refuse need_places
// ge:keywords prefs_dir common_flags command_flags
// ge:skill agent-interface
// ge:test tests/gamecli_cli_test.cpp
#include "gamecli/gamecli.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include "core/json_write.hpp"
#include "core/output.hpp"
#include "gamecli/commands.hpp"
#include "gamecli/process.hpp"

namespace ge::gamecli {

namespace fs = std::filesystem;
using core::json_arr;
using core::json_bool;
using core::json_obj;
using core::json_str;

namespace {

constexpr Command kEngine[] = {
    {"help", "[<command>]", "the commands, or one command's words and flags (--json: the catalog)",
     "help servers", "engine", nullptr},
    {"skill", "", "print a SKILL.md an agent installs to drive this game through this CLI",
     "skill > ~/.claude/skills/game/SKILL.md", "engine", nullptr},
    {"where", "", "where the game's install, settings, logs and programs are, and which exist",
     "where --json", "engine", cmd_where},
    {"settings list", "[<prefix>]",
     "every setting with its value, default, kind, range and help (--changed: only those moved)",
     "settings list sensitivity --json", "engine", cmd_settings_list},
    {"settings get", "<name>", "one setting: its value, default, range, tier and help",
     "settings get sensitivity", "engine", cmd_settings_get},
    {"settings set", "<name> <value>",
     "a player's setting changed in the config the game reads at its next start",
     "settings set sensitivity 4.5", "engine", cmd_settings_set},
    {"settings reset", "<name>", "a player's setting back to its default",
     "settings reset sensitivity", "engine", cmd_settings_reset},
    {"settings check", "[<file>]",
     "a config's mistakes: settings this game lacks, values none of its own fit, a setting set "
     "twice (the player's config.cfg when no file is named)",
     "settings check autoexec.cfg", "engine", cmd_settings_check},
    {"settings format", "[<file>]",
     "a config written back tidy (--comments: each setting's help above it; --pretty: sections, "
     "a fixed order, values aligned; --write: in place)",
     "settings format --pretty --write", "engine", cmd_settings_format},
    {"server check", "[<file>]",
     "a server cfg's mistakes, as settings check finds them (server.cfg when no file is named)",
     "server check", "engine", cmd_server_check},
    {"server format", "[<file>]",
     "a server cfg written back tidy, as settings format writes it (server.cfg when no file is "
     "named)",
     "server format --pretty", "engine", cmd_server_format},
    {"server settings", "[<prefix>]",
     "the dedicated server's settings: each value, where it was set (server.cfg's layers) and its "
     "default",
     "server settings sv_", "engine", cmd_server_settings},
    {"server set", "<name> <value>", "a dedicated server's setting written to its server.cfg",
     "server set sv_hostname \"Friday frags\"", "engine", cmd_server_set},
    {"servers", "",
     "the servers on the LAN, at the masters and those named: name, map, mode, players, ping",
     "servers --has=dm --sort=players --json", "engine", cmd_servers},
    {"route", "<host[:port]>",
     "the lowest-ping way to a server: straight, or through the forwarders the browser finds",
     "route 203.0.113.7:27500 --wait=4", "engine", cmd_route},
    {"server info", "<host[:port]>", "one server: its info, its rules and every player",
     "server info 192.168.1.20:27500", "engine", cmd_server_info},
    {"players", "<host[:port]>", "the players on one server: name, frags, team, ping",
     "players 192.168.1.20", "engine", cmd_players},
    {"rcon", "<host[:port]> <command...>",
     "a command run on a server's console by its remote console password",
     "rcon 192.168.1.20 --password=secret -- status", "engine", cmd_rcon},
    {"host", "", "host a game: in the window (a listen server) or, --dedicated, a server process",
     "host --dedicated --map=dm4 --name=\"Friday frags\" --maxclients=8", "engine", cmd_host},
    {"host list", "", "the servers this CLI started, and whether each still runs",
     "host list --json", "engine", cmd_host_list},
    {"host stop", "<port>|--all", "a server this CLI started, stopped", "host stop 27500", "engine",
     cmd_host_stop},
    {"join", "<host[:port]>", "start the game on a server (a friend's game)",
     "join 192.168.1.20:27500", "engine", cmd_join},
    {"link open", "<link>",
     "a hub's Join, Spectate, Watch or Play link opened in the game (a demo fetched first)",
     "link open shooters_edge://join/192.168.1.20:27500", "engine", cmd_link_open},
    {"link register", "",
     "this game's links made to open in it: a hub's buttons then start the game",
     "link register --dry-run", "engine", cmd_link_register},
    {"account signup", "<name>",
     "an account made at a master's account service: one name on every server, any game",
     "account signup Ranger --master=hub.example.org --password=<it>", "engine",
     cmd_account_signup},
    {"account signin", "<name>", "signed in: a ticket kept for 24 hours that servers check",
     "account signin Ranger --password=<it>", "engine", cmd_account_signin},
    {"account code", "<name>",
     "a one-time login code for a classic client's console (login <name> <code>)",
     "account code Ranger --password=<it>", "engine", cmd_account_code},
    {"account recover", "<name> <recovery code>",
     "a new password set with the recovery code shown at sign-up",
     "account recover Ranger K7QF-29MD-AB12-CD34 --password=<new>", "engine", cmd_account_recover},
    {"account show", "", "the account service used last, its key, and whose ticket is kept",
     "account show --json", "engine", cmd_account_show},
    {"logs", "[<name>]", "the logs of what this CLI started, or the end of one",
     "logs dedicated-27500 --lines=40", "engine", cmd_logs},
};

constexpr Flag kCommon[] = {
    {"json", "", "answer with one JSON object"},
    {"settings", "<dir>", "the folder of the player's config.cfg (default: the game's own)"},
    {"logs", "<dir>", "where started programs' logs go (default: the game's)"},
    {"state", "<dir>", "where the hosted servers are recorded (default: the game's)"},
    {"bin", "<dir>", "where the game's programs are (default: beside this CLI)"},
};

struct CommandFlags {
  const char* command;
  std::vector<Flag> flags;
};

const std::vector<Flag> kAccountFlags = {
    {"master", "<host[:port]>",
     "the account service (a hub run with --accounts, or a server that is its own master); kept "
     "for the next time"},
    {"key", "<hex>", "the service's key, when known (otherwise learned at the first contact)"},
    {"password", "<text>", "the password (otherwise one line of standard input)"},
    {"wait", "<s>", "the most seconds to wait for the service (default 4)"}};

const std::vector<CommandFlags>& flag_table() {
  static const std::vector<CommandFlags> table = {
      {"settings list",
       {{"changed", "", "only the settings off their default"},
        {"tier", "<player|server|all>", "whose settings (default player)"}}},
      {"settings format",
       {{"comments", "", "each setting's one sentence of help above it"},
        {"pretty", "", "sections in a fixed order, values aligned, and the comments"},
        {"write", "", "rewrite the file in place (default: print it)"}}},
      {"server format",
       {{"comments", "", "each setting's one sentence of help above it"},
        {"pretty", "", "sections in a fixed order, values aligned, and the comments"},
        {"write", "", "rewrite the file in place (default: print it)"}}},
      {"servers",
       {{"lan", "<0|1>", "look on the LAN (default: the player's sb_lan)"},
        {"master", "<host:port,...>", "masters to ask, beside the player's sb_masters"},
        {"server", "<host:port,...>", "servers to ask, beside the player's sb_servers"},
        {"has", "<text>", "only servers whose name, map or mode holds this"},
        {"mode", "<mode>", "only this mode (ffa, team, coop, a mod's word)"},
        {"sort", "<ping|players|name|map|mode>", "the order (default ping)"},
        {"not-empty", "", "only servers with players"},
        {"not-full", "", "only servers with a free slot"},
        {"wait", "<s>", "the most seconds to wait for replies (default 3)"}}},
      {"route",
       {{"lan", "<0|1>", "look on the LAN (default: the player's sb_lan)"},
        {"master", "<host:port,...>", "masters to ask, beside the player's sb_masters"},
        {"server", "<host:port,...>", "servers and forwarders to ask, beside sb_servers"},
        {"wait", "<s>", "the most seconds to wait for each step's replies (default 3)"}}},
      {"server info", {{"wait", "<s>", "the most seconds to wait for the reply (default 2)"}}},
      {"players", {{"wait", "<s>", "the most seconds to wait for the reply (default 2)"}}},
      {"rcon",
       {{"password", "<text>", "the server's remote console password (default: rcon_password)"},
        {"wait", "<s>", "the most seconds to wait for the reply (default 2)"}}},
      {"host",
       {{"dedicated", "", "a server process of its own, kept running after the CLI and the window"},
        {"map", "<name>", "the first map"},
        {"name", "<text>", "the server's name in the browser"},
        {"port", "<n>", "the port (default the game's)"},
        {"maxclients", "<n>", "the most players"},
        {"bots", "<n>", "bots in the game at the start (dedicated)"},
        {"password", "<text>", "the password joining takes (dedicated)"},
        {"dry-run", "", "say what would be started, start nothing"}}},
      {"host stop", {{"all", "", "every server this CLI started"}}},
      {"join", {{"dry-run", "", "say what would be started, start nothing"}}},
      {"link open", {{"dry-run", "", "say what would be fetched and started, do neither"}}},
      {"link register", {{"dry-run", "", "say which files and commands it takes, do nothing"}}},
      {"logs", {{"lines", "<n>", "the last this many lines (default 30)"}}},
      {"account signup", kAccountFlags},
      {"account signin", kAccountFlags},
      {"account code", kAccountFlags},
      {"account recover", kAccountFlags},
  };
  return table;
}

const Command* find_command(const Spec& spec, std::string_view name) {
  for (const Command& c : kEngine)
    if (name == c.name) return &c;
  for (const Command& c : spec.commands)
    if (name == c.name) return &c;
  return nullptr;
}

std::vector<const Command*> all_commands(const Spec& spec) {
  std::vector<const Command*> out;
  for (const Command& c : kEngine) out.push_back(&c);
  for (const Command& c : spec.commands) out.push_back(&c);
  return out;
}

std::string flags_line(const Flag& f) {
  return std::string("--") + f.name + (f.value[0] != '\0' ? std::string("=") + f.value : "");
}

std::string kebab(std::string_view s) {
  std::string out(s);
  for (char& c : out)
    if (c == '_') c = '-';
  return out;
}

int run_help(Ctx& ctx) {
  const std::string which =
      ctx.words.empty() ? "" : ctx.words[0] + (ctx.words.size() > 1 ? " " + ctx.words[1] : "");
  if (!which.empty() && find_command(ctx.spec, which) == nullptr &&
      find_command(ctx.spec, ctx.words[0]) == nullptr)
    return refuse(ctx, 2, "no command is called '" + which + "'",
                  std::string(ctx.spec.cli) + " help");
  if (ctx.json) {
    ctx.out += catalog_json(ctx.spec) + "\n";
    return 0;
  }
  ctx.out += help_text(
      ctx.spec, find_command(ctx.spec, which) ? which : (ctx.words.empty() ? "" : ctx.words[0]));
  return 0;
}

}  // namespace

std::string Ctx::flag(std::string_view name, std::string_view fallback) const {
  const auto it = flags.find(name);
  return it == flags.end() ? std::string(fallback) : it->second;
}

std::span<const Command> engine_commands() { return kEngine; }
std::span<const Flag> common_flags() { return kCommon; }
std::span<const Flag> command_flags(std::string_view command) {
  for (const CommandFlags& c : flag_table())
    if (command == c.command) return c.flags;
  return {};
}

bool try_places(Ctx& ctx) {
  if (ctx.located) return true;
  if (!ctx.locate_error.empty()) return false;
  Places p;
  std::string error;
  if (ctx.spec.locate != nullptr && !ctx.spec.locate(ctx, p, error)) {
    ctx.locate_error = error;
    return false;
  }
  const std::string prefs = prefs_dir(ctx.spec.prefs_org, ctx.spec.prefs_app);
  if (p.state.empty()) p.state = prefs.empty() ? "cli" : (fs::path(prefs) / "cli").string();
  if (p.logs.empty()) p.logs = prefs.empty() ? "logs" : (fs::path(prefs) / "logs").string();
  if (p.settings.empty()) p.settings = prefs.empty() ? "." : prefs;
  if (ctx.has("settings")) p.settings = ctx.flag("settings");
  if (ctx.has("logs")) p.logs = ctx.flag("logs");
  if (ctx.has("state")) p.state = ctx.flag("state");
  ctx.places = p;
  ctx.located = true;
  return true;
}

bool need_places(Ctx& ctx) {
  if (try_places(ctx)) return true;
  refuse(ctx, 1, ctx.locate_error,
         "pass --settings=<dir>, or the game's own flags (" + std::string(ctx.spec.cli) + " help)");
  return false;
}

int answer(Ctx& ctx, const std::string& result, const std::string& text, const std::string& summary,
           const std::string& next) {
  if (ctx.json) {
    ctx.out += json_obj({{"command", json_str(ctx.command)},
                         {"ok", json_bool(true)},
                         {"result", result.empty() ? "{}" : result},
                         {"summary", json_str(summary)},
                         {"next", json_str(next)}}) +
               "\n";
    return 0;
  }
  ctx.out += text;
  if (!text.empty() && text.back() != '\n') ctx.out += '\n';
  if (!summary.empty()) ctx.out += summary + "\n";
  if (!next.empty()) ctx.out += "next: " + next + "\n";
  return 0;
}

int refuse(Ctx& ctx, int exit, const std::string& error, const std::string& next) {
  if (ctx.json) {
    ctx.out += json_obj({{"command", json_str(ctx.command)},
                         {"ok", json_bool(false)},
                         {"exit", std::to_string(exit)},
                         {"error", json_str(error)},
                         {"next", json_str(next)}}) +
               "\n";
  } else {
    ctx.out += std::string(ctx.spec.cli) + ": " + error + "\nnext: " + next + "\n";
  }
  return exit;
}

std::string help_text(const Spec& spec, std::string_view command) {
  std::string out;
  if (!command.empty()) {
    for (const Command* c : all_commands(spec)) {
      const std::string_view n = c->name;
      if (n != command &&
          !(n.starts_with(command) && n.size() > command.size() && n[command.size()] == ' '))
        continue;
      out += std::string(spec.cli) + " " + c->name +
             (c->args[0] ? std::string(" ") + c->args : "") + "\n  " + c->summary + "\n  e.g. " +
             spec.cli + " " + c->example + "\n";
      for (const Flag& f : command_flags(c->name))
        out += "  " + flags_line(f) + "  " + f.help + "\n";
    }
    out += "every command: --json (one JSON object), --settings=<dir>, --bin=<dir>";
    for (const Flag& f : spec.flags) out += ", " + flags_line(f);
    return out + "\n";
  }
  out += std::string(spec.cli) + ": " + spec.title +
         " for agents and players: everything but the playing. Answers are text, or one JSON "
         "object with --json.\n";
  std::string group;
  for (const Command* c : all_commands(spec)) {
    if (group != c->group) {
      group = c->group;
      out += group == "engine" ? std::string("\n") : "\n" + group + ":\n";
    }
    std::string head = std::string("  ") + c->name + (c->args[0] ? std::string(" ") + c->args : "");
    if (head.size() < 34) head.append(34 - head.size(), ' ');
    out += head + " " + c->summary + "\n";
  }
  out += "\nevery command: --json, --settings=<dir>, --logs=<dir>, --state=<dir>, --bin=<dir>";
  for (const Flag& f : spec.flags) out += ", " + flags_line(f);
  out += "\n`" + std::string(spec.cli) + " help <command>` names its flags and an example.\n";
  return out;
}

std::string catalog_json(const Spec& spec) {
  const auto flags_json = [](std::span<const Flag> flags) {
    std::vector<std::string> out;
    for (const Flag& f : flags)
      out.push_back(json_obj(
          {{"name", json_str(f.name)}, {"value", json_str(f.value)}, {"help", json_str(f.help)}}));
    return json_arr(out);
  };
  std::vector<std::string> commands;
  for (const Command* c : all_commands(spec))
    commands.push_back(json_obj({{"name", json_str(c->name)},
                                 {"args", json_str(c->args)},
                                 {"summary", json_str(c->summary)},
                                 {"example", json_str(std::string(spec.cli) + " " + c->example)},
                                 {"group", json_str(c->group)},
                                 {"flags", flags_json(command_flags(c->name))}}));
  std::vector<Flag> every(kCommon, kCommon + std::size(kCommon));
  every.insert(every.end(), spec.flags.begin(), spec.flags.end());
  return json_obj({{"command", json_str("help")},
                   {"ok", json_bool(true)},
                   {"result", json_obj({{"cli", json_str(spec.cli)},
                                        {"game", json_str(spec.game)},
                                        {"title", json_str(spec.title)},
                                        {"commands", json_arr(commands)},
                                        {"flags", flags_json(every)}})},
                   {"summary", json_str(std::to_string(commands.size()) + " commands")},
                   {"next", json_str("")}});
}

std::string skill_text(const Spec& spec) {
  const std::string cli = spec.cli;
  std::string out = "---\nname: " + kebab(spec.game) + "\ndescription: \"Use to do anything with " +
                    spec.title +
                    " but play it, for its player: change a setting, find a server "
                    "and who is on it, host a game or join a friend's, run a command on a server, "
                    "read what went wrong. Drive it with `" +
                    cli +
                    "`; every answer is JSON with "
                    "--json.\"\n---\n\n";
  out += "# " + std::string(spec.title) + " through `" + cli + "`\n\n";
  out += "`" + cli + "` sits beside the game's programs (`" + spec.window + "`" +
         (spec.server[0] ? std::string(", `") + spec.server + "`" : "") +
         "). It does everything a player does outside a match. Run it with `--json` and read the "
         "one object it prints: `ok`, `result`, `summary` and `next` (on a refusal `error` and "
         "`next`, the exact next step; exit 1 refused, 2 a usage error). `" +
         cli + " help <command>` names a command's flags.\n\n";
  out +=
      "## The loop\n\n"
      "1. `" +
      cli +
      " where --json`: the install, the settings folder and which programs are "
      "there. A refusal here says which folder to pass.\n"
      "2. Run the command the person asked for, with `--json`; follow `next` when `ok` is false.\n"
      "3. Tell the person what changed in their words (the setting's help, the server's name), "
      "never the JSON.\n\n";
  out += "## Commands\n\n| Command | What |\n| --- | --- |\n";
  for (const Command* c : all_commands(spec))
    out += "| `" + cli + " " + c->name + (c->args[0] ? std::string(" ") + c->args : "") + "` | " +
           c->summary + " |\n";
  out +=
      "\n## Rules\n\n"
      "- Settings: find the name first (`settings list <word> --json`: each row's `help` says "
      "what it does and its range), then `settings set`. The game reads its config when it "
      "starts and writes it back when it quits, so change settings while the game is closed, "
      "or type the same `<name> <value>` in its console while it runs.\n"
      "- A server's settings (`sv_` names, the rules) are the server's: `server set` writes "
      "them to the dedicated server's server.cfg; `server settings` says where each value came "
      "from.\n"
      "- Hosting: `host --dedicated` keeps the server running after the CLI exits; `host list` "
      "shows it and `host stop <port>` ends it. Tell the person the address friends join: this "
      "machine's LAN address and the port, and that a game over the internet needs the port "
      "forwarded on their router.\n"
      "- Joining: `servers --has=<friend's server name>` finds it, `players <address>` says "
      "who is on it, `join <address>` starts the game there.\n"
      "- Something went wrong: `logs` lists the logs of what the CLI started; `logs <name>` "
      "prints the end of one.\n"
      "- Never start or stop a server, or join one, that the person did not ask for.\n";
  if (!spec.notes.empty()) out += "\n## " + std::string(spec.title) + "\n\n" + spec.notes + "\n";
  return out;
}

std::string prefs_dir(std::string_view org, std::string_view app) {
  fs::path base;
#ifdef _WIN32
  if (const char* a = std::getenv("APPDATA"); a != nullptr && *a) base = a;
#elif defined(__APPLE__)
  if (const char* h = std::getenv("HOME"); h != nullptr && *h)
    base = fs::path(h) / "Library" / "Application Support";
#else
  if (const char* x = std::getenv("XDG_DATA_HOME"); x != nullptr && *x)
    base = x;
  else if (const char* h = std::getenv("HOME"); h != nullptr && *h)
    base = fs::path(h) / ".local" / "share";
#endif
  if (base.empty()) return {};
  if (!org.empty()) base /= std::string(org);
  base /= std::string(app);
  std::string s = base.string();
  s += static_cast<char>(fs::path::preferred_separator);
  return s;
}

int run(const Spec& spec, const std::vector<std::string>& args, std::string& out,
        const std::string& self_dir) {
  Ctx ctx(spec);
  ctx.self_dir = self_dir;
  std::vector<std::string> words;
  bool rest = false;
  for (const std::string& a : args) {
    if (!rest && a == "--") {
      rest = true;
    } else if (!rest && a.size() > 2 && a.starts_with("--")) {
      const size_t eq = a.find('=');
      ctx.flags[a.substr(2, eq == std::string::npos ? std::string::npos : eq - 2)] =
          eq == std::string::npos ? "" : a.substr(eq + 1);
    } else {
      words.push_back(a);
    }
  }
  ctx.json = ctx.has("json");
  if (words.empty()) words.push_back("help");
  const Command* c = nullptr;
  size_t used = 0;
  if (words.size() >= 2) c = find_command(spec, words[0] + " " + words[1]), used = 2;
  if (c == nullptr) c = find_command(spec, words[0]), used = 1;
  if (c == nullptr) {
    ctx.command = words[0];
    const int rc =
        refuse(ctx, 2, "no command is called '" + words[0] + "'", std::string(spec.cli) + " help");
    out = ctx.out;
    return rc;
  }
  ctx.command = c->name;
  ctx.words.assign(words.begin() + static_cast<long>(used), words.end());
  // Every flag must be one this command or every command takes: a typo is said, not ignored.
  for (const auto& [name, value] : ctx.flags) {
    bool known = false;
    for (const Flag& f : kCommon) known = known || name == f.name;
    for (const Flag& f : spec.flags) known = known || name == f.name;
    for (const Flag& f : command_flags(c->name)) known = known || name == f.name;
    if (!known) {
      const int rc = refuse(ctx, 2, "--" + name + " is no flag of '" + c->name + "'",
                            std::string(spec.cli) + " help " + c->name);
      out = ctx.out;
      return rc;
    }
  }
  int rc = 0;
  if (std::string_view(c->name) == "help") {
    rc = run_help(ctx);
  } else if (std::string_view(c->name) == "skill") {
    ctx.out += skill_text(spec);
  } else {
    rc = c->run(ctx);
  }
  out = ctx.out;
  return rc;
}

int main(int argc, char** argv, const Spec& spec) {
  // What the engine's libraries print on the way (a status line, a warning) goes to the errors,
  // so the answer on the output stays one text or one JSON object.
  core::output::set_output_sink([](const core::output::LogLine& line, void*) {
    if (line.level == core::output::Level::status) return;
    std::fprintf(stderr, "%.*s\n", static_cast<int>(line.message.size()), line.message.data());
  });
  std::vector<std::string> args(argv + 1, argv + argc);
  std::string out;
  const int rc = run(spec, args, out, self_dir(argc > 0 ? argv[0] : nullptr));
  std::fwrite(out.data(), 1, out.size(), stdout);
  return rc;
}

}  // namespace ge::gamecli
