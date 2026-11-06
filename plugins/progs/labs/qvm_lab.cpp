// THE qvm LAB: what the compiled-module VM (progs/qvm.hpp) runs and what it costs. It runs the
// sample module (progs/qvm_asm.hpp) at several sizes and tables and charts the instructions each
// export takes (deterministic: the digest holds them), shows a fault with its frames, and times the
// interpreter on fib (measured, outside the digest). Given `--file=<module.qvm>` (a match mod's
// qwprogs.qvm a player has) it also reads that module: its size, memory, CRC and functions, the
// opcode mix, the functions' sizes as a histogram, and its init export run against a host that
// answers only the interface version and logs every other system call (all measured: the file is
// the player's, never the tree's).
//
// ge:summary The qvm lab: the sample module's instructions by size, a fault, throughput, a module
// ge:keywords lab qvm compiled-module interpreter throughput opcode-mix syscall init histogram
// ge:keywords sample fib digest bench qwprogs
// ge:skill progs labs
// ge:test tests/progs_qvm_test.cpp
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "progs/qvm.hpp"
#include "progs/qvm_asm.hpp"

namespace {

namespace gl = ge::lab;
namespace pg = ge::progs;

std::string fixed(double v, int d) { return ge::core::num::to_fixed(v, d); }

// The file's name without its folders (a lab's words never carry a player's paths).
std::string base_name(const std::string& path) {
  const size_t slash = path.find_last_of("/\\");
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

void shoot(gl::Lab& lab, const std::string& id, const gl::Chart& chart, const std::string& caption,
           const std::string& look_for, const std::string& flag, gl::Det det) {
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()), det);
  lab.shot({id, chart.title, caption, look_for, {{flag, base_name(lab.arg(flag))}}},
           gl::to_picture(chart.plot), det);
}

// The sample's host: system call 0 answers its argument times ten.
int32_t sample_host(void*, pg::Qvm&, const int32_t* args) {
  return args[0] == 0 ? args[1] * 10 : 0;
}

void sample_section(gl::Lab& lab, pg::Qvm& vm) {
  gl::LabTable table{
      "the sample module's exports by size", {"export", "n", "result", "instructions"}, {}};
  gl::Trace fib{"fib(n)", {}, {}}, sum{"sum(n), one system call a round", {}, {}};
  for (double d : lab.arg_nums("sizes")) {
    const auto n = static_cast<int32_t>(d);
    for (int32_t cmd : {pg::kQvmSampleFib, pg::kQvmSampleSum}) {
      const uint64_t before = vm.instructions;
      const int32_t r = pg::qvm_call(vm, cmd, n);
      if (!vm.fault.empty()) {
        lab.fail("the sample faulted: " + vm.fault, "run `make test T=progs_qvm` (skill: progs)");
        pg::qvm_clear_fault(vm);
        return;
      }
      const auto ran = static_cast<double>(vm.instructions - before);
      gl::Trace& t = cmd == pg::kQvmSampleFib ? fib : sum;
      t.x.push_back(d);
      t.y.push_back(ran);
      table.rows.push_back({cmd == pg::kQvmSampleFib ? "fib" : "sum", std::to_string(n),
                            std::to_string(r), fixed(ran, 0)});
      lab.metric(std::string(cmd == pg::kQvmSampleFib ? "fib." : "sum.") + std::to_string(n), ran,
                 "instructions");
    }
  }
  lab.table(table);
  gl::LineChart spec{{"QVM SAMPLE: INSTRUCTIONS BY SIZE", "the sample module (progs/qvm_asm.hpp)",
                      "n", "instructions a call (log)"},
                     {fib, sum},
                     {},
                     {},
                     {},
                     true,
                     true,
                     false};
  shoot(lab, "sample", gl::line_chart(spec),
        "Instructions one host call runs: fib(n) by recursion (a call and two frames a step) and "
        "sum(n), a loop that makes one system call a round.",
        "fib a straight line on the log scale (it grows by the golden ratio), sum a gentle curve "
        "(linear in n)",
        "sizes", gl::Det::deterministic);
}

void fault_section(gl::Lab& lab, pg::Qvm& vm) {
  pg::qvm_call(vm, pg::kQvmSampleDiv, 1, 0);
  lab.note("a fault, as the host sees it: " + vm.fault);
  lab.metric("fault.pc", vm.fault_pc, "instruction");
  pg::qvm_clear_fault(vm);
}

void throughput_section(gl::Lab& lab, pg::Qvm& vm) {
  const auto n = static_cast<int32_t>(lab.arg_num("fib"));
  const int repeat = std::max(1, static_cast<int>(lab.arg_num("timings")));
  gl::Sample s{"fib(" + std::to_string(n) + ")", {}};
  uint64_t per_call = 0;
  for (int r = 0; r < repeat; ++r) {
    const uint64_t before = vm.instructions;
    const auto t0 = std::chrono::steady_clock::now();
    pg::qvm_call(vm, pg::kQvmSampleFib, n);
    const double secs =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    per_call = vm.instructions - before;
    s.values.push_back(static_cast<double>(per_call) / std::max(secs, 1e-9) / 1e6);
  }
  std::vector<double> sorted = s.values;
  std::sort(sorted.begin(), sorted.end());
  lab.metric("throughput.fib", sorted[sorted.size() / 2], "M/s", gl::Det::measured);
  lab.metric("instructions.fib", static_cast<double>(per_call), "count");
  const gl::Chart box =
      gl::box_chart({"QVM THROUGHPUT", "fib by recursion, one box over the timings (this machine)",
                     "M instructions/s", ""},
                    {s});
  shoot(lab, "throughput", box,
        "Millions of module instructions a second on fib, every load and store checked. A debug "
        "build times the sanitizers too.",
        "a narrow box; a release build several times a debug one", "fib", gl::Det::measured);
}

