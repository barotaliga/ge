// Tests: a cfg checked and tidied without running it (engine/console/cfg_tidy.hpp): unknown
// settings and commands, values that fit none, a setting set twice; the plain, commented and
// pretty writers, a formatted cfg formatted again unchanged, and the commands kept as written.
//
// ge:summary Tests: the cfg check's findings and the formatter's plain, commented and pretty output
// ge:keywords test cfg-check cfg-format check_cfg format_cfg pretty comments
// ge:skill console testing
#include <string>

#include "check.hpp"
#include "console/cfg_tidy.hpp"
#include "console/console.hpp"

namespace con = ge::console;

namespace {

void settings(con::Console& c) {
  c.cvars().add({.name = "sv_maxclients",
                 .fallback = "8",
                 .help = "The most players the server seats.",
                 .kind = con::CvarKind::integer,
                 .min = 1,
                 .max = 32,
                 .values = "1..32: that many (8 the default)"});
  c.cvars().add({.name = "r_look",
                 .fallback = "modern",
                 .help = "The picture's look.",
                 .kind = con::CvarKind::text,
                 .values = "classic: crisp; modern: filtered (the default)"});
  c.cvars().add({.name = "sensitivity",
                 .fallback = "3",
                 .help = "The mouse's turn.",
                 .min = 0.1,
                 .max = 20,
                 .values = "0.1..20: that much (3 the default)"});
}

}  // namespace

TEST(the_check_finds_unknown_settings_bad_values_and_repeats) {
  con::Console c{false};
  settings(c);
  const std::string text =
      "sv_maxclients 40\n"
      "r_look classik\n"
      "sensitvity 4\n"
      "set sv_maxclent 4\n"
      "alias +go \"forward\"\n"
      "+go\n"
      "r_look classic // the crisp one\n"
      "sensitivity $foo\n";
  const auto f = con::check_cfg(c, text);
  CHECK(f.size() == 5);
  CHECK_MSG(f[0].line == 1 &&
                f[0].what.find("\"40\", which is not one of its values") != std::string::npos,
            f[0].what);
  CHECK_MSG(f[0].next.find("1..32") != std::string::npos, f[0].next);
  CHECK(f[1].line == 2 && f[1].what.find("classik") != std::string::npos);
  CHECK(f[2].line == 2 && f[2].what.find("set again on line 7") != std::string::npos);
  CHECK_MSG(f[3].line == 3 && f[3].next.find("did you mean 'sensitivity'") != std::string::npos,
            f[3].next);
  CHECK(f[4].line == 4 && f[4].what.find("'sv_maxclent' is no setting") != std::string::npos);
}

TEST(the_plain_and_commented_formats_keep_the_commands) {
  con::Console c{false};
  settings(c);
  const std::string text = "// mine\nr_look   classic;sensitivity 4 // fast\n\n  exec more\n";
  const std::string plain = con::format_cfg(c, text, {});
  CHECK_MSG(plain == "// mine\nr_look \"classic\"\nsensitivity \"4\" // fast\n\nexec more\n",
            plain);
  const std::string commented = con::format_cfg(c, text, {.comments = true});
  CHECK_MSG(commented ==
                "// mine\n// The picture's look.\nr_look \"classic\"\n// The mouse's turn.\n"
                "sensitivity \"4\" // fast\n\nexec more\n",
            commented);
  CHECK(con::format_cfg(c, commented, {.comments = true}) == commented);  // no second comment
  CHECK(con::check_cfg(c, commented).empty());
}

TEST(the_pretty_format_sorts_into_sections_and_aligns) {
  con::Console c{false};
  settings(c);
  const std::string text =
      "bind w +forward\nsensitivity 4\nbind a +moveleft\nr_look classic\nsv_maxclients 16\n"
      "exec last\n";
  const std::string pretty = con::format_cfg(c, text, {.pretty = true});
  const size_t server = pretty.find("// Server"), mouse = pretty.find("// Mouse and keys"),
               picture = pretty.find("// Picture"), binds = pretty.find("// Binds"),
               commands = pretty.find("// Commands");
  CHECK_MSG(server < mouse && mouse < picture && picture < binds && binds < commands, pretty);
  CHECK(pretty.find("bind a +moveleft\nbind w +forward") != std::string::npos);
  CHECK(pretty.find("// The most players the server seats.\nsv_maxclients \"16\"") !=
        std::string::npos);
  CHECK(pretty.rfind("exec last\n") == pretty.size() - 10);
  CHECK(con::format_cfg(c, pretty, {.pretty = true}) == pretty);
  CHECK(con::check_cfg(c, pretty).empty());
}
