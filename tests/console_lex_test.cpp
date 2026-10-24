// The console language's text: splitting commands, words, $ expansion and alias arguments.
//
// ge:summary Tests the console lexer: ; and lines, quotes, comments, $ expansion, $1..$9 and $*
// ge:keywords test console lexer split_commands tokenize expand substitute_args quote
// ge:skill console testing
#include "check.hpp"
#include "console/lex.hpp"

using namespace ge::console;

TEST(commands_split_on_semicolons_and_lines_outside_quotes) {
  const auto l = split_commands("a 1; b \"x;y\" // c; d\n\n  e  \r\nf // only a comment\n// x\n");
  CHECK(l.size() == 4);
  CHECK(l[0].text == "a 1" && l[0].line == 1);
  CHECK(l[1].text == "b \"x;y\"" && l[1].line == 1);
  CHECK(l[2].text == "e" && l[2].line == 3);
  CHECK(l[3].text == "f" && l[3].line == 4);
  // A // inside quotes is kept (a URL).
  const auto u = split_commands("echo \"http://x\"");
  CHECK(u.size() == 1 && u[0].text == "echo \"http://x\"");
}

TEST(words_and_quotes) {
  const auto w = tokenize("bind  space \"+jump; say hi\" x");
  CHECK(w.size() == 4);
  CHECK(w[2] == "+jump; say hi");
  const auto e = tokenize("set name \"\"");
  CHECK(e.size() == 3 && e[2].empty());
  CHECK(tokenize("echo \"open").back() == "open");
  CHECK(rest_after("alias a \"b c\" d", 2) == "\"b c\" d");
}

TEST(expansion) {
  auto lookup = [](std::string_view n, std::string& out) {
    if (n == "a") out = "1";
    if (n == "b.c") out = "two";
    return n == "a" || n == "b.c";
  };
  CHECK(expand("x $a ${a}b $b.c $nope $$a $", lookup) == "x 1 1b two $nope $a $");
  CHECK(expand("$1 ${ $", lookup) == "$1 ${ $");
}

TEST(alias_arguments) {
  CHECK(substitute_args("say $1 to $2; echo $*", {"greet", "hi", "two words"}) ==
        "say hi to two words; echo hi \"two words\"");
  CHECK(substitute_args("echo $3 $$x", {"a"}) == "echo  $$x");
  CHECK(quote("a b") == "\"a b\"" && quote("ab") == "ab" && quote("") == "\"\"");
}
