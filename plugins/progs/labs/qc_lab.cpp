// THE qc LAB: what a QC program costs and where its statements go. It runs the sample program
// (progs/sample.hpp, every opcode) at several sizes and tables and pictures each function's share
// of the statements and its calls (deterministic: the digest holds them), checks the run replays
// to the bit, shows a fault's QC stack trace, and times groups of opcodes in tight loops built
// with the assembler (measured: statements a second, outside the digest). It writes the sample as
// previews/qc/sample.dat with its line table, so `ge qc` can be tried on it.
//
// ge:summary The qc lab: the sample QC program's profile by size, opcode throughput, a fault
// ge:keywords lab qc progs profile throughput opcode statements calls heatmap box-chart fault
// ge:keywords sample replay digest bench
// ge:skill progs labs
// ge:test tests/progs_vm_test.cpp
#include <algorithm>
#include <bit>
#include <chrono>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "progs/assembler.hpp"
#include "progs/sample.hpp"
#include "progs/vm.hpp"

namespace {

namespace gl = ge::lab;
namespace pg = ge::progs;

std::string fixed(double v, int d) { return ge::core::num::to_fixed(v, d); }

uint64_t fnv(const std::vector<uint32_t>& words) {
  uint64_t h = 0xcbf29ce484222325ull;
  for (uint32_t w : words) {
    for (int i = 0; i < 4; i++) h = (h ^ ((w >> (8 * i)) & 0xffu)) * 0x100000001b3ull;
  }
  return h;
}

void shoot(gl::Lab& lab, const std::string& id, const gl::Chart& chart, const std::string& caption,
           const std::string& look_for, gl::Det det) {
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()), det);
  lab.shot(
      {id,
       chart.title,
       caption,
       look_for,
       {{"sizes", lab.arg("sizes")}, {"iters", lab.arg("iters")}, {"timings", lab.arg("timings")}}},
      gl::to_picture(chart.plot), det);
}

// One run of the sample's run(n): the VM after it.
struct SampleRun {
  pg::Vm vm;
  pg::SampleOutput out;
  bool ok = false;
};

void run_sample(const pg::Progs& p, float n, SampleRun& r) {
  pg::create_vm(r.vm, p);
  r.vm.user = &r.out;
  pg::install_sample_builtins(r.vm);
  pg::set_global_float(r.vm, pg::parm(0), n);
  r.ok = pg::call_function(r.vm, pg::find_function(p, "run"));
}

// The throughput programs: one function a group, `iters` rounds of eight of its opcodes.
struct Group {
  const char* name;
  const char* what;
};
constexpr Group kGroups[] = {
    {"float", "ADD_F MUL_F SUB_F DIV_F"}, {"vector", "ADD_V SUB_V MUL_VF MUL_V"},
    {"compare", "LT EQ_F NE_V NOT_F"},    {"field", "LOAD_F LOAD_V ADDRESS STOREP_F"},
    {"call", "CALL0 and its DONE"},       {"string", "EQ_S NOT_S"},
    {"branch", "IF IFNOT (not taken)"},
};

