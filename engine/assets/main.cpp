// The `assets` executable's entry point: every `assets/<name>/data.cpp` under engine/, plugins/
// and games/ is compiled into the same binary and registers its data set, so this is one call.
//
// ge:summary The assets executable's main: hands the command line to the assets tool
// ge:keywords assets main executable entry
// ge:skill assets
#include "assets/tool.hpp"

int main(int argc, char** argv) { return ge::assets::run_assets(argc, argv); }
