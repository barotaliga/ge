// The reference manual: a console described in passes, the source's text read back, the passes
// merged into rows (a game's own default and meaning, its spellings, the games a name is in) and
// the page written with its data.
//
// ge:summary Tests for the reference manual: describe, the text, the merge and the page
// ge:keywords manual reference-manual describe rows merge page html test
// ge:skill console testing
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "check.hpp"
#include "console/console.hpp"
#include "console/manual.hpp"
#include "console/manual_page.hpp"

using namespace ge::console;

namespace {

// A console as a window registers one: an engine setting, a game layer meeting it (`same` in
// q2, `own` in q1 when `own`), a game's spelling and its own setting, commands and a flag.
struct Rig {
  Console con{false};
  double gamma = 1, fov = 90;
  Rig() {
    con.cvars().bind({.name = "gamma",
                      .fallback = "1",
                      .flags = kArchive,
                      .help = "the picture's gamma",
                      .min = 0.5,
                      .max = 3},
                     &gamma);
    con.cvars().bind(
        {.name = "fov", .fallback = "90", .flags = kArchive, .help = "the view's width in degrees"},
        &fov);
    con.add_command("screenshot", "screenshot: the picture kept", [](Console&, const Args&) {});
    con.set_manual({.program = {"se", "Shooter's Edge", {}},
                    .games = {{"q1", "Quake", {}}, {"q2", "Quake II", {}}},
                    .pass = "",
                    .flags = {{"-game", "<folder>", "Play a mod."}}});
  }
  // The game's own layer, as a game switch loads it.
  void load(std::string_view game) {
    if (game == "q2") {
      CvarLayer layer(con.cvars(), "game:q2");
      con.cvars().add({.name = "gamma",
                       .fallback = "1.2",
                       .flags = kArchive,
                       .help = "the picture's gamma",
                       .min = 0.5,
                       .max = 3,
                       .overlap = Overlap::same});
      con.cvars().add_synonym("vid_gamma", "gamma");
      con.cvars().add({.name = "cl_gibs", .fallback = "1", .help = "gibs thrown"});
      con.add_command_synonym("sshot", "screenshot");
    } else {
      CvarLayer layer(con.cvars(), "game:q1");
      con.cvars().add({.name = "fov",
                       .fallback = "90",
                       .flags = kArchive,
                       .help = "the field of view, Quake's way",
                       .overlap = Overlap::own});
    }
  }
  std::vector<ManualEntry> pass(std::string_view game) {
    load(game);
    std::vector<ManualEntry> out = describe(con, "se", game);
    con.drop_layer("game:" + std::string(game));
    return out;
  }
};

const ManualEntry* find(const std::vector<ManualEntry>& v, ManualKind k, std::string_view name) {
  for (const ManualEntry& e : v)
    if (e.kind == k && e.name == name) return &e;
  return nullptr;
}

const ManualRow* row(const std::vector<ManualRow>& v, ManualKind k, std::string_view name) {
  for (const ManualRow& r : v)
    if (r.kind == k && r.name == name) return &r;
  return nullptr;
}

ManualSource both_passes(Rig& r) {
  ManualSource s;
  s.programs = {r.con.manual().program};
  s.games = r.con.manual().games;
  for (const char* g : {"q1", "q2"}) {
    std::vector<ManualEntry> p = r.pass(g);
    s.entries.insert(s.entries.end(), p.begin(), p.end());
  }
  return s;
}

}  // namespace

TEST(a_pass_lists_settings_spellings_commands_and_flags_with_their_layers) {
  Rig r;
  const std::vector<ManualEntry> q2 = r.pass("q2");
  const ManualEntry* gamma = find(q2, ManualKind::setting, "gamma");
  CHECK(gamma != nullptr);
  if (gamma) {
    CHECK(gamma->layer == "game:q2");
    CHECK(gamma->overlap == "same:engine");
    CHECK(gamma->fallback == "1.2");
    CHECK(gamma->min == "0.5");
    CHECK(gamma->flags.front() == 'A');
    CHECK(gamma->games == "q2");
  }
  const ManualEntry* spelled = find(q2, ManualKind::setting_synonym, "vid_gamma");
  CHECK(spelled != nullptr && spelled->target == "gamma" && spelled->layer == "game:q2");
  const ManualEntry* shot = find(q2, ManualKind::command, "screenshot");
  CHECK(shot != nullptr && shot->layer == "engine" && shot->help == "screenshot: the picture kept");
  const ManualEntry* sshot = find(q2, ManualKind::command_synonym, "sshot");
  CHECK(sshot != nullptr && sshot->target == "screenshot");
  const ManualEntry* flag = find(q2, ManualKind::flag, "-game");
  CHECK(flag != nullptr && flag->target == "<folder>");
  // The other game's own is not in this pass; the built-in commands are, in the engine's layer.
  CHECK(find(q2, ManualKind::setting, "cl_gibs") != nullptr);
  CHECK(find(r.pass("q1"), ManualKind::setting, "cl_gibs") == nullptr);
  CHECK(find(q2, ManualKind::command, "writemanual") != nullptr);
}