std::vector<uint8_t> bench_program(std::vector<int32_t>& fns) {
  pg::Assembler a;
  const int x = a.constant_f(1.25f), y = a.constant_f(0.75f), v = a.constant_v(1, 2, 3);
  const int w = a.constant_v(0.5f, -1, 2), zero = a.constant_f(0), one = a.constant_f(1);
  const int s1 = a.constant_s("alpha"), s2 = a.constant_s("alphb");
  const int ent = a.constant_i(0), fld = a.field("velocity", pg::kTypeVector);
  const int fld_f = a.field("health", pg::kTypeFloat);
  const int32_t empty = a.begin_function("empty", {}, 0);
  a.emit(pg::kOpDone);
  a.end_function();
  for (const Group& g : kGroups) {
    const int32_t fn = a.begin_function(g.name, {1}, 12);
    fns.push_back(fn);
    const int n = a.param(0), i = a.local(0), t = a.local(1), r = a.local(2), q = a.local(5);
    const int e = a.local(8), p = a.local(9);
    const int top = a.label(), done = a.label();
    a.emit(pg::kOpStoreF, zero, i);
    a.emit(pg::kOpStoreEnt, ent, e);
    // Entity 1: the world's successor, in use for the field group.
    a.emit(pg::kOpAddF, zero, zero, t);
    a.place(top);
    a.emit(pg::kOpLt, i, n, t);
    a.jump(pg::kOpIfNot, done, t);
    for (int k = 0; k < 2; k++) {
      const std::string name = g.name;
      if (name == "float") {
        a.emit(pg::kOpAddF, x, y, t);
        a.emit(pg::kOpMulF, x, y, t);
        a.emit(pg::kOpSubF, x, y, t);
        a.emit(pg::kOpDivF, x, y, t);
      } else if (name == "vector") {
        a.emit(pg::kOpAddV, v, w, r);
        a.emit(pg::kOpSubV, v, w, q);
        a.emit(pg::kOpMulVF, v, x, r);
        a.emit(pg::kOpMulV, v, w, t);
      } else if (name == "compare") {
        a.emit(pg::kOpLt, x, y, t);
        a.emit(pg::kOpEqF, x, y, t);
        a.emit(pg::kOpNeV, v, w, t);
        a.emit(pg::kOpNotF, x, 0, t);
      } else if (name == "field") {
        a.emit(pg::kOpLoadF, e, fld_f, t);
        a.emit(pg::kOpLoadV, e, fld, r);
        a.emit(pg::kOpAddress, e, fld_f, p);
        a.emit(pg::kOpStorePF, x, p);
      } else if (name == "call") {
        a.emit(pg::kOpCall0, a.constant_fn(empty));
        a.emit(pg::kOpCall0, a.constant_fn(empty));
      } else if (name == "string") {
        a.emit(pg::kOpEqS, s1, s2, t);
        a.emit(pg::kOpNotS, s1, 0, t);
        a.emit(pg::kOpEqS, s1, s1, t);
        a.emit(pg::kOpNotS, s2, 0, t);
      } else {
        const int skip = a.label();
        a.jump(pg::kOpIf, skip, zero);
        a.jump(pg::kOpIfNot, skip, one);
        a.jump(pg::kOpIf, skip, zero);
        a.jump(pg::kOpIfNot, skip, one);
        a.place(skip);
      }
    }
    a.emit(pg::kOpAddF, i, one, i);
    a.jump(pg::kOpGoto, top);
    a.place(done);
    a.emit(pg::kOpDone);
    a.end_function();
  }
  std::vector<int32_t> lines;
  return a.bytes();
}

