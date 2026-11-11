// THE `brush` TOOL, run through `ge tool brush -- <command> ...`: a map's contents from the command
// line, for an agent that wants to know what a map holds before it draws or plays it. Every command
// prints one JSON object on the output and exits 0 when it answered, 1 when a map was refused or
// missing (the object's "error" says why and what to do), 2 on a usage error.
//
//   info <map>      the family, the counts (vertices, surfaces, textures, lightmaps, leaves,
//                   clusters, models, clip nodes, entities), the texture sequences and the BSPX
//                   lumps
//   ents <map> [--class=<glob>] [--near=x,y,z [--radius=<units>]]   every entity block (or those
//                   whose class matches the glob, `light_torch*`) as key/value; with --near only
//                   those within the radius (256 by default) of the point, nearest first, each with
//                   its distance, a brush entity measured to its box and given the box
//   liquids <map> [--deepest]   the map's pools of water, slime and lava, deepest first: each one's
//                   kind, box, depth, narrowest side and leaves (survey.hpp); --deepest only the
//                   deepest at least 32 units across both ways (an eye fits), else the deepest
//   slice <map> --x=|--y=|--z=<units> [--step=<units>]   the leaf contents on that plane as rows of
//                   characters (`.` empty, `#` solid, `~` water, `%` slime, `!` lava, `^` sky)
//   check          every `.bsp` on the search path read, its counts, and every refusal named
//   help           the commands and flags
//
// A <map> is a file on disk, or with `--root=<dir> --game=<name>[,<mod>...]` a name on that search
// path (vfs/search.hpp: `maps/start.bsp` in any case). `check` needs the search path. Nothing read
// is written anywhere: the answer is counts and names.
//
// ge:summary The brush tool: a map's counts, entities near a point, liquids, a slice, maps checked
// ge:keywords brush bsp tool executable info ents entities check json search-path ge-tool
// ge:keywords near radius liquids deepest slice cross-section class-glob
// ge:keywords texture-sequences bspx counts classname
// ge:skill brush agent-interface
// ge:test tests/brush_tool_test.cpp
#pragma once

#include <ostream>

namespace ge::brush {

// The `brush` executable's main: argv[1] the command, then its words and flags.
int bsp_main(int argc, const char* const* argv, std::ostream& out);

}  // namespace ge::brush