// Settings outside the console (ManualInfo::more: a game's rules) join the pass with its program
// and games; a name the console has stays the console's.
TEST(settings_outside_the_console_join_the_pass) {
  Console con(true);
  con.cvars().add({.name = "hostname", .fallback = "a", .help = "the console's"});
  ManualInfo info;
  info.program = {"sv", "Server", {}};
  info.pass = "q2";
  info.more = [](std::string_view games, std::vector<ManualEntry>& out) {
    Cvars rules(true);
    rules.add({.name = "deathmatch", .fallback = "0", .help = "the match's rules"});
    rules.add({.name = "hostname", .fallback = "b", .help = "the rules'"});
    for (ManualEntry& e : describe_settings(rules, "game:" + std::string(games))) out.push_back(e);
  };
  con.set_manual(info);
  const std::vector<ManualEntry> pass = describe(con, "sv", "q2");
  const ManualEntry* dm = find(pass, ManualKind::setting, "deathmatch");
  CHECK(dm != nullptr && dm->layer == "game:q2" && dm->program == "sv" && dm->games == "q2");
  int hostnames = 0;
  for (const ManualEntry& e : pass)
    if (e.name == "hostname") hostnames += e.help == "the console's" ? 1 : 100;
  CHECK(hostnames == 1);
}

TEST(the_source_text_reads_back_field_for_field) {
  Rig r;
  const ManualSource s = both_passes(r);
  ManualSource back;
  std::string error;
  CHECK(read_manual(manual_text(s), back, error));
  CHECK(error.empty());
  CHECK(back.entries.size() == s.entries.size());
  CHECK(back.games.size() == 2 && back.games[1].name == "Quake II");
  CHECK(back.programs.size() == 1 && back.programs[0].name == "Shooter's Edge");
  bool same = back.entries.size() == s.entries.size();
  for (size_t i = 0; same && i < s.entries.size(); ++i) {
    const ManualEntry &a = s.entries[i], &b = back.entries[i];
    same = a.kind == b.kind && a.name == b.name && a.games == b.games && a.layer == b.layer &&
           a.fallback == b.fallback && a.help == b.help && a.overlap == b.overlap &&
           a.target == b.target && a.min == b.min && a.flags == b.flags;
  }
  CHECK(same);
}

TEST(a_game_line_keeps_the_games_its_filter_takes_in) {
  ManualSource s;
  s.games = {{"qw", "QuakeWorld", {"q1"}}};
  ManualSource back;
  std::string error;
  CHECK(read_manual(manual_text(s), back, error));
  CHECK(back.games.size() == 1 && back.games[0].takes_in.size() == 1 &&
        back.games[0].takes_in[0] == "q1" && back.games[0].name == "QuakeWorld");
}

TEST(a_damaged_line_is_refused_with_its_number) {
  ManualSource s;
  std::string error;
  CHECK(!read_manual("# manual 1\nsetting\tse\n", s, error));
  CHECK(error.find("line 2") != std::string::npos);
  CHECK(!read_manual("setting\tse\t\tx\t\t\t\t\t\t\t\t\t\t\n", s, error));
  CHECK(error.find("before") != std::string::npos);
  CHECK(!read_manual("# manual 2\n", s, error));
}