void profile_section(gl::Lab& lab, const pg::Progs& p) {
  const std::vector<double> sizes = lab.arg_nums("sizes");
  std::vector<std::string> names;
  std::vector<int> fns;
  for (size_t f = 1; f < p.functions.size(); f++) {
    if (p.functions[f].first_statement > 0) {
      names.push_back(pg::progs_string(p, p.functions[f].name));
      fns.push_back(static_cast<int>(f));
    }
  }
  std::vector<double> share(names.size() * sizes.size(), 0.0);
  gl::LabTable table{"the sample's run(n): each function's statements and calls",
                     {"n", "function", "statements", "share %", "calls"},
                     {}};
  for (size_t c = 0; c < sizes.size(); c++) {
    const float n = static_cast<float>(sizes[c]);
    SampleRun a, b;
    run_sample(p, n, a);
    run_sample(p, n, b);
    const std::string key = "n-" + fixed(sizes[c], 0);
    if (!a.ok) {
      lab.fail("the sample's run(" + fixed(sizes[c], 0) + ") faulted: " + pg::fault_text(a.vm),
               "run `make test T=progs` (skill: progs)");
      continue;
    }
    if (fnv(a.vm.globals) != fnv(b.vm.globals) || fnv(a.vm.entities) != fnv(b.vm.entities))
      lab.fail("two runs of run(" + fixed(sizes[c], 0) + ") disagree: the VM keeps state",
               "run tests/progs_vm_test.cpp (skill: progs, determinism)");
    lab.metric(key + ".checksum", static_cast<double>(pg::global_float(a.vm, pg::kGlobalReturn)),
               "value");
    lab.metric(key + ".statements", static_cast<double>(a.vm.statements_run), "count");
    int64_t builtins = 0;
    for (int64_t k : a.vm.builtin_calls) builtins += k;
    lab.metric(key + ".builtin-calls", static_cast<double>(builtins), "count");
    for (size_t r = 0; r < fns.size(); r++) {
      const pg::FunctionProfile& fp = a.vm.profile[static_cast<size_t>(fns[r])];
      const double pct = 100.0 * static_cast<double>(fp.statements) /
                         static_cast<double>(std::max<int64_t>(1, a.vm.statements_run));
      share[r * sizes.size() + c] = pct;
      table.rows.push_back({fixed(sizes[c], 0), names[r], std::to_string(fp.statements),
                            fixed(pct, 1), std::to_string(fp.calls)});
    }
  }
  lab.table(table);
  const gl::Chart heat = gl::heatmap_chart(
      {"WHERE THE SAMPLE'S STATEMENTS GO, BY SIZE", "share of run(n)'s statements, each function",
       "n (the column's size)", "%"},
      names, share, sizes.front(), sizes.back());
  shoot(lab, "profile", heat,
        "Rows are the sample's functions, columns run(n) at each size; darker is a larger share "
        "of the statements run; a ring is the outlier rule's finding (a fixed cost's share falling "
        "as fib grows).",
        "fib's row darkens to the right (its calls grow as the Fibonacci numbers); vectors grows "
        "linearly; entities, strings and bits fade as fixed costs",
        gl::Det::deterministic);
}

void fault_section(gl::Lab& lab, const pg::Progs& p) {
  pg::Vm vm;
  pg::create_vm(vm, p);
  const bool ok = pg::call_function(vm, pg::find_function(p, "fault"));
  if (ok) {
    lab.fail("the sample's fault() returned instead of faulting",
             "run `make test T=progs_vm` (skill: progs)");
    return;
  }
  lab.metric("fault.frames", vm.fault.depth, "count");
  const pg::Frame& f = vm.fault.frames[0];
  const int32_t line = p.lines.empty() ? 0 : p.lines[static_cast<size_t>(f.statement)];
  lab.metric("fault.line", line, "line");
  if (line == 0) {
    lab.fail("the sample's fault has no line: the line table did not reach the fault",
             "run `make test T=progs_vm` and read plugins/progs/sample.cpp (skill: progs)");
  }
  const std::string text = pg::fault_text(vm);
  for (size_t at = 0;;) {
    const size_t nl = text.find('\n', at);
    lab.note(text.substr(at, nl == std::string::npos ? std::string::npos : nl - at));
    if (nl == std::string::npos) break;
    at = nl + 1;
  }
}

