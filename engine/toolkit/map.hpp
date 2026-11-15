// THE `map` GROUP: a classic BSP map's contents read from the command line, for a mapper or a
// modder with either front door, before they draw or play it:
//
//   map info <map>                     the family and the counts, texture sequences, BSPX lumps
//   map ents <map> [--class=<glob>] [--near=x,y,z [--radius=<units>]]   the entity blocks
//   map liquids <map> [--deepest]      the pools of water, slime and lava, deepest first
//   map slice <map> --x=|--y=|--z=<units> [--step=<units>]   the leaf contents on a plane
//   map check --root=<dir> --game=<name>   every map on the search path read, refusals named
//
// A <map> is a file, or with --root and --game a name on that search path (`maps/start.bsp`).
// The bodies are the brush plugin's `brush` tool (plugins/brush/tool.hpp), which the engine does
// not link, so each row names the tool "brush" and runs it through `Call::tool`: `ge` builds the
// game repo's `brush` executable, a game's CLI that links the plugin calls it in its own process
// (plugins/brush/cli). The tool answers one JSON object; `map_answer` turns it into the Outcome
// both front doors print. Building a map (`map build`, `map plan`) is the plugin's own rows in a
// game's CLI (plugins/brush/cli/commands.hpp).
//
// ge:summary The map command group: a BSP map's counts, entities, liquids, a slice, maps checked
// ge:keywords toolkit map bsp brush info ents entities liquids slice check map_answer map_rows
// ge:keywords tool mapper modder search-path
// ge:skill agent-interface brush
// ge:test tests/toolkit_map_test.cpp
#pragma once

#include <span>
#include <string_view>

#include "toolkit/call.hpp"

namespace ge::toolkit {

// The tool's one JSON object as an Outcome for `command` ("info", "ents", ...): the result, a line
// of text a thing found, the summary, a refusal as a finding with its next step.
Outcome map_answer(const Call& call, const ToolRun& run, std::string_view command);

std::span<const Row> map_rows();

}  // namespace ge::toolkit
