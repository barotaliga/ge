// The brush plugin's tool, the executable `brush` that `ge tool brush` runs: brush/tool.hpp's
// `bsp_main` on the standard output.
//
// ge:summary The brush tool executable behind ge tool brush (info, ents, check)
// ge:keywords brush bsp executable main ge-tool tool
// ge:skill brush agent-interface
// ge:test tests/brush_tool_test.cpp
#include <iostream>

#include "brush/tool.hpp"

int main(int argc, char** argv) { return ge::brush::bsp_main(argc, argv, std::cout); }
