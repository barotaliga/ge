// Tests: the mod server's hooks and their order (games/shooters_edge/server/commands.hpp and
// qc_server.hpp). An assembled mod logs every hook it has with dprint: connecting (SetNewParms,
// ClientConnect, PutClientInServer; a spectator's SpectatorConnect), a frame (each player's
// pre-think and post-think as its packet is read, then StartFrame; a spectator's think),
// leaving, ClientKill; the client command routing (the server's fixed commands never reach the
// mod, GE_ClientCommand before UserCmd, the overridable commands after a mod declines them,
// "Bad user command"), chat through ChatMessage, the info-change hooks, the console's ConsoleCmd
// with argv reading the console's words, and the pause hooks (GE_ShouldPause may refuse,
// GE_PausedTic each paused frame, the server unpausing once the last player leaves).
//
// ge:summary Tests: every hook's order and arguments, client command routing, chat, info, console
// ge:keywords test qc hooks order client-command usercmd ge_clientcommand chatmessage
// ge:keywords userinfo_changed localinfochanged consolecmd ge_shouldpause ge_pausedtic routing
// ge:skill progs shooter testing
#include <string>
#include <vector>

#include "check.hpp"
#include "progs/assembler.hpp"
#include "shooter/client.hpp"
#include "shooters_edge_qc_fixture.hpp"

using qcfix::Fixture;
namespace sv = shooters_edge::server;
namespace pg = ge::progs;
namespace sh = ge::shooter;