void throughput_section(gl::Lab& lab) {
  std::vector<int32_t> fns;
  const std::vector<uint8_t> bytes = bench_program(fns);
  pg::Progs p;
  std::string error;
  if (!pg::load_progs(bytes.data(), bytes.size(), p, error)) {
    lab.fail("the throughput program does not load: " + error,
             "fix bench_program in plugins/progs/labs/qc_lab.cpp (skill: progs)");
    return;
  }
  pg::Vm vm;
  pg::create_vm(vm, p);
  vm.num_entities = 2;
  const float iters = static_cast<float>(lab.arg_num("iters"));
  const int repeat = std::max(1, static_cast<int>(lab.arg_num("timings")));
  std::vector<gl::Sample> samples;
  gl::LabTable table{"opcode throughput: " + fixed(iters, 0) + " rounds of eight, " +
                         std::to_string(repeat) + " times (measured)",
                     {"group", "opcodes", "statements a call", "median M statements/s"},
                     {}};
  for (size_t g = 0; g < fns.size(); g++) {
    gl::Sample s{kGroups[g].name, {}};
    int64_t per_call = 0;
    for (int r = 0; r < repeat; r++) {
      pg::set_global_float(vm, pg::parm(0), iters);
      const int64_t before = vm.statements_run;
      const auto t0 = std::chrono::steady_clock::now();
      const bool ok = pg::call_function(vm, fns[g]);
      const double s_elapsed =
          std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      if (!ok) {
        lab.fail(
            std::string("throughput group ") + kGroups[g].name + " faulted: " + pg::fault_text(vm),
            "lower --iters under the run-away limit (skill: progs)");
        pg::clear_fault(vm);
        break;
      }
      per_call = vm.statements_run - before;
      s.values.push_back(static_cast<double>(per_call) / std::max(s_elapsed, 1e-9) / 1e6);
    }
    if (s.values.empty()) continue;
    std::vector<double> sorted = s.values;
    std::sort(sorted.begin(), sorted.end());
    const double median = sorted[sorted.size() / 2];
    lab.metric(std::string("throughput.") + kGroups[g].name, median, "M/s", gl::Det::measured);
    lab.metric(std::string("statements.") + kGroups[g].name, static_cast<double>(per_call),
               "count");
    table.rows.push_back(
        {kGroups[g].name, kGroups[g].what, std::to_string(per_call), fixed(median, 1)});
    samples.push_back(std::move(s));
  }
  lab.table(table, gl::Det::measured);
  if (samples.empty()) return;
  const gl::Chart box = gl::box_chart(
      {"QC OPCODE THROUGHPUT", "each group's loop, one box over the rounds (this machine)",
       "M statements/s", ""},
      samples);
  shoot(lab, "throughput", box,
        "One box per group of opcodes: millions of statements a second over the repeated "
        "rounds, the loop's own statements included. A debug build times the sanitizers.",
        "vector the slowest (three words a statement), then call (a frame pushed and popped); "
        "branch the fastest; no box spread wider than its neighbours by far",
        gl::Det::measured);
}

void run(gl::Lab& lab) {
  pg::Progs p;
  std::string error;
  if (!pg::load_sample(p, error)) {
    lab.fail("the sample does not load: " + error, "run `make test T=progs` (skill: progs)");
    return;
  }
  std::vector<int32_t> lines;
  const std::vector<uint8_t> bytes = pg::build_sample(lines);
  lab.attach("sample.dat", bytes);
  const std::string lt = pg::lines_text(lines);
  lab.attach("sample.dat.lines", std::vector<uint8_t>(lt.begin(), lt.end()));
  lab.metric("sample.statements", static_cast<double>(p.statements.size()), "count");
  lab.metric("sample.functions", static_cast<double>(p.functions.size()), "count");
  profile_section(lab, p);
  fault_section(lab, p);
  throughput_section(lab);
  lab.note(
      "try: build/debug/ge qc profile previews/qc/sample.dat --function=run --args=12 "
      "--builtins=sample");
}

const gl::LabSpec kSpec{
    "qc",
    gl::LabKind::sim,
    "plugin:progs",
    "plugins/progs/vm.hpp running progs/sample.hpp and assembled opcode loops",
    "progs",
    "Where do a QC program's statements go as it grows, and how many statements a second does "
    "each group of opcodes run?",
    {
        {"sizes", "6,9,12,15,18", "the sample's run(n) sizes profiled (comma-separated)"},
        {"iters", "4000", "rounds of eight opcodes a throughput call (under the run-away limit)"},
        {"timings", "15", "throughput calls a group (the runner owns --repeat)"},
    },
    "--sizes=4,6,8 --iters=300 --timings=3",
    run,
};
GE_LAB(kSpec);

}  // namespace
