// The console's string commands (script.cpp's header lists them): a cvar's value put into or
// taken out of an alias's text or another cvar's value, a cvar set to an alias's text or a key's
// bind, and a text's hash. The forms old teamplay configs use to build a list of weapons or a
// message piece by piece.
//
// ge:summary alias_in, alias_out, cvar_in, cvar_out, set_alias_str, set_bind_str and hash
// ge:keywords alias_in alias_out cvar_in cvar_out set_alias_str set_bind_str hash string-commands
// ge:keywords config-scripting insert-value remove-value
// ge:skill console
// ge:test tests/console_script_test.cpp
#include <cstdint>
#include <string>

#include "console/binds.hpp"
#include "console/console.hpp"
#include "console/lex.hpp"

namespace ge::console {

namespace {

constexpr size_t kMostText = 1023;  // an alias's text or a cvar's value built this way, at most

int options_of(const Args& a, size_t at) {
  double n = 0;
  return a.size() > at && parse_number(a[at], n) ? static_cast<int>(n) : 0;
}

// `alias_in` and `cvar_in` (`into_alias`): the second name's value put into the first's text.
void put_in(Console& c, const Args& a, bool into_alias) {
  const std::string& what = a[0];
  if (a.size() < 3 || a.size() > 4) {
    c.print(what + (into_alias ? " <alias> <cvar> [<options>]" : " <cvar1> <cvar2> [<options>]"));
    return;
  }
  const int opt = options_of(a, 3);
  const Cvar* from = c.cvars().find(a[2]);
  if (from == nullptr) {
    c.print(what + ": unknown cvar \"" + a[2] + "\"");
    return;
  }
  std::string text;
  if (into_alias) {
    const auto it = c.aliases().find(lower(a[1]));
    if (it == c.aliases().end() && (opt & 8) == 0) {
      c.print(what + ": unknown alias \"" + a[1] + "\"");
      return;
    }
    if (it != c.aliases().end()) text = it->second.body;
  } else {
    const Cvar* to = c.cvars().find(a[1]);
    if (to == nullptr && (opt & 8) == 0) {
      c.print(what + ": unknown cvar \"" + a[1] + "\"");
      return;
    }
    if (to != nullptr) text = to->value;
  }
  if ((opt & 2) != 0 && text.find(from->value) != std::string::npos) {
    if ((opt & 4) != 0) c.print(what + ": already inserted");
    return;
  }
  text = (opt & 1) != 0 ? text + from->value : from->value + text;
  if (text.size() > kMostText) text.resize(kMostText);
  if (into_alias) {
    const auto it = c.aliases().find(lower(a[1]));
    c.set_alias(a[1], text, it != c.aliases().end() && it->second.temp);
  } else {
    const std::string why = set_result_text(c.cvars().set(a[1], text, a.source, true), a[1]);
    if (!why.empty()) c.error(why, "");
  }
}

// `alias_out` and `cvar_out`: the first place the second name's value is found taken out.
void take_out(Console& c, const Args& a, bool from_alias) {
  const std::string& what = a[0];
  if (a.size() < 3 || a.size() > 4) {
    c.print(what + (from_alias ? " <alias> <cvar> [<options>]" : " <cvar1> <cvar2> [<options>]"));
    return;
  }
  const int opt = options_of(a, 3);
  const Cvar* piece = c.cvars().find(a[2]);
  if (piece == nullptr) {
    c.print(what + ": unknown cvar \"" + a[2] + "\"");
    return;
  }
  std::string text;
  bool temp = false;
  if (from_alias) {
    const auto it = c.aliases().find(lower(a[1]));
    if (it == c.aliases().end()) {
      c.print(what + ": unknown alias \"" + a[1] + "\"");
      return;
    }
    text = it->second.body;
    temp = it->second.temp;
  } else {
    const Cvar* v = c.cvars().find(a[1]);
    if (v == nullptr) {
      c.print(what + ": unknown cvar \"" + a[1] + "\"");
      return;
    }
    text = v->value;
  }
  const size_t at = piece->value.empty() ? std::string::npos : text.find(piece->value);
  if (at == std::string::npos) {
    if ((opt & 2) == 0) c.print(what + ": not found");
    return;
  }
  if ((opt & 1) != 0) return;  // a test only
  text.erase(at, piece->value.size());
  if (from_alias) {
    c.set_alias(a[1], text, temp);
  } else {
    const std::string why = set_result_text(c.cvars().set(a[1], text, a.source, true), a[1]);
    if (!why.empty()) c.error(why, "");
  }
}

// The cvar `name` set to `value`, made when there is none; false (said) when a command has the
// name.
bool set_made(Console& c, const Args& a, const std::string& name, const std::string& value) {
  if (c.has_command(name)) {
    c.print("\"" + name + "\" is a command");
    return false;
  }
  const std::string why = set_result_text(c.cvars().set(name, value, a.source, true), name);
  if (!why.empty()) c.error(why, "");
  return true;
}

}  // namespace

uint32_t text_hash(std::string_view text) {
  uint32_t h = 0;
  for (const char ch : text) {
    const uint32_t c = static_cast<uint32_t>(static_cast<unsigned char>(ch)) & ~0x20u;
    h = c + (h << 6) + (h << 16) - h;  // wraps at 2^32
  }
  return h;
}

void register_script_strings(Console& con) {
  con.add_command("alias_in", "alias_in <alias> <cvar> [<options>]: the cvar's value put in",
                  [](Console& c, const Args& a) { put_in(c, a, true); });
  con.add_command("alias_out", "alias_out <alias> <cvar> [<options>]: the cvar's value taken out",
                  [](Console& c, const Args& a) { take_out(c, a, true); });
  con.add_command("cvar_in", "cvar_in <cvar1> <cvar2> [<options>]: cvar2's value put in cvar1",
                  [](Console& c, const Args& a) { put_in(c, a, false); });
  con.add_command("cvar_out", "cvar_out <cvar1> <cvar2> [<options>]: cvar2's value taken out",
                  [](Console& c, const Args& a) { take_out(c, a, false); });
  con.add_command("set_alias_str", "set_alias_str <cvar> <alias>: the cvar set to the alias's text",
                  [](Console& c, const Args& a) {
                    if (a.size() != 3) {
                      c.print("usage: set_alias_str <cvar> <alias>");
                      return;
                    }
                    const auto it = c.aliases().find(lower(a[2]));
                    if (!set_made(c, a, a[1], it == c.aliases().end() ? "" : it->second.body))
                      return;
                    if (it == c.aliases().end()) c.print("Unknown alias \"" + a[2] + "\"");
                  });
  con.add_command("set_bind_str", "set_bind_str <cvar> <key>: the cvar set to the key's bind",
                  [](Console& c, const Args& a) {
                    if (a.size() != 3) {
                      c.print("usage: set_bind_str <cvar> <key>");
                      return;
                    }
                    std::string key;
                    if (!normalize_key(a[2], key)) {
                      c.print("Unknown key \"" + a[2] + "\"");
                      return;
                    }
                    set_made(c, a, a[1], c.binds().get(BindMode::game, key));
                  });
  con.add_command("hash", "hash <text>: the text's hash, a whole number",
                  [](Console& c, const Args& a) {
                    if (a.size() != 2) {
                      c.print("Usage: hash <string to hash>");
                      return;
                    }
                    c.print(std::to_string(text_hash(a[1])));
                  });
}

}  // namespace ge::console