namespace {

constexpr int kDprint = 25, kFtos = 26, kArgv = 86;

// The hook mod: every hook logs "<hook> <its arguments>" to the console. UserCmd and
// GE_ClientCommand handle "mine"; ChatMessage handles "secret"; ConsoleCmd handles "modcmd";
// GE_ShouldPause answers the global `allow_pause` (1 at first).
pg::Progs hook_mod(bool with_client_command) {
  pg::Assembler a;
  const int32_t dprint = a.builtin("dprint", kDprint);
  const int32_t ftos = a.builtin("ftos", kFtos, {1});
  const int32_t argv = a.builtin("argv", kArgv, {1});
  const int allow = a.global("allow_pause", pg::kTypeFloat, {pg::as_word(1)});
  const int zero = a.constant_f(0), space = a.constant_s(" ");
  const int s_mine = a.constant_s("mine"), s_secret = a.constant_s("secret");
  const int s_modcmd = a.constant_s("modcmd");
  const int netname = a.field("netname", pg::kTypeString);

  // void() <name>: dprint("<name> ", self.netname)
  auto plain = [&](const char* name, int global) {
    const int32_t fn = a.begin_function(name, {}, 1);
    a.emit(pg::kOpLoadS, pg::kGSelf, netname, a.local(0));
    a.call(dprint, {{a.constant_s(name), 1}, {space, 1}, {a.local(0), 1}});
    a.emit(pg::kOpDone);
    a.end_function();
    if (global >= 0) a.globals[static_cast<size_t>(global)] = static_cast<uint32_t>(fn);
  };
  plain("SetNewParms", pg::kGSetNewParms);
  plain("SetChangeParms", pg::kGSetChangeParms);
  plain("ClientConnect", pg::kGClientConnect);
  plain("PutClientInServer", pg::kGPutClientInServer);
  plain("ClientDisconnect", pg::kGClientDisconnect);
  plain("PlayerPreThink", pg::kGPlayerPreThink);
  plain("PlayerPostThink", pg::kGPlayerPostThink);
  plain("ClientKill", pg::kGClientKill);
  plain("StartFrame", pg::kGStartFrame);
  plain("SpectatorConnect", -1);
  plain("SpectatorThink", -1);
  plain("SpectatorDisconnect", -1);

  // float(string cmd [, string args]) UserCmd / GE_ClientCommand: logs, handles "mine".
  auto command = [&](const char* name, bool two) {
    if (two)
      a.begin_function(name, {1, 1}, 1);
    else
      a.begin_function(name, {1}, 1);
    if (two)
      a.call(dprint,
             {{a.constant_s(name), 1}, {space, 1}, {a.param(0), 1}, {space, 1}, {a.param(1), 1}});
    else
      a.call(dprint, {{a.constant_s(name), 1}, {space, 1}, {a.param(0), 1}});
    a.emit(pg::kOpEqS, a.param(0), s_mine, a.local(0));
    a.emit(pg::kOpReturn, a.local(0));
    a.end_function();
  };
  command("UserCmd", false);
  if (with_client_command) command("GE_ClientCommand", true);

  // float(string text, float team) ChatMessage
  a.begin_function("ChatMessage", {1, 1}, 1);
  a.call(ftos, {{a.param(1), 1}});
  a.emit(pg::kOpStoreS, pg::kGlobalReturn, a.local(0));
  a.call(
      dprint,
      {{a.constant_s("ChatMessage"), 1}, {space, 1}, {a.param(0), 1}, {space, 1}, {a.local(0), 1}});
  a.emit(pg::kOpEqS, a.param(0), s_secret, a.local(0));
  a.emit(pg::kOpReturn, a.local(0));
  a.end_function();

  // void(string key, string old, string new) UserInfo_Changed / localinfoChanged
  for (const char* name : {"UserInfo_Changed", "localinfoChanged"}) {
    a.begin_function(name, {1, 1, 1}, 0);
    a.call(dprint, {{a.constant_s(name), 1},
                    {space, 1},
                    {a.param(0), 1},
                    {space, 1},
                    {a.param(1), 1},
                    {space, 1},
                    {a.param(2), 1}});
    a.emit(pg::kOpDone);
    a.end_function();
  }

  // float() ConsoleCmd: logs argv(0), handles "modcmd".
  a.begin_function("ConsoleCmd", {}, 2);
  a.call(argv, {{zero, 1}});
  a.emit(pg::kOpStoreS, pg::kGlobalReturn, a.local(0));
  a.call(dprint, {{a.constant_s("ConsoleCmd"), 1}, {space, 1}, {a.local(0), 1}});
  a.emit(pg::kOpEqS, a.local(0), s_modcmd, a.local(1));
  a.emit(pg::kOpReturn, a.local(1));
  a.end_function();

  // float(float newstate) GE_ShouldPause: logs, answers allow_pause.
  a.begin_function("GE_ShouldPause", {1}, 1);
  a.call(ftos, {{a.param(0), 1}});
  a.emit(pg::kOpStoreS, pg::kGlobalReturn, a.local(0));
  a.call(dprint, {{a.constant_s("GE_ShouldPause"), 1}, {space, 1}, {a.local(0), 1}});
  a.emit(pg::kOpReturn, allow);
  a.end_function();

  // void(float duration) GE_PausedTic
  a.begin_function("GE_PausedTic", {1}, 1);
  a.call(ftos, {{a.param(0), 1}});
  a.emit(pg::kOpStoreS, pg::kGlobalReturn, a.local(0));
  a.call(dprint, {{a.constant_s("GE_PausedTic"), 1}, {space, 1}, {a.local(0), 1}});
  a.emit(pg::kOpDone);
  a.end_function();

  pg::Progs p;
  std::string error;
  CHECK_MSG(a.build(p, error), error);
  return p;
}

struct Hooks {
  pg::Progs mod;
  Fixture fx;
  explicit Hooks(bool with_client_command = false)
      : mod(hook_mod(with_client_command)), fx("{ \"classname\" \"worldspawn\" }", {}, &mod) {
    CHECK_MSG(fx.loaded, fx.error);
  }
  // The console lines since `from`.
  std::vector<std::string> since(size_t from) const {
    return {fx.s->console.begin() + static_cast<std::ptrdiff_t>(from), fx.s->console.end()};
  }
  size_t mark() const { return fx.s->console.size(); }
  sv::CommandResult cmd(int slot, const std::string& line) {
    return sv::client_command(*fx.s, slot, line);
  }
  std::vector<std::string> prints() const {
    std::vector<std::string> out;
    for (const sv::NetEvent& e : fx.s->out.events)
      if (e.kind == sv::EventKind::print) out.push_back(e.text);
    return out;
  }
};

using Lines = std::vector<std::string>;

}  // namespace

