// A PROGRAM OF RULES FROM ITS TABLE (sim/rules_api.hpp): the `se_rules_v1` a rules package
// returns, or the game's own built in, laid out as a native program (progs/native.hpp) the
// classic server runs in place of a compiled one. Each function runs through one dispatch that
// hands it the call: the services over the VM (every word checked, a fault as the bytecode's),
// where its declaration landed and the form's offsets.
//
// THE PACKAGES: `load_rules_package` opens a package directory (its mod.manifest and library,
// engine/mod/loader.hpp) whose game is SE_RULES_GAME, and keeps it open for as long as the
// process runs, since a progs built from it points at its table. The game's own rules (the arena
// and the campaign, baseline_mod.hpp) are the same tables built into the game, so a build that
// loads no libraries (the web's) still has them; their packages play the same bits.
//
// ge:summary A program of rules from its table: a rules package's, or the game's own, as a progs
// ge:keywords rules-package rules_progs load_rules_package own_rules se_rules_v1 dispatch
// ge:keywords native-program services package arena campaign
// ge:skill mod progs shooter
// ge:test tests/shooters_edge_rules_package_test.cpp
#pragma once

#include <filesystem>
#include <string>

#include "progs/file.hpp"
#include "shooters_edge/sim/rules_api.hpp"

namespace shooters_edge::server {

// `rules` laid out in `out`. False with `error` (a table of another size or API, a form that is
// none, a declaration the layout refuses). `rules` must outlive every VM made over `out`.
bool rules_progs(const se_rules_v1& rules, ge::progs::Progs& out, std::string& error);

// The package in `dir` loaded and its program laid out in `out`. False with `error` naming what
// the loader or the layout refused.
bool load_rules_package(const std::filesystem::path& dir, ge::progs::Progs& out,
                        std::string& error);

}  // namespace shooters_edge::server
