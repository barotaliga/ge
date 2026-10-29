// The progs plugin's tool, the executable `progs` that `ge qc` runs: progs/tool.hpp's `qc_main` on the standard output.
//
// ge:summary The progs tool executable behind ge qc (disasm, run, trace, profile)
// ge:keywords qc progs executable main ge-qc tool
// ge:skill progs agent-interface
// ge:test tests/progs_tool_test.cpp
#include <iostream>

#include "progs/tool.hpp"

int main(int argc, char** argv) { return ge::progs::qc_main(argc, argv, std::cout); }
