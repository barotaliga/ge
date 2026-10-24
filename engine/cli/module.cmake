# The ge command line (engine/cli, cmake/Cli.cmake): built here too when a game pulls the engine
# in, so the game runs the ge of the engine it pins. A tool, not a library a game links: not
# exported (the ge executable installs to bin/).
include(cmake/Cli.cmake)
ge_add_cli()
ge_module(ge_cli)
