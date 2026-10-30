// THE `brush` TOOL, run through `ge tool brush -- <command> ...`: a map's contents from the command
// line, for an agent that wants to know what a map holds before it draws or plays it. Every command
// prints one JSON object on the output and exits 0 when it answered, 1 when a map was refused or
// missing (the object's "error" says why and what to do), 2 on a usage error.
//
//   info <map>      the family, the counts (vertices, surfaces, textures, lightmaps, leaves,
//                   clusters, models, clip nodes, entities), the texture sequences and the BSPX
//                   lumps
//   ents <map> [--class=<classname>]   every entity block (or those of one class) as key/value
//   check          every `.bsp` on the search path read, its counts, and every refusal named
//   help           the commands and flags
//
// A <map> is a file on disk, or with `--root=<dir> --game=<name>[,<mod>...]` a name on that search
// path (vfs/search.hpp: `maps/start.bsp` in any case). `check` needs the search path. Nothing read
// is written anywhere: the answer is counts and names.
//
// ge:summary The brush tool: a map's counts, entities and texture sequences, every map checked
// ge:keywords brush bsp tool executable info ents entities check json search-path ge-tool
// ge:keywords texture-sequences bspx counts classname
// ge:skill brush agent-interface
// ge:test tests/brush_tool_test.cpp
#pragma once

#include <ostream>

namespace ge::brush {

// The `brush` executable's main: argv[1] the command, then its words and flags.
int bsp_main(int argc, const char* const* argv, std::ostream& out);

}  // namespace ge::brush