TEST(connecting_runs_the_new_parms_then_connect_then_put_in_server) {
  Hooks h;
  const size_t m = h.mark();
  h.fx.join(0, "ann");
  h.fx.join(1, "bob", true);
  CHECK(h.since(m) == (Lines{"SetNewParms ann", "ClientConnect ann", "PutClientInServer ann",
                             "SetNewParms bob", "SpectatorConnect bob"}));
  // The server's own hook log says the same, by hook.
  const auto& log = h.fx.s->hook_log;
  CHECK(log.size() >= 5);
  CHECK(log[log.size() - 5].hook == sv::HookId::set_new_parms && log[log.size() - 5].self == 1);
  CHECK(log.back().hook == sv::HookId::spectator_connect && log.back().self == 2);
}

TEST(a_frame_runs_each_packets_thinks_then_start_frame) {
  Hooks h;
  h.fx.join(0, "ann");
  h.fx.join(1, "bob", true);
  sh::Packet p[2];
  p[0].client = 0;
  p[0].sequence = 1;
  p[1].client = 1;
  p[1].sequence = 1;
  for (sh::Packet& k : p)
    for (sh::MoveCmd& c : k.cmds) c.msec = 13;
  const size_t m = h.mark();
  sv::qc_frame(*h.fx.s, p, 2);
  CHECK(h.since(m) ==
        (Lines{"PlayerPreThink ann", "PlayerPostThink ann", "SpectatorThink bob", "StartFrame "}));
}

TEST(leaving_and_kill) {
  Hooks h;
  h.fx.join(0, "ann");
  h.fx.join(1, "bob", true);
  size_t m = h.mark();
  CHECK(h.cmd(0, "kill") == sv::CommandResult::server);
  CHECK(h.since(m) == (Lines{"UserCmd kill", "ClientKill ann"}));
  m = h.mark();
  CHECK(h.cmd(1, "kill") == sv::CommandResult::server);  // a spectator's kill does nothing
  CHECK(h.since(m) == (Lines{"UserCmd kill"}));
  m = h.mark();
  sv::disconnect(*h.fx.s, 0);
  sv::disconnect(*h.fx.s, 1);
  CHECK(h.since(m) == (Lines{"ClientDisconnect ann", "SpectatorDisconnect bob"}));
  m = h.mark();
  h.fx.join(0, "ann");
  sv::save_change_parms(*h.fx.s);
  CHECK(h.since(m).back() == "SetChangeParms ann");
}

TEST(the_servers_fixed_commands_never_reach_the_mod) {
  Hooks h;
  h.fx.join(0, "ann");
  const size_t m = h.mark();
  for (const char* c : {"new", "spawn", "begin", "pings", "download x", "ptrack 2"})
    CHECK(h.cmd(0, c) == sv::CommandResult::server);
  CHECK(h.since(m).empty());
  CHECK(h.cmd(0, "setinfo team red") == sv::CommandResult::server);
  CHECK(h.since(m) == (Lines{"UserInfo_Changed team  red"}));
  CHECK(sv::info_value(h.fx.s->clients[0].userinfo, "team") == "red");
}

TEST(usercmd_takes_what_the_mod_handles_and_declines_the_rest) {
  Hooks h;
  h.fx.join(0, "ann");
  size_t m = h.mark();
  CHECK(h.cmd(0, "mine please") == sv::CommandResult::mod);
  CHECK(h.since(m) == (Lines{"UserCmd mine"}));
  m = h.mark();
  CHECK(h.cmd(0, "nothing") == sv::CommandResult::bad);
  CHECK(h.since(m) == (Lines{"UserCmd nothing"}));
  CHECK(h.prints().back() == "Bad user command: nothing\n");
  CHECK(h.cmd(0, "") == sv::CommandResult::bad);
  CHECK(h.cmd(5, "mine") == sv::CommandResult::bad);  // no client there
}

TEST(ge_clientcommand_comes_first_with_the_arguments_line) {
  Hooks h(true);
  h.fx.join(0, "ann");
  size_t m = h.mark();
  CHECK(h.cmd(0, "mine a \"b c\"") == sv::CommandResult::mod);
  CHECK(h.since(m) == (Lines{"GE_ClientCommand mine a \"b c\""}));  // UserCmd is not called
  m = h.mark();
  CHECK(h.cmd(0, "rate 2500") == sv::CommandResult::server);
  CHECK(h.since(m) == (Lines{"GE_ClientCommand rate 2500", "UserInfo_Changed rate  2500"}));
}

