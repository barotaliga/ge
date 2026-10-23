// The console's bind commands:
//
//   bind [-<mode>] <key> [<command>]   bind a key (no command: print its bind)
//   unbind [-<mode>] <key>             unbindall                bindlist [-<mode>]
//
// ge:summary The console's bind commands: bind, unbind, unbindall, bindlist
// ge:keywords console bind unbind unbindall bindlist mode key
// ge:skill console
// ge:test tests/console_binds_test.cpp
#include "console/console.hpp"
#include "console/lex.hpp"

namespace ge::console {

namespace {

// A leading `-mode` word read off `a`; returns the index of the next word, 0 on an error.
size_t read_mode(Console& c, const Args& a, BindMode& mode) {
  mode = BindMode::game;
  if (a.size() > 1 && a[1].size() > 1 && a[1][0] == '-') {
    if (!parse_mode(lower(a[1].substr(1)), mode)) {
      c.error("'" + a[1] + "' is not a bind mode", "use -game, -console, -menu or -demo");
      return 0;
    }
    return 2;
  }
  return 1;
}

bool read_key(Console& c, const Args& a, size_t i, std::string& key) {
  if (i >= a.size()) {
    c.error(a[0] + " needs a key", "write `" + a[0] + " <key>`; key names are in docs/console.md");
    return false;
  }
  if (!normalize_key(a[i], key)) {
    c.error("'" + a[i] + "' is not a key",
            "use a key's name (a, f1, space, mouse1, mwheelup, pad_a, ctrl+s): docs/console.md");
    return false;
  }
  return true;
}

}  // namespace

void register_bind_commands(Console& con) {
  con.add_command(
      "bind", "bind [-<mode>] <key> [<command>]: what a key runs",
      [](Console& c, const Args& a) {
        BindMode mode;
        const size_t i = read_mode(c, a, mode);
        std::string key;
        if (i == 0 || !read_key(c, a, i, key)) return;
        if (a.size() == i + 1) {
          const std::string& cmd = c.binds().get(mode, key);
          c.print(cmd.empty() ? key + " is not bound" : key + " = \"" + cmd + "\"");
          return;
        }
        const std::string rest = rest_after(a.raw, i + 1);
        c.binds().set(mode, key, !rest.empty() && rest.front() == '"' ? a[i + 1] : rest);
      },
      kNoExpand);
  con.add_command("unbind", "unbind [-<mode>] <key>: a key runs nothing",
                  [](Console& c, const Args& a) {
                    BindMode mode;
                    const size_t i = read_mode(c, a, mode);
                    std::string key;
                    if (i == 0 || !read_key(c, a, i, key)) return;
                    c.binds().remove(mode, key);
                  });
  con.add_command("unbindall", "unbindall: every key in every mode runs nothing",
                  [](Console& c, const Args&) { c.binds().clear(); });
  con.add_command("bindlist", "bindlist [-<mode>]: every bind", [](Console& c, const Args& a) {
    BindMode mode;
    const size_t i = read_mode(c, a, mode);
    if (i == 0) return;
    int n = 0;
    for (const auto& [k, cmd] : c.binds().table(mode)) {
      c.print(k + " \"" + cmd + "\"");
      ++n;
    }
    c.print(std::to_string(n) + " binds (" + std::string(mode_name(mode)) + ")");
  });
}

}  // namespace ge::console
