// Tests: the `qc` tool behind `ge qc` on the sample program written to a file: disasm lists every
// function with named operands and lines, run returns fib's value, trace shows each call with its
// arguments and return, profile counts statements and calls, a fault is an error finding with the
// stack trace, and every usage error is refused with exit 2. The protocol lines `ge qc` reads are
// checked here too.
//
// ge:summary Tests: the qc tool's disasm, run, trace and profile and its answer lines
// ge:keywords test progs qc tool disasm run trace profile fault finding protocol usage
// ge:skill progs testing
// ge:test tests/progs_tool_test.cpp
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "progs/sample.hpp"
#include "progs/tool.hpp"

using namespace ge::progs;

namespace {

std::string sample_file() {
  const auto path = std::filesystem::temp_directory_path() / "progs_tool_test_sample.dat";
  std::vector<int32_t> lines;
  const std::vector<uint8_t> bytes = build_sample(lines);
  std::ofstream(path, std::ios::binary)
      .write(reinterpret_cast<const char*>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  std::ofstream(path.string() + ".lines") << lines_text(lines);
  return path.string();
}

struct Ran {
  int code;
  std::string out;
};

Ran qc(std::vector<std::string> args) {
  args.insert(args.begin(), "qc");
  std::vector<const char*> argv;
  argv.reserve(args.size());
  for (const std::string& a : args) argv.push_back(a.c_str());
  std::ostringstream out;
  const int code = qc_main(static_cast<int>(argv.size()), argv.data(), out);
  return {code, out.str()};
}

bool has(const Ran& r, const std::string& s) { return r.out.find(s) != std::string::npos; }

}  // namespace

TEST(disasm_lists_every_function_with_named_operands) {
  const std::string file = sample_file();
  const Ran r = qc({"disasm", file});
  CHECK_MSG(r.code == 0, r.out);
  CHECK_MSG(has(r, "progs version 6, crc 54730"), r.out);
  CHECK(has(r, "function 7 fib (sample.qc): 1 parameters (1)"));
  CHECK(has(r, "builtin 1, 1 parameters"));
  CHECK(has(r, "STATE"));
  CHECK(has(r, "b:self"));
  CHECK(has(r, "; line 81"));
  CHECK(has(r, "@qc summary progs version 6"));
  CHECK(has(r, "@qc result {\"version\":6,\"crc\":54730"));
  const Ran one = qc({"disasm", file, "--function=think"});
  CHECK(one.code == 0 && has(one, "think") && !has(one, "function 7 fib"));
}

TEST(run_returns_the_value_and_counts_the_statements) {
  const std::string file = sample_file();
  const Ran r = qc({"run", file, "--function=fib", "--args=10"});
  CHECK_MSG(r.code == 0, r.out);
  CHECK_MSG(has(r, "fib returned 55 (word 1113325568"), r.out);
  CHECK(has(r, "\"return\":[55,"));
  const Ran all = qc({"run", file, "--function=run", "--args=4", "--builtins=sample"});
  CHECK_MSG(all.code == 0, all.out);
  CHECK(has(all, "printed: 4"));
  const Ran no_builtins = qc({"run", file, "--function=run", "--args=4"});
  CHECK(no_builtins.code == 1);
  CHECK_MSG(has(no_builtins, "builtin 2 is not provided by this game"), no_builtins.out);
}

TEST(a_fault_is_an_error_finding_with_the_stack) {
  const std::string file = sample_file();
  const Ran r = qc({"run", file, "--function=fault"});
  CHECK(r.code == 1);
  CHECK_MSG(has(r, "@qc finding error\tQC fault: NULL function\t"), r.out);
  CHECK(has(r, "fault (sample.qc) statement"));
  CHECK(has(r, "line 81"));
  CHECK(has(r, "ge qc trace"));
}

TEST(trace_shows_each_call_with_its_arguments_and_return) {
  const std::string file = sample_file();
  const Ran r = qc({"trace", file, "--function=fib", "--args=3"});
  CHECK_MSG(r.code == 0, r.out);
  CHECK_MSG(has(r, "  -> fib(3)\n"), r.out);
  CHECK(has(r, "    -> fib(2)\n"));
  CHECK(has(r, "  <- fib = 2 ("));
  const Ran st = qc({"trace", file, "--function=fib", "--args=1", "--statements"});
  CHECK(has(st, "LT"));
  const Ran b = qc({"trace", file, "--function=strings", "--args=2", "--builtins=sample"});
  CHECK_MSG(has(b, "builtin 1 ftos(2) = "), b.out);
}

TEST(profile_counts_statements_and_calls) {
  const std::string file = sample_file();
  const Ran r = qc({"profile", file, "--function=fib", "--args=6", "--repeat=2"});
  CHECK_MSG(r.code == 0, r.out);
  CHECK(has(r, "\"calls\":50"));  // fib(6) enters fib 25 times, twice
  CHECK(has(r, "M statements/s, measured"));
  const Ran all = qc({"profile", file, "--function=run", "--args=5", "--builtins=sample"});
  CHECK(has(all, "builtin 4 spawn: 3 calls"));
}

TEST(usage_errors_exit_two) {
  CHECK(qc({}).code == 2);
  CHECK(qc({"explode", "x.dat"}).code == 2);
  CHECK(qc({"run", "x.dat"}).code == 2);
  CHECK(qc({"disasm", "x.dat", "--bogus"}).code == 2);
  CHECK(qc({"run", "x.dat", "--function=f", "--builtins=all"}).code == 2);
  const Ran missing = qc({"disasm", "/nonexistent/progs.dat"});
  CHECK(missing.code == 1 && has(missing, "cannot read"));
  const Ran nofn = qc({"run", sample_file(), "--function=nothing"});
  CHECK(nofn.code == 1 && has(nofn, "no function named 'nothing'"));
}
