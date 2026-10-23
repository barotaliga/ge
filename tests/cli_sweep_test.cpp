// `ge lab sweep`'s parts that need no build (engine/cli/sweep.hpp): a constant's one-line
// constexpr definition found by its bare or qualified name, its initializer rewritten with the
// rest of the file untouched, the runs' metrics laid side by side with the current value
// starred, and a --flag sweep's refusals (malformed, or beside --set).
//
// ge:summary Test: ge lab sweep's constant search, its rewrite and the metrics table
// ge:keywords test cli lab sweep constant constexpr find_constants with_value flag-sweep
// ge:skill testing agent-interface
#include <filesystem>
#include <string>

#include "check.hpp"
#include "cli/build.hpp"
#include "cli/sweep.hpp"

using namespace ge::cli;

namespace {

const std::string kSpec =
    "// THE WING\n"
    "#pragma once\n"
    "namespace ge::paramotor::spec {\n"
    "inline constexpr double kArea = 16;\n"
    "namespace launch {\n"
    "// How long a launch takes, s.\n"
    "inline constexpr double kFlight = 0.7;  // s\n"
    "}  // namespace launch\n"
    "constexpr int kLimit = 3;\n"
    "inline constexpr double kAreaSquared = kArea * kArea;\n"
    "}  // namespace ge::paramotor::spec\n";

}  // namespace

TEST(a_bare_name_finds_its_one_line_definition) {
  const auto sites = find_constants(kSpec, "plugins/paramotor/spec.hpp", "kFlight");
  CHECK(sites.size() == 1);
  CHECK(sites[0].line == 7);
  CHECK(sites[0].value == "0.7");
  CHECK(sites[0].file == "plugins/paramotor/spec.hpp");
}

TEST(a_plain_constexpr_and_an_expression_are_found_too) {
  CHECK(find_constants(kSpec, "f", "kLimit").size() == 1);
  const auto sq = find_constants(kSpec, "f", "kAreaSquared");
  CHECK(sq.size() == 1);
  CHECK(sq[0].value == "kArea * kArea");
}

TEST(a_name_is_matched_whole_never_as_a_prefix) {
  const auto sites = find_constants(kSpec, "f", "kArea");
  CHECK(sites.size() == 1);
  CHECK(sites[0].line == 4);
}

TEST(a_qualified_name_keeps_only_files_that_open_its_namespaces) {
  CHECK(find_constants(kSpec, "f", "launch::kFlight").size() == 1);
  CHECK(find_constants(kSpec, "f", "paramotor::spec::launch::kFlight").size() == 1);
  CHECK(find_constants(kSpec, "f", "snowmobile::kFlight").empty());
}

TEST(the_rewrite_changes_the_initializer_alone) {
  const auto site = find_constants(kSpec, "f", "kFlight").at(0);
  const std::string out = with_value(kSpec, site, "0.95");
  CHECK(out.find("inline constexpr double kFlight = 0.95;  // s\n") != std::string::npos);
  CHECK(out.find("0.7") == std::string::npos);
  // Every other line, and the file's last newline, as they were.
  CHECK(out.size() == kSpec.size() + 1);
  CHECK(with_value(out, find_constants(out, "f", "kFlight").at(0), "0.7") == kSpec);
}

TEST(the_table_lays_the_runs_side_by_side) {
  const std::vector<SweepRun> runs{
      {"0.5",
       {"# key\tvalue\tunit\tdet", "launch.top-speed\t41.25\tkm/h\tdeterministic",
        "launch.sink\t1.5\tm/s\tdeterministic"}},
      {"0.7", {"launch.top-speed\t43\tkm/h\tdeterministic"}},
  };
  const std::string all = sweep_table(runs, "0.7", {});
  CHECK(all ==
        "metric            0.5    0.7*  unit\n"
        "launch.top-speed  41.25  43    km/h\n"
        "launch.sink       1.5    -     m/s\n");  // the second run measured no sink
  const std::string some = sweep_table(runs, "0.7", {"speed"});
  CHECK(some.find("launch.sink") == std::string::npos);
  CHECK(some.find("launch.top-speed") != std::string::npos);
}

TEST(a_flag_sweep_is_refused_before_any_build_when_malformed_or_beside_set) {
  const BuildOptions o{std::filesystem::temp_directory_path() / "ge-sweep-test-none", "debug", ""};
  SweepOptions both{"para", "kFlight=0.5", "hz=60,120", "", "", {}};
  const Outcome a = run_lab_sweep(o, both);
  CHECK(a.exit == 2);
  CHECK(!a.findings.empty() && a.findings[0].what.find("--set and --flag") != std::string::npos);
  SweepOptions bare{"para", "", "hz", "", "", {}};  // no values
  const Outcome b = run_lab_sweep(o, bare);
  CHECK(b.exit == 2);
  CHECK(!b.findings.empty() && b.findings[0].what.find("--flag wants") != std::string::npos);
}