TEST(rows_keep_the_common_definition_and_each_games_own) {
  Rig r;
  const std::vector<ManualRow> rows = manual_rows(both_passes(r));
  const ManualRow* gamma = row(rows, ManualKind::setting, "gamma");
  CHECK(gamma != nullptr);
  if (gamma) {
    CHECK(gamma->layer == "engine");
    CHECK(gamma->fallback == "1");
    CHECK(gamma->games.empty());  // every game has it
    CHECK(gamma->own.size() == 1 && gamma->own[0].overlap == "same" &&
          gamma->own[0].fallback == "1.2" && gamma->own[0].games == std::vector<std::string>{"q2"});
    CHECK(gamma->spellings.size() == 1 && gamma->spellings[0].name == "vid_gamma" &&
          gamma->spellings[0].games == std::vector<std::string>{"q2"});
  }
  const ManualRow* fov = row(rows, ManualKind::setting, "fov");
  CHECK(fov != nullptr && fov->own.size() == 1 && fov->own[0].overlap == "own" &&
        fov->own[0].help == "the field of view, Quake's way" &&
        fov->help.find("width") != std::string::npos);
  const ManualRow* gibs = row(rows, ManualKind::setting, "cl_gibs");
  CHECK(gibs != nullptr && gibs->games == std::vector<std::string>{"q2"} && gibs->own.empty());
  const ManualRow* shot = row(rows, ManualKind::command, "screenshot");
  CHECK(shot != nullptr && shot->spellings.size() == 1 && shot->spellings[0].name == "sshot");
  // A spelling is never a row of its own when its target has one.
  CHECK(row(rows, ManualKind::setting, "vid_gamma") == nullptr);
  CHECK(row(rows, ManualKind::flag, "-game") != nullptr);
}

// A word only games' own layers have, alike but for the layer (a server's `changelevel` in
// QuakeWorld's and in Quake II's), is one row of "a game's own" in both; told otherwise, each
// game keeps its own words.
TEST(a_word_alike_in_each_games_own_layer_is_one_row) {
  ManualSource s;
  std::string error;
  CHECK_MSG(read_manual("# manual 1\n# program sesv Dedicated server\n# game qw QuakeWorld\n"
                        "# game q2 Quake II\n"
                        "command\tsesv\tqw\tchangelevel\tgame:qw\t\t\t\t\t\t\t\tgo on\t\n"
                        "command\tsesv\tq2\tchangelevel\tgame:q2\t\t\t\t\t\t\t\tgo on\t\n"
                        "command\tsesv\tqw\tmap\tgame:qw\t\t\t\t\t\t\t\ta new match\t\n"
                        "command\tsesv\tq2\tmap\tgame:q2\t\t\t\t\t\t\t\ta new game\t\n",
                        s, error),
            error);
  const std::vector<ManualRow> rows = manual_rows(s);
  CHECK(rows.size() == 2);
  CHECK(rows[0].name == "changelevel" && rows[0].layer == "game" && rows[0].own.empty());
  CHECK(rows[1].name == "map" && rows[1].layer == "game:qw" && rows[1].own.size() == 1);
  CHECK(rows[1].own[0].help == "a new game");
}

TEST(the_page_carries_its_data_and_never_closes_its_script_early) {
  Rig r;
  r.con.cvars().add({.name = "con_note",
                     .fallback = "</script><b>",
                     .help = "a <tag> & more",
                     .kind = CvarKind::text});
  const ManualSource s = both_passes(r);
  const std::vector<ManualRow> rows = manual_rows(s);
  const ManualPage page{"Shooter's Edge manual", "Every setting.", "1.2.3"};
  const std::string html = manual_html(s, rows, page);
  CHECK(html.starts_with("<!doctype html>"));
  CHECK(html.find("<title>Shooter&#39;s Edge manual</title>") != std::string::npos ||
        html.find("<title>Shooter's Edge manual</title>") != std::string::npos);
  CHECK(html.find("\"n\":\"vid_gamma\"") != std::string::npos);
  CHECK(html.find("</script><b>") == std::string::npos);
  CHECK(html.find("\\u003c/script>\\u003cb>") != std::string::npos);
  const std::string json = manual_json(s, rows, page);
  CHECK(json.find("\"games\":[{\"id\":\"q1\"") != std::string::npos);
  CHECK(json.find("\"version\":\"1.2.3\"") != std::string::npos);
}

TEST(writemanual_writes_the_consoles_pass) {
  Rig r;
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "ge_console_manual_test" / "se.txt";
  std::filesystem::remove(path);
  r.con.run_now("writemanual \"" + path.generic_string() + "\"");
  std::ifstream f(path);
  std::stringstream text;
  text << f.rdbuf();
  ManualSource back;
  std::string error;
  CHECK(read_manual(text.str(), back, error));
  CHECK(find(back.entries, ManualKind::setting, "gamma") != nullptr);
  CHECK(find(back.entries, ManualKind::flag, "-game") != nullptr);
  r.con.run_now("writemanual");
  CHECK(!r.con.problems().empty());
  std::filesystem::remove_all(path.parent_path());
}