// A player's module: the facts, the opcode mix, the functions' sizes, and its init export.
std::map<int, int> g_calls;
std::string g_printed;
int32_t logging_host(void*, pg::Qvm& vm, const int32_t* args) {
  g_calls[args[0]]++;
  if (args[0] == 0) return 16;  // the interface version
  if (args[0] == 1 || args[0] == 75) g_printed += pg::qvm_string(vm, args[1], "print");
  if (args[0] == 2)
    pg::qvm_raise(vm, "the module's error: " + std::string(pg::qvm_string(vm, args[1], "error")));
  return 0;
}

void file_section(gl::Lab& lab, const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  const std::vector<uint8_t> file{std::istreambuf_iterator<char>(in), {}};
  pg::Qvm vm;
  std::string error;
  if (file.empty() || !pg::load_qvm(file, vm, error)) {
    lab.fail(base_name(path) + ": " + (file.empty() ? std::string("cannot be read") : error),
             "next: give --file a compiled module (a .qvm); a refusal names the rule and the "
             "instruction (skill: progs)");
    return;
  }
  std::map<int, int> mix;
  std::vector<double> sizes;
  int functions = 0, deepest = 0, biggest_frame = 0;
  for (size_t i = 0; i < vm.code.size(); ++i) {
    const pg::QvmInstr& c = vm.code[i];
    mix[c.op]++;
    if (c.op == pg::kOpEnter) {
      ++functions;
      deepest = std::max(deepest, c.aux);
      biggest_frame = std::max(biggest_frame, c.v);
      sizes.push_back(0);
    }
    sizes.back() += 1;
  }
  lab.table({"the module",
             {"bytes", "instructions", "functions", "memory", "crc", "deepest operand stack",
              "largest frame"},
             {{std::to_string(file.size()), std::to_string(vm.code.size()),
               std::to_string(functions), std::to_string(vm.mem.size()), std::to_string(vm.crc),
               std::to_string(deepest), std::to_string(biggest_frame)}}},
            gl::Det::measured);
  std::vector<std::pair<int, int>> ranked(mix.begin(), mix.end());
  std::sort(ranked.begin(), ranked.end(), [](auto a, auto b) { return a.second > b.second; });
  gl::LabTable ops{"the opcode mix (the 16 most used)", {"opcode", "count", "share %"}, {}};
  for (size_t i = 0; i < ranked.size() && i < 16; ++i)
    ops.rows.push_back({pg::qvm_op_name(ranked[i].first), std::to_string(ranked[i].second),
                        fixed(100.0 * ranked[i].second / static_cast<double>(vm.code.size()), 1)});
  lab.table(ops, gl::Det::measured);
  for (double& s : sizes) s = std::min(s, 400.0);
  shoot(lab, "functions",
        gl::histogram_chart({"MODULE FUNCTIONS BY SIZE", base_name(path),
                             "instructions (400 and over at 400)", "functions"},
                            {{"functions", sizes}}),
        "How many instructions each of the module's functions holds; the largest are counted at "
        "400.",
        "most functions tens to hundreds of instructions; a long tail of a few large ones", "file",
        gl::Det::measured);

  g_calls.clear();
  g_printed.clear();
  vm.syscall = logging_host;
  const auto t0 = std::chrono::steady_clock::now();
  const int32_t r = pg::qvm_call(vm, 0, 0, 1);
  const double ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  gl::LabTable calls{
      "init's system calls (a host that answers only the version)", {"system call", "times"}, {}};
  for (auto [k, v] : g_calls) calls.rows.push_back({std::to_string(k), std::to_string(v)});
  lab.table(calls, gl::Det::measured);
  lab.note("init returned " + std::to_string(r) + " (the game data block's address) after " +
           std::to_string(vm.instructions) + " instructions in " + fixed(ms, 1) + " ms" +
           (vm.fault.empty() ? std::string() : "; fault: " + vm.fault));
  if (!g_printed.empty()) lab.note("it printed: " + g_printed);
  lab.metric("module.instructions", static_cast<double>(vm.code.size()), "count",
             gl::Det::measured);
  lab.metric("module.init", static_cast<double>(vm.instructions), "instructions",
             gl::Det::measured);
}

void run(gl::Lab& lab) {
  pg::Qvm vm;
  std::string error;
  if (!pg::load_qvm(pg::qvm_sample(), vm, error)) {
    lab.fail("the sample module does not load: " + error,
             "run `make test T=progs_qvm` (skill: progs)");
    return;
  }
  vm.syscall = sample_host;
  lab.metric("sample.instructions", static_cast<double>(vm.code.size()), "count");
  sample_section(lab, vm);
  fault_section(lab, vm);
  throughput_section(lab, vm);
  if (!lab.arg("file").empty()) file_section(lab, lab.arg("file"));
}

const gl::LabSpec kSpec{
    "qvm",
    gl::LabKind::sim,
    "plugin:progs",
    "plugins/progs/qvm.hpp running progs/qvm_asm.hpp's sample, and a player's module",
    "progs",
    "How many instructions does a compiled module's export run, how fast does the VM run them, "
    "and what does a real module hold and call when it starts?",
    {
        {"sizes", "4,8,12,16,20", "the sample's n (comma-separated)"},
        {"fib", "25", "the n fib is timed at"},
        {"timings", "9", "timed calls (the runner owns --repeat)"},
        {"file", "", "a compiled module (.qvm) to read and start (measured; never in the tree)"},
    },
    "--sizes=4,8,12 --fib=12 --timings=2",
    run,
};
GE_LAB(kSpec);

}  // namespace
