// The ge command catalog and its answer (engine/cli/commands.hpp, outcome.hpp, json.hpp): every
// command has its summary, stage, example and skill; the parser matches two-word commands and
// refuses what the catalog does not allow; `ge help --json` lists every command; an outcome's
// JSON carries every field and its exit code follows its findings.
//
// ge:summary Tests: the ge catalog is complete, the parser follows it, help and --json answers
// ge:keywords test cli ge commands catalog help json outcome parse_args check_args emit
// ge:skill agent-interface testing
// ge:test tests/cli_commands_test.cpp
#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "cli/commands.hpp"
#include "cli/json.hpp"
#include "cli/outcome.hpp"
#include "cli/skills.hpp"

using namespace ge::cli;

namespace {

Args parse(std::vector<const char*> words) {
  words.insert(words.begin(), "ge");
  return parse_args(static_cast<int>(words.size()), words.data());
}

}  // namespace

TEST(every_command_has_its_help_and_a_consumer_skill) {
  std::vector<std::string> skills;
  for (const ConsumerSkill& s : consumer_skills()) skills.push_back(s.name);
  for (const CommandSpec& c : commands()) {
    const std::string name(c.name);
    CHECK_MSG(c.summary.size() > 20, "ge " + name + ": a summary of what it does");
    CHECK_MSG(!c.stage.empty(), "ge " + name + ": the stage of the loop it serves");
    CHECK_MSG(c.example.starts_with("ge " + name),
              "ge " + name + ": its example must run it; next: fix kCommands in commands.cpp");
    CHECK_MSG(std::find(skills.begin(), skills.end(), std::string(c.skill)) != skills.end(),
              "ge " + name + " names skill \"" + std::string(c.skill) +
                  "\", which is not in consumer-skills/");
    CHECK_MSG(help_text(&c).find("example: " + std::string(c.example)) != std::string::npos,
              "ge help " + name + " leaves out its example");
    CHECK_MSG(help_text(nullptr).find("ge " + name) != std::string::npos,
              "ge help leaves out ge " + name);
    for (const FlagSpec& f : c.flags) {
      CHECK_MSG(f.help.size() > 8, "ge " + name + " --" + std::string(f.name) + ": its help");
    }
  }
  CHECK(find_command("skills sync") != nullptr);
  CHECK(find_command("add lab") != nullptr);
}

TEST(the_ge_skill_names_every_command) {
  std::string text;
  for (const ConsumerSkill& s : consumer_skills()) {
    if (s.name == "ge") text = std::string(s.text);
  }
  CHECK_MSG(!text.empty(), "consumer-skills/ge/SKILL.md is missing");
  for (const CommandSpec& c : commands()) {
    if (c.name == "help" || c.name == "version") continue;
    const std::string first(c.name.substr(0, c.name.find(' ')));
    CHECK_MSG(text.find("ge " + first) != std::string::npos,
              "the ge skill does not mention ge " + first + "; next: add it to its loop");
  }
}

TEST(the_parser_matches_commands_and_refuses_what_the_catalog_does_not_allow) {
  const Args lab = parse({"add", "lab", "wind", "--kind=scenario", "--json"});
  CHECK(lab.command && lab.command->name == "add lab");
  CHECK(lab.words.size() == 1 && lab.words[0] == "wind");
  CHECK(flag_value(lab, "kind") == "scenario" && has_flag(lab, "json"));
  CHECK(!check_args(lab).has_value());

  const Args diff = parse({"lab", "diff", "world", "--run", "--psnr=40,30", "--", "--views=vista"});
  CHECK(diff.command && diff.command->name == "lab diff");
  CHECK(diff.words.size() == 1 && diff.words[0] == "world" && has_flag(diff, "run"));
  CHECK(flag_value(diff, "psnr") == "40,30" && diff.passthrough.size() == 1);
  CHECK(!check_args(diff).has_value());
  CHECK(check_args(parse({"lab", "diff"})).has_value());  // which lab
  const Args pass = parse({"lab", "run", "--smoke", "--", "--seeds=7", "x"});
  CHECK(pass.command && pass.command->name == "lab");
  CHECK(pass.passthrough.size() == 2 && pass.passthrough[0] == "--seeds=7");
  CHECK(!check_args(pass).has_value());

  CHECK(check_args(parse({"build", "--bogus"})).value_or("").find("unknown flag --bogus") !=
        std::string::npos);
  CHECK(check_args(parse({"new"})).has_value());                 // a missing directory
  CHECK(check_args(parse({"build", "--preset"})).has_value());   // a value flag with no value
  CHECK(check_args(parse({"check", "--dry-run"})).has_value());  // a flag of another command
  CHECK(check_args(parse({"doctor", "--", "x"})).has_value());   // only ge lab passes words on
  CHECK(!parse({"frobnicate"}).command);
}

TEST(help_json_lists_every_command) {
  const std::string all = help_json(nullptr);
  CHECK(all.starts_with("{\"version\":"));
  for (const CommandSpec& c : commands()) {
    CHECK_MSG(all.find("\"name\":\"" + std::string(c.name) + "\"") != std::string::npos,
              "ge help --json leaves out " + std::string(c.name));
  }
  CHECK(help_json(find_command("add lab")).find("\"--kind\"") != std::string::npos);
}

TEST(an_outcome_prints_one_json_object_and_its_exit_code_follows_its_findings) {
  Outcome ok;
  ok.written = {"sim/wind.hpp"};
  ok.result = json_obj({{"n", json_num(2)}});
  ok.summary = "done";
  ok.next = "ge test";
  std::ostringstream out;
  CHECK(emit("ge add system", ok, true, out) == 0);
  CHECK(out.str() ==
        "{\"command\":\"ge add system\",\"ok\":true,\"exit\":0,\"findings\":[],"
        "\"written\":[\"sim/wind.hpp\"],\"result\":{\"n\":2},\"summary\":\"done\","
        "\"next\":\"ge test\"}\n");

  Outcome bad;
  bad.findings.push_back({Severity::error, "a \"quoted\"\nline", "a.cpp:3", "fix it"});
  bad.next = "ge test";
  std::ostringstream out2;
  CHECK(emit("ge build", bad, true, out2) == 1);
  CHECK(out2.str().find("\"what\":\"a \\\"quoted\\\"\\nline\"") != std::string::npos);
  CHECK(out2.str().find("\"next\":\"\"}") !=
        std::string::npos);  // a failure's next is in its findings
  CHECK(out2.str().find("\"result\":null") != std::string::npos);

  std::ostringstream text;
  CHECK(emit("ge build", bad, false, text) == 1);
  CHECK(text.str().find("ge build: error: a \"quoted\"") != std::string::npos);
  CHECK(text.str().find("next: ge test") == std::string::npos);

  Outcome usage;
  usage.findings.push_back({Severity::error, "unknown flag", "the command line", "ge help"});
  usage.exit = 2;
  std::ostringstream out3;
  CHECK(emit("ge", usage, true, out3) == 2);
}

TEST(json_strings_escape_control_bytes) {
  CHECK(json_str("a\\b\t\x01") == "\"a\\\\b\\t\\u0001\"");
  CHECK(json_strs({"x", "y"}) == "[\"x\",\"y\"]");
  CHECK(json_arr({}) == "[]");
}
