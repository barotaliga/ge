// The toolkit's groups in every game's player CLI (engine/toolkit, docs/gamecli.md § The
// toolkit): each toolkit row a command of the CLI's, its flags known to the flag check, help and
// the catalog, and run through the row's own body, so `secli pak ls` and `ge pak ls` are one
// implementation. The answer is the toolkit's outcome (findings, written, result, summary, next),
// a superset of the CLI's own answer, spelled with the CLI's name.
//
// ge:summary The toolkit's commands (pak, ...) in every game's player CLI, run through one body
// ge:keywords gamecli toolkit shared-command pak qc modder player-cli toolkit_commands
// ge:keywords toolkit_flags
// ge:keywords toolkit_offers tool
// ge:skill agent-interface
// ge:test tests/gamecli_cli_test.cpp
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "gamecli/commands.hpp"
#include "toolkit/call.hpp"

namespace ge::gamecli {

namespace {

int run_toolkit(Ctx& ctx) {
  const toolkit::Row* row = toolkit::find_row(ctx.command);
  if (row == nullptr)
    return refuse(ctx, 2, "no command is called '" + ctx.command + "'",
                  std::string(ctx.spec.cli) + " help");
  toolkit::Call call;
  call.program = ctx.spec.cli;
  call.words = ctx.words;
  // Only the row's own flags: the CLI's common ones and the game's (`--basedir`) are the frame's.
  for (const auto& [name, value] : ctx.flags)
    for (const toolkit::Flag& f : row->flags)
      if (f.name == name) call.flags.emplace_back(name, value);
  call.tool = ctx.spec.tool;
  const toolkit::Outcome o = toolkit::run_row(*row, call);
  std::ostringstream out;
  const int code = toolkit::emit(std::string(ctx.spec.cli) + " " + ctx.command, o, ctx.json, out);
  ctx.out += out.str();
  return code;
}

}  // namespace

std::span<const Command> toolkit_commands() {
  // The rows' words are literals, so their views end in a NUL and stand as the table's strings.
  static const std::vector<Command> all = [] {
    std::vector<Command> v;
    for (const toolkit::Row& r : toolkit::rows())
      v.push_back({r.name.data(), r.args.data(), r.summary.data(), r.example.data(), "engine",
                   run_toolkit});
    return v;
  }();
  return all;
}

bool toolkit_offers(const Spec& spec, std::string_view command) {
  const toolkit::Row* row = toolkit::find_row(command);
  if (row == nullptr) return false;
  if (row->tool.empty()) return true;
  for (const std::string& t : spec.tools)
    if (t == row->tool) return true;
  return false;
}

std::span<const Flag> toolkit_flags(std::string_view command) {
  static const std::map<std::string, std::vector<Flag>, std::less<>> by_row = [] {
    std::map<std::string, std::vector<Flag>, std::less<>> m;
    for (const toolkit::Row& r : toolkit::rows()) {
      std::vector<Flag>& flags = m[std::string(r.name)];
      for (const toolkit::Flag& f : r.flags)
        flags.push_back({f.name.data(), f.value.data(), f.help.data()});
    }
    return m;
  }();
  const auto it = by_row.find(command);
  return it == by_row.end() ? std::span<const Flag>{} : std::span<const Flag>(it->second);
}

}  // namespace ge::gamecli
