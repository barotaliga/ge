// The `qc` tool's commands (tool.hpp). The profile's wall-clock time is the only clock read: it is
// printed and kept out of the deterministic answer.
//
// ge:summary The qc tool's commands: parsing, the headless run, the trace and the profile
// ge:keywords qc progs tool disasm run trace profile arguments findings protocol
// ge:skill progs agent-interface
// ge:test tests/progs_tool_test.cpp
#include "progs/tool.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "progs/disasm.hpp"
#include "progs/qc_compile.hpp"
#include "progs/sample.hpp"
#include "progs/vm.hpp"

namespace ge::progs {

namespace {

constexpr const char* kUsage =
    "usage: qc disasm|run|trace|profile <progs> [--function=<name>] [--args=<n,...>] "
    "[--builtins=sample] [--entities=<n>] [--statements] [--repeat=<n>] [--top=<n>]; "
    "qc build <progs.src|file.qc> [--out=<progs>]";

std::string json_escape(const std::string& s) {
  std::string out = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (static_cast<unsigned char>(c) < 0x20) {
      char b[8];
      std::snprintf(b, sizeof b, "\\u%04x", static_cast<unsigned>(c));
      out += b;
    } else {
      out += c;
    }
  }
  return out + "\"";
}

std::string num(double v) { return core::num::to_string(v); }

struct Answer {
  std::ostream& out;
  int exit = 0;
  void finding(const char* severity, const std::string& what, const std::string& where,
               const std::string& next) {
    std::string w = what;
    std::replace(w.begin(), w.end(), '\n', ' ');
    std::replace(w.begin(), w.end(), '\t', ' ');
    out << "@qc finding " << severity << "\t" << w << "\t" << where << "\t" << next << "\n";
    if (std::strcmp(severity, "error") == 0) exit = 1;
  }
  void summary(const std::string& s) { out << "@qc summary " << s << "\n"; }
  void result(const std::string& json) { out << "@qc result " << json << "\n"; }
};

struct Flags {
  std::string command, file, function, args, builtins, entities, repeat = "1", top = "10", out;
  bool statements = false;
  std::string bad;  // the first usage error
};

Flags parse(int argc, const char* const* argv) {
  Flags f;
  for (int i = 1; i < argc; i++) {
    const std::string a = argv[i];
    if (a.rfind("--", 0) == 0) {
      const size_t eq = a.find('=');
      const std::string k = a.substr(2, eq == std::string::npos ? std::string::npos : eq - 2);
      const std::string v = eq == std::string::npos ? "" : a.substr(eq + 1);
      if (k == "function")
        f.function = v;
      else if (k == "args")
        f.args = v;
      else if (k == "builtins")
        f.builtins = v;
      else if (k == "entities")
        f.entities = v;
      else if (k == "repeat")
        f.repeat = v;
      else if (k == "top")
        f.top = v;
      else if (k == "statements")
        f.statements = true;
      else if (k == "out")
        f.out = v;
      else if (f.bad.empty())
        f.bad = "unknown flag --" + k;
    } else if (f.command.empty()) {
      f.command = a;
    } else if (f.file.empty()) {
      f.file = a;
    } else if (f.bad.empty()) {
      f.bad = "one progs file a command, not '" + a + "' too";
    }
  }
  if (f.bad.empty() && f.command != "disasm" && f.command != "run" && f.command != "trace" &&
      f.command != "profile" && f.command != "build")
    f.bad = "unknown command '" + f.command + "'";
  if (f.bad.empty() && f.file.empty()) f.bad = "no progs file given";
  if (f.bad.empty() && f.command != "disasm" && f.command != "build" && f.function.empty())
    f.bad = f.command + " needs --function=<name>";
  if (f.bad.empty() && !f.builtins.empty() && f.builtins != "sample")
    f.bad = "--builtins takes 'sample' (a game's own builtins come with the game)";
  return f;
}

bool load(const std::string& file, Progs& p, Answer& a) {
  std::ifstream in(file, std::ios::binary);
  if (!in) {
    a.finding("error", "cannot read " + file, file, "pass the path of a progs file (.dat)");
    return false;
  }
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
  std::string error;
  // A program that declares no engine-side layout (crc 0: a compiled standalone program, no
  // game's) is run as it stands; any other crc is held to the form it names.
  LoadOptions options = every_form();
  if (bytes.size() >= 8 && bytes[4] == 0 && bytes[5] == 0 && bytes[6] == 0 && bytes[7] == 0) {
    options.any_crc = true;
    options.check_layout = false;
  }
  if (!load_progs(bytes.data(), bytes.size(), p, error, options)) {
    a.finding("error", error, file, "see docs/shooters-edge.md; skill: progs");
    return false;
  }
  // The line table, when one lies beside the file (file.hpp `parse_lines`).
  std::ifstream lines_in(file + ".lines", std::ios::binary);
  if (lines_in) {
    const std::string text((std::istreambuf_iterator<char>(lines_in)),
                           std::istreambuf_iterator<char>());
    std::vector<int32_t> lines;
    if (!parse_lines(text, lines, error) || !set_lines(p, std::move(lines), error))
      a.finding("warning", error, file + ".lines", "faults name no lines until it is rewritten");
  }
  return true;
}

std::string value_text(const Vm& vm, int ofs, uint16_t type) {
  const uint32_t w = vm.globals[static_cast<size_t>(ofs)];
  switch (type & kTypeMask) {
    case kTypeVector:
      return "'" + num(as_float(w)) + " " + num(global_float(vm, ofs + 1)) + " " +
             num(global_float(vm, ofs + 2)) + "'";
    case kTypeString: {
      const char* s = vm_string(vm, static_cast<int32_t>(w));
      return s != nullptr ? "\"" + std::string(s) + "\"" : "<bad string>";
    }
    case kTypeEntity: {
      const int n = entity_number(vm, static_cast<int32_t>(w));
      return n >= 0 ? "entity " + std::to_string(n) : "<bad entity>";
    }
    case kTypeFunction:
      return "function " + std::to_string(static_cast<int32_t>(w));
    default:
      return num(as_float(w));
  }
}

// The trace's hooks: one line a call, a return and a builtin, indented by depth.
struct Tracer {
  std::ostream* out;
  bool statements = false;
  int64_t lines = 0;
};

void trace_enter(void* ctx, const Vm& vm, int32_t fn, int depth) {
  auto& t = *static_cast<Tracer*>(ctx);
  const Progs& p = *vm.progs;
  const Function& f = p.functions[static_cast<size_t>(fn)];
  std::string line(static_cast<size_t>(depth * 2), ' ');
  line += "-> " + std::string(progs_string(p, f.name)) + "(";
  int at = f.parm_start;
  for (int i = 0; i < f.num_parms; i++) {
    line += (i ? ", " : "") + value_text(vm, at, f.parm_size[i] == 3 ? kTypeVector : kTypeFloat);
    at += f.parm_size[i];
  }
  *t.out << line << ")\n";
  t.lines++;
}

void trace_leave(void* ctx, const Vm& vm, int32_t fn, int depth) {
  auto& t = *static_cast<Tracer*>(ctx);
  *t.out << std::string(static_cast<size_t>(depth * 2), ' ') << "<- "
         << progs_string(*vm.progs, vm.progs->functions[static_cast<size_t>(fn)].name) << " = "
         << value_text(vm, kGlobalReturn, kTypeFloat) << " ("
         << static_cast<int32_t>(vm.globals[kGlobalReturn]) << ")\n";
  t.lines++;
}

void trace_builtin(void* ctx, const Vm& vm, int32_t number, int argc, int depth) {
  auto& t = *static_cast<Tracer*>(ctx);
  std::string line(static_cast<size_t>(depth * 2 + 2), ' ');
  line += "builtin " + std::to_string(number) + " " +
          vm.builtins[static_cast<size_t>(number)].name + "(";
  for (int i = 0; i < argc; i++) line += (i ? ", " : "") + value_text(vm, parm(i), kTypeFloat);
  *t.out << line << ") = " << value_text(vm, kGlobalReturn, kTypeFloat) << "\n";
  t.lines++;
}

void trace_statement(void* ctx, const Vm& vm, int32_t, int32_t st) {
  auto& t = *static_cast<Tracer*>(ctx);
  if (!t.statements) return;
  *t.out << std::string(static_cast<size_t>(vm.depth * 2), ' ') << "  "
         << statement_text(*vm.progs, st) << "\n";
  t.lines++;
}

// The VM for a run: builtins, entities and the arguments in the parameter slots.
bool prepare(Vm& vm, const Progs& p, const Flags& f, SampleOutput& sample, int32_t& fn, Answer& a) {
  fn = find_function(p, f.function.c_str());
  if (fn < 0) {
    a.finding("error", "no function named '" + f.function + "' in " + f.file, f.file,
              "run `ge qc disasm " + f.file + "` for the functions it has");
    return false;
  }
  create_vm(vm, p);
  vm.user = &sample;
  if (f.builtins == "sample") install_sample_builtins(vm);
  if (!f.entities.empty()) {
    vm.num_entities = std::clamp(std::atoi(f.entities.c_str()), 1, vm.max_entities);
    sample.entities = vm.num_entities;
  }
  const Function& func = p.functions[static_cast<size_t>(fn)];
  std::vector<float> values;
  for (size_t at = 0; at < f.args.size();) {
    const size_t comma = f.args.find(',', at);
    const std::string one = f.args.substr(at, comma == std::string::npos ? comma : comma - at);
    values.push_back(static_cast<float>(std::strtod(one.c_str(), nullptr)));
    if (comma == std::string::npos) break;
    at = comma + 1;
  }
  size_t k = 0;
  for (int i = 0; i < func.num_parms; i++) {
    for (int j = 0; j < func.parm_size[i]; j++)
      set_global_float(vm, parm(i) + j, k < values.size() ? values[k++] : 0.0f);
  }
  if (k < values.size())
    a.finding("warning",
              std::to_string(values.size() - k) + " of the --args went unused: " + f.function +
                  " takes " + std::to_string(func.num_parms) + " parameters",
              f.file, "pass one number a word of its parameters (see `ge qc disasm`)");
  return true;
}

void report_fault(const Vm& vm, const Flags& f, Answer& a) {
  std::string where = f.file;
  if (vm.fault.depth > 0) where += ": " + frame_text(vm, vm.fault.frames[0]);
  a.out << fault_text(vm) << "\n";
  a.finding("error", std::string("QC fault: ") + vm.fault.message, where,
            "run `ge qc trace " + f.file + " --function=" + f.function +
                " --statements` to see the statements before it; skill: progs");
}

std::string run_result(const Vm& vm, const Flags& f, int64_t statements) {
  return "{\"function\":" + json_escape(f.function) +
         ",\"ok\":" + (vm.fault.set ? "false" : "true") + ",\"return\":[" +
         num(global_float(vm, 1)) + "," + num(global_float(vm, 2)) + "," +
         num(global_float(vm, 3)) +
         "],\"return_word\":" + std::to_string(static_cast<int32_t>(vm.globals[1])) +
         ",\"statements\":" + std::to_string(statements) +
         ",\"fault\":" + json_escape(vm.fault.set ? vm.fault.message : "") + "}";
}

int disasm(const Flags& f, Answer& a) {
  Progs p;
  if (!load(f.file, p, a)) return a.exit;
  a.out << summary_text(p) << "\n";
  int builtins = 0;
  for (const Function& fn : p.functions) builtins += fn.first_statement < 0 ? 1 : 0;
  if (!f.function.empty()) {
    const int fn = find_function(p, f.function.c_str());
    if (fn < 0) {
      a.finding("error", "no function named '" + f.function + "'", f.file,
                "run `ge qc disasm " + f.file + "` without --function for every one");
      return a.exit;
    }
    a.out << function_text(p, fn);
  } else {
    for (size_t i = 1; i < p.functions.size(); i++) a.out << function_text(p, static_cast<int>(i));
  }
  a.summary(summary_text(p));
  a.result("{\"version\":" + std::to_string(p.version) + ",\"crc\":" + std::to_string(p.crc) +
           ",\"statements\":" + std::to_string(p.statements.size()) + ",\"functions\":" +
           std::to_string(p.functions.size()) + ",\"builtins\":" + std::to_string(builtins) +
           ",\"globals\":" + std::to_string(p.globals.size()) +
           ",\"fields\":" + std::to_string(p.fielddefs.size()) +
           ",\"entityfields\":" + std::to_string(p.entityfields) +
           ",\"lines\":" + (p.lines.empty() ? "false" : "true") + "}");
  return a.exit;
}

int run(const Flags& f, Answer& a, bool traced) {
  Progs p;
  if (!load(f.file, p, a)) return a.exit;
  Vm vm;
  SampleOutput sample;
  int32_t fn = 0;
  if (!prepare(vm, p, f, sample, fn, a)) return a.exit;
  Tracer tracer{&a.out, f.statements};
  TraceHooks hooks{&tracer, trace_enter, trace_leave, trace_builtin, trace_statement};
  if (traced) vm.trace = &hooks;
  const bool ok = call_function(vm, fn);
  if (!sample.text.empty())
    a.out << "printed: " << sample.text << (sample.text.back() == '\n' ? "" : "\n");
  if (!ok)
    report_fault(vm, f, a);
  else
    a.out << f.function << " returned " << value_text(vm, kGlobalReturn, kTypeFloat) << " (word "
          << static_cast<int32_t>(vm.globals[kGlobalReturn]) << ", as a vector "
          << value_text(vm, kGlobalReturn, kTypeVector) << ") after " << vm.statements_run
          << " statements\n";
  a.summary(f.function + (ok ? " returned " + num(global_float(vm, kGlobalReturn)) : " faulted") +
            " after " + std::to_string(vm.statements_run) + " statements" +
            (traced ? ", " + std::to_string(tracer.lines) + " trace lines" : ""));
  a.result(run_result(vm, f, vm.statements_run));
  return a.exit;
}

int profile(const Flags& f, Answer& a) {
  Progs p;
  if (!load(f.file, p, a)) return a.exit;
  Vm vm;
  SampleOutput sample;
  int32_t fn = 0;
  if (!prepare(vm, p, f, sample, fn, a)) return a.exit;
  const int repeat = std::max(1, std::atoi(f.repeat.c_str()));
  const std::vector<uint32_t> start_globals = vm.globals;
  const auto t0 = std::chrono::steady_clock::now();
  bool ok = true;
  for (int r = 0; r < repeat && ok; r++) {
    // Each repeat from the same globals; entities and strings carry on as a game's would.
    std::copy(start_globals.begin(), start_globals.end(), vm.globals.begin());
    ok = call_function(vm, fn);
  }
  const double ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  if (!ok) report_fault(vm, f, a);
  std::vector<int> order;
  for (size_t i = 1; i < vm.profile.size(); i++) {
    if (vm.profile[i].calls > 0 || vm.profile[i].statements > 0)
      order.push_back(static_cast<int>(i));
  }
  std::stable_sort(order.begin(), order.end(), [&](int x, int y) {
    return vm.profile[static_cast<size_t>(x)].statements >
           vm.profile[static_cast<size_t>(y)].statements;
  });
  const size_t top = static_cast<size_t>(std::max(1, std::atoi(f.top.c_str())));
  char line[160];
  std::snprintf(line, sizeof line, "%-24s %12s %6s %10s\n", "function", "statements", "share",
                "calls");
  a.out << line;
  std::string rows;
  for (size_t i = 0; i < order.size() && i < top; i++) {
    const FunctionProfile& fp = vm.profile[static_cast<size_t>(order[i])];
    const char* name = progs_string(p, p.functions[static_cast<size_t>(order[i])].name);
    std::snprintf(line, sizeof line, "%-24s %12lld %5.1f%% %10lld\n", name,
                  static_cast<long long>(fp.statements),
                  vm.statements_run > 0 ? 100.0 * static_cast<double>(fp.statements) /
                                              static_cast<double>(vm.statements_run)
                                        : 0.0,
                  static_cast<long long>(fp.calls));
    a.out << line;
    rows += std::string(rows.empty() ? "" : ",") + "{\"function\":" + json_escape(name) +
            ",\"statements\":" + std::to_string(fp.statements) +
            ",\"calls\":" + std::to_string(fp.calls) + "}";
  }
  std::string hit;
  for (size_t n = 1; n < vm.builtin_calls.size(); n++) {
    if (vm.builtin_calls[n] == 0) continue;
    a.out << "builtin " << n << " " << vm.builtins[n].name << ": " << vm.builtin_calls[n]
          << " calls\n";
    hit += std::string(hit.empty() ? "" : ",") + "{\"builtin\":" + std::to_string(n) +
           ",\"name\":" + json_escape(vm.builtins[n].name) +
           ",\"calls\":" + std::to_string(vm.builtin_calls[n]) + "}";
  }
  const double per_s = ms > 0 ? static_cast<double>(vm.statements_run) / (ms / 1000.0) : 0.0;
  a.out << vm.statements_run << " statements in " << core::num::to_fixed(ms, 3) << " ms ("
        << core::num::to_fixed(per_s / 1e6, 1) << " M statements/s, measured)\n";
  a.summary(f.function + " x" + std::to_string(repeat) + ": " + std::to_string(vm.statements_run) +
            " statements, " + std::to_string(order.size()) + " functions, " +
            core::num::to_fixed(ms, 3) + " ms");
  a.result("{\"function\":" + json_escape(f.function) + ",\"repeat\":" + std::to_string(repeat) +
           ",\"statements\":" + std::to_string(vm.statements_run) +
           ",\"ms\":" + core::num::to_fixed(ms, 3) + ",\"functions\":[" + rows +
           "],\"builtins\":[" + hit + "]}");
  return a.exit;
}

// `qc build`: a project file (progs.src) or one source compiled; the progs and its line table
// written; every diagnostic a finding at its file and line.
int build(const Flags& f, Answer& a) {
  const std::filesystem::path in(f.file);
  QcProject project;
  if (in.extension() == ".qc") {
    std::ifstream file(in, std::ios::binary);
    if (!file) {
      a.finding("error", "cannot read " + f.file, f.file, "pass a progs.src or a .qc file");
      return a.exit;
    }
    std::ostringstream text;
    text << file.rdbuf();
    const QcSource src[] = {{in.filename().string(), text.str()}};
    project.result = compile_qc(src);
    project.output = std::filesystem::path(in).replace_extension(".dat");
  } else {
    project = compile_project(in);
  }
  const std::filesystem::path dir = in.parent_path();
  for (const QcDiagnostic& d : project.result.diagnostics) {
    const std::string where = (dir / d.file).generic_string() + ":" + std::to_string(d.line);
    a.finding(d.error ? "error" : "warning", d.message, where,
              d.error ? "fix the source and build again (skill: progs, § The QC language)"
                      : "the progs builds; a game may refuse it");
  }
  if (!project.result.ok) {
    a.summary("the build stopped at the first error");
    return a.exit;
  }
  const std::filesystem::path out = f.out.empty() ? project.output : std::filesystem::path(f.out);
  const Progs& p = project.result.progs;
  const std::vector<uint8_t> bytes = write_progs(p);
  std::ofstream(out, std::ios::binary)
      .write(reinterpret_cast<const char*>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  std::ofstream(out.string() + ".lines") << lines_text(p.lines);
  if (!std::filesystem::exists(out)) {
    a.finding("error", "cannot write " + out.generic_string(), out.generic_string(),
              "check the directory exists and is writable");
    return a.exit;
  }
  int builtins = 0;
  for (const Function& fn : p.functions) builtins += fn.first_statement < 0 ? 1 : 0;
  a.summary(out.generic_string() + ": " + std::to_string(bytes.size()) + " bytes, " +
            std::to_string(p.functions.size() - 1 - static_cast<size_t>(builtins)) +
            " functions, " + std::to_string(builtins) + " builtins, " +
            std::to_string(p.statements.size()) + " statements, crc " + std::to_string(p.crc));
  a.result("{\"out\":" + json_escape(out.generic_string()) + ",\"bytes\":" +
           std::to_string(bytes.size()) + ",\"statements\":" + std::to_string(p.statements.size()) +
           ",\"functions\":" + std::to_string(p.functions.size()) + ",\"globals\":" +
           std::to_string(p.globals.size()) + ",\"crc\":" + std::to_string(p.crc) + "}");
  return a.exit;
}

}  // namespace

int qc_main(int argc, const char* const* argv, std::ostream& out) {
  Answer a{out};
  const Flags f = parse(argc, argv);
  if (!f.bad.empty()) {
    a.finding("error", f.bad, "", std::string("run `ge help qc run`; ") + kUsage);
    return 2;
  }
  if (f.command == "disasm") return disasm(f, a);
  if (f.command == "build") return build(f, a);
  if (f.command == "profile") return profile(f, a);
  return run(f, a, f.command == "trace");
}

}  // namespace ge::progs
