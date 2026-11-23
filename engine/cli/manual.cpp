// `ge manual`, defined (manual.hpp).
//
// ge:summary ge manual defined: the sources read, merged and written as the page and its JSON
// ge:keywords cli manual run_manual user-manual manual-json
// ge:skill agent-interface console
// ge:test tests/cli_manual_test.cpp
#include "cli/manual.hpp"

#include "cli/json.hpp"
#include "console/manual.hpp"
#include "console/manual_page.hpp"

namespace ge::cli {

Outcome run_manual(const ManualOptions& o) {
  Outcome out;
  console::ManualSource all;
  for (const std::string& path : o.sources) {
    const std::optional<std::string> text = read_text(path);
    if (!text) {
      out.findings.push_back({Severity::error, "no manual source at " + path, path,
                              "write it with se -manual <file> or sesv --manual=<file>"});
      continue;
    }
    std::string error;
    if (!console::read_manual(*text, all, error))
      out.findings.push_back({Severity::error, error, path, "console/manual.hpp's format"});
  }
  if (count_of(out.findings, Severity::error) > 0) return out;
  // A program or a game named by two sources is listed once.
  const auto once = [](std::vector<console::ManualName>& v) {
    std::vector<console::ManualName> kept;
    for (console::ManualName& n : v) {
      bool seen = false;
      for (const console::ManualName& k : kept) seen = seen || k.id == n.id;
      if (!seen) kept.push_back(std::move(n));
    }
    v = std::move(kept);
  };
  once(all.programs);
  once(all.games);
  const std::vector<console::ManualRow> rows = console::manual_rows(all);
  const console::ManualPage page{o.title, o.intro, o.version};
  const fs::path html(o.out);
  const fs::path json = html.parent_path() / "manual.json";
  std::string error;
  if (!write_text(html, console::manual_html(all, rows, page), error) ||
      !write_text(json, console::manual_json(all, rows, page), error)) {
    out.findings.push_back({Severity::error, error, o.out, "pick a folder that can be written"});
    return out;
  }
  out.written = {html.string(), json.string()};
  int settings = 0, commands = 0, flags = 0;
  for (const console::ManualRow& r : rows) {
    settings += r.kind == console::ManualKind::setting ? 1 : 0;
    commands += r.kind == console::ManualKind::command ? 1 : 0;
    flags += r.kind == console::ManualKind::flag ? 1 : 0;
  }
  out.summary = std::to_string(settings) + " settings, " + std::to_string(commands) +
                " commands and " + std::to_string(flags) + " flags from " +
                std::to_string(o.sources.size()) + " sources written to " + o.out;
  out.result = "{\"settings\":" + std::to_string(settings) +
               ",\"commands\":" + std::to_string(commands) + ",\"flags\":" + std::to_string(flags) +
               ",\"out\":" + json_str(o.out) + "}";
  return out;
}

}  // namespace ge::cli