TEST(chat_goes_through_chatmessage) {
  Hooks h;
  h.fx.join(0, "ann");
  size_t m = h.mark();
  CHECK(h.cmd(0, "say \"hello all\"") == sv::CommandResult::server);
  CHECK(h.since(m) == (Lines{"UserCmd say", "ChatMessage hello all 0"}));
  CHECK(h.prints().back() == "ann: hello all\n");
  m = h.mark();
  const size_t printed = h.prints().size();
  h.cmd(0, "say_team secret");
  CHECK(h.since(m) == (Lines{"UserCmd say_team", "ChatMessage secret 1"}));
  CHECK(h.prints().size() == printed);  // the mod handled it: nothing printed
}

TEST(info_changes_call_their_hooks_only_when_the_value_moves) {
  Hooks h;
  h.fx.join(0, "ann");
  size_t m = h.mark();
  sv::set_user_info(*h.fx.s, 0, "name", "anna");
  sv::set_user_info(*h.fx.s, 0, "name", "anna");
  CHECK(h.since(m) == (Lines{"UserInfo_Changed name ann anna"}));
  m = h.mark();
  sv::server_command(*h.fx.s, "localinfo motd hi");
  sv::server_command(*h.fx.s, "localinfo motd hi");
  sv::set_local_info(*h.fx.s, "motd", "bye");
  CHECK(h.since(m) == (Lines{"localinfoChanged motd  hi", "localinfoChanged motd hi bye"}));
}

TEST(the_console_offers_unknown_lines_to_consolecmd) {
  Hooks h;
  size_t m = h.mark();
  CHECK(sv::server_command(*h.fx.s, "modcmd one two"));
  CHECK(h.since(m) == (Lines{"ConsoleCmd modcmd"}));
  m = h.mark();
  CHECK(!sv::server_command(*h.fx.s, "nothing at all"));
  CHECK(h.since(m) == (Lines{"ConsoleCmd nothing", "Unknown command \"nothing\""}));
  m = h.mark();
  CHECK(sv::server_command(*h.fx.s, "deathmatch 3"));  // a cvar: no hook
  CHECK(h.fx.s->cvars.find("deathmatch")->value == "3");
  CHECK(sv::server_command(*h.fx.s, "deathmatch"));
  CHECK(h.since(m) == (Lines{"deathmatch is \"3\""}));
  CHECK(sv::server_command(*h.fx.s, "set newvar 5"));
  CHECK(h.fx.s->cvars.find("newvar") != nullptr);
}

TEST(pausing_asks_the_mod_and_ticks_while_paused) {
  Hooks h;
  h.fx.join(0, "ann");
  h.fx.join(1, "bob", true);
  CHECK(!sv::request_pause(*h.fx.s, 1, true));  // a spectator cannot
  size_t m = h.mark();
  CHECK(h.cmd(0, "pause") == sv::CommandResult::server);
  CHECK(h.since(m) == (Lines{"UserCmd pause", "GE_ShouldPause 1"}));
  CHECK(h.fx.s->paused);
  m = h.mark();
  sv::qc_frame(*h.fx.s, nullptr, 0);
  sv::qc_frame(*h.fx.s, nullptr, 0);
  CHECK(h.since(m).size() == 2);
  CHECK(h.since(m)[0].rfind("GE_PausedTic", 0) == 0);
  // The mod refuses the unpause.
  pg::set_global_float(h.fx.vm(), pg::find_global(h.mod, "allow_pause")->offset, 0);
  CHECK(!sv::request_pause(*h.fx.s, 0, false));
  CHECK(h.fx.s->paused);
  pg::set_global_float(h.fx.vm(), pg::find_global(h.mod, "allow_pause")->offset, 1);
  // The last player leaving: the server asks with self the world and unpauses.
  m = h.mark();
  sv::disconnect(*h.fx.s, 0);
  CHECK(!h.fx.s->paused);
  CHECK(h.since(m) == (Lines{"ClientDisconnect ann", "GE_ShouldPause 0"}));
}

TEST(cheats_are_refused_unless_the_server_allows_them) {
  Hooks h;
  h.fx.join(0, "ann");
  CHECK(h.cmd(0, "noclip") == sv::CommandResult::server);
  CHECK(h.prints().back() == "Cheats are not enabled on this server\n");
  CHECK(h.fx.s->sv.edicts[1].movetype != sh::kMovetypeNoclip);
}
