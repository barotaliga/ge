// The `labs` executable's entry point: every `*_lab.cpp` under engine/, plugins/ and games/ is
// compiled into the same binary and registers itself, so this is one call.
//
// ge:summary The labs executable's main: hands the command line to the runner
// ge:keywords lab labs main executable entry
// ge:skill labs
#include "lab/runner.hpp"

int main(int argc, char** argv) { return ge::lab::run_labs(argc, argv); }
