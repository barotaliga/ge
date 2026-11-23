// Shooter's Edge's menu script (games/shooters_edge/menu/menu_script.hpp): a script's page
// replacing the game's page of its name and a new page added; every row word made the row it
// stands for (values by number, strings by words, pairs, a range either way, a toggle of one bit,
// a key row, an action opening a page, a field, a blank, a save slot); $ expansion when read and
// a quoted command kept for when it runs; `$$name` items as the setting's words; comments and
// joined lines; the pages the game draws itself left to it and SELECT GAME kept first; each line
// the script cannot use told with what was done; and the game's menu reading the script off its
// files, or not with `menu_script 0`.
//
// ge:summary Tests the menu script: rows, pages over the game's, expansion, warnings, the files
// ge:keywords test menu-script q2pro-menu pages rows include warnings menu_script
// ge:skill ui testing
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "shooters_edge/menu/game_menu.hpp"
#include "shooters_edge/menu/main_menu.hpp"
#include "shooters_edge/menu/menu_script.hpp"
#include "vfs/pak.hpp"
#include "vfs/search.hpp"

namespace sm = shooters_edge::menu;
using ge::ui::ItemKind;

namespace {

const sm::Entry* row(const sm::MenuTable& t, std::string_view page, std::string_view id) {
  return sm::find_entry(page, id, t);
}

std::vector<const sm::Entry*> rows_of(const sm::MenuTable& t, std::string_view page) {
  std::vector<const sm::Entry*> out;
  for (const sm::Entry& e : t.entries)
    if (e.page == page) out.push_back(&e);
  return out;
}

bool has_page(const sm::MenuTable& t, std::string_view page) {
  for (const sm::PageTitle& p : t.pages)
    if (p.id == page) return true;
  return false;
}

std::string_view title_of(const sm::MenuTable& t, std::string_view page) {
  for (const sm::PageTitle& p : t.pages)
    if (p.id == page) return p.title;
  return "?";
}

bool warned(const sm::ScriptMenu& s, std::string_view words) {
  for (const std::string& w : s.warnings())
    if (w.find(words) != std::string::npos) return true;
  return false;
}

// A script read over Quake II's table with `vars` for its $ names and `files` to include.
void read(sm::ScriptMenu& s, std::string_view text,
          const std::map<std::string, std::string>& vars = {},
          const std::map<std::string, std::string>& files = {}) {
  s.read(
      "q2pro.menu", text, sm::q2_menu(),
      [files](std::string_view name) -> std::optional<std::string> {
        const auto it = files.find(std::string(name));
        if (it == files.end()) return std::nullopt;
        return it->second;
      },
      [vars](std::string_view name, std::string& value) {
        const auto it = vars.find(std::string(name));
        if (it == vars.end()) return false;
        value = it->second;
        return true;
      });
}

}  // namespace

TEST(a_script_page_replaces_ours_and_a_new_page_is_added) {
  sm::ScriptMenu s;
  read(s, R"(
begin sound
  title "My Sound"
  values "sound engine" s_enable "no sound" software
  strings "colour" cl_railcore_color red green blue
  pairs "ambient" s_ambient no 0 yes 1
  range "volume" s_volume 0 1
  range "texture quality" gl_picmip 3 0 1
  toggle "allow health" dmflags ~0
  toggle "no powerups" dmflags 1
  toggle "swap stereo" s_swapstereo ~
  bind "fire" +attack
  action "railgun trail" pushmenu railtrail
  action "my page" pushmenu mine
  field "name" name
  field -c -w 32 adr0
  blank
  savegame save3
end
begin mine
  action -a "back" popmenu
end
)");
  CHECK_MSG(s.warnings().empty(), s.warnings().empty() ? "" : s.warnings()[0]);
  CHECK(s.pages() == 2);
  const sm::MenuTable& t = s.table();
  CHECK(title_of(t, "sound") == "My Sound");
  CHECK(has_page(t, "mine") && has_page(t, "video") && has_page(t, "enhanced"));
  CHECK(rows_of(t, "sound").size() == 15);  // the script's rows only, none of ours
  if (rows_of(t, "sound").size() != 15) return;
  const sm::Entry* e = row(t, "sound", "s_enable");
  CHECK(e != nullptr && e->kind == ItemKind::choice && e->choices.size() == 2 &&
        e->choices[1].value == "1" && e->target == sm::Target::client && e->needs == "s_enable");
  e = row(t, "sound", "cl_railcore_color");
  CHECK(e != nullptr && e->choices[2].text == "blue" && e->choices[2].value == "blue");
  e = row(t, "sound", "s_ambient");
  CHECK(e != nullptr && e->choices.size() == 2 && e->choices[1].text == "yes" &&
        e->choices[1].value == "1");
  e = row(t, "sound", "s_volume");
  CHECK(e != nullptr && e->kind == ItemKind::slider && e->min == 0 && e->max == 1 &&
        e->step == 0.1 && !e->reversed);
  e = row(t, "sound", "gl_picmip");
  CHECK(e != nullptr && e->reversed && e->min == 0 && e->max == 3 && e->step == 1);
  // One bit of a number the game's table sets on the server: set there.
  e = row(t, "sound", "dmflags.0");
  CHECK(e != nullptr && e->bit == 0 && e->inverted && e->needs == "dmflags" &&
        e->target == sm::Target::server);
  e = row(t, "sound", "dmflags.1");
  CHECK(e != nullptr && e->bit == 1 && !e->inverted);
  e = row(t, "sound", "s_swapstereo");
  CHECK(e != nullptr && e->kind == ItemKind::toggle && e->inverted && e->bit < 0);
  const std::vector<const sm::Entry*> r = rows_of(t, "sound");
  CHECK(r[8]->bind == "+attack" && r[8]->kind == ItemKind::choice);
  CHECK(r[9]->opens == "railtrail" && r[9]->line.empty());
  CHECK(r[10]->opens == "mine");
  e = row(t, "sound", "name");
  CHECK(e != nullptr && e->field && e->line == "toggleconsole" && e->needs == "name");
  e = row(t, "sound", "adr0");
  CHECK(e != nullptr && e->field && e->text == "adr0");
  CHECK(r[13]->kind == ItemKind::label && r[13]->text.empty());
  CHECK(r[14]->line == "save save3" && r[14]->closes);
  CHECK(rows_of(t, "mine").size() == 1 && rows_of(t, "mine")[0]->line == "popmenu" &&
        rows_of(t, "mine")[0]->needs == "popmenu");
  // Pages the script did not touch keep ours, row for row.
  CHECK(rows_of(t, "video").size() == rows_of(sm::q2_menu(), "video").size());
  CHECK(&sm::game_table(t) == &sm::q2_menu());
  // The table builds: every row a stop or a label on its page.
  const ge::ui::Menu m = sm::build_menu(t);
  CHECK(m.page("mine") != nullptr && m.page("sound")->items.size() == 15);
}

TEST(a_script_expands_names_when_read_and_keeps_quoted_commands) {
  sm::ScriptMenu s;
  read(s, R"(/* the video page
   over two lines */
begin video // ours replaced
  values "mode" vid_fullscreen windowed $$vid_modelist
  strings "map" sv_map $$nothing start
  strings "dollar" x $$$cash
  action "go" map $mymap
  action "later" "map $mymap"
  action "two" echo one; \
    echo two
end
)",
       {{"vid_modelist", "640x480 1920x1080"}, {"mymap", "base1"}});
  CHECK_MSG(s.warnings().empty(), s.warnings().empty() ? "" : s.warnings()[0]);
  const sm::MenuTable& t = s.table();
  const sm::Entry* e = row(t, "video", "vid_fullscreen");
  CHECK(e != nullptr && e->choices.size() == 3 && e->choices[2].text == "1920x1080" &&
        e->choices[2].value == "2");
  e = row(t, "video", "sv_map");
  CHECK(e != nullptr && e->choices.size() == 1 && e->choices[0].text == "start");
  e = row(t, "video", "x");
  CHECK(e != nullptr && e->choices.size() == 1 && e->choices[0].text == "$cash");
  const std::vector<const sm::Entry*> r = rows_of(t, "video");
  CHECK(r.size() == 6);
  CHECK(r[3]->line == "map base1");
  CHECK(r[4]->line == "map $mymap");  // expanded when it runs
  CHECK(r[5]->line == "echo one; echo two");
}

TEST(the_pages_the_game_draws_stay_its_own_and_select_game_stays_first) {
  sm::ScriptMenu s;
  read(s, R"(
begin players
  action "nothing" echo
end
begin loadgame
  loadgame save0
end
begin main
  bitmap m_main_game pushmenu singleplayer
  bitmap -N m_main_quit_sel m_main_quit quit
end
)");
  CHECK(s.warnings().empty());
  CHECK(s.pages() == 1);  // only the root: player setup and LOAD are the game's
  const sm::MenuTable& t = s.table();
  const std::vector<const sm::Entry*> root = rows_of(t, "main");
  CHECK(root.size() == 3);
  CHECK(root[0]->id == "games");
  CHECK(root[1]->text == "game" && root[1]->opens == "single");
  CHECK(root[2]->text == "quit" && root[2]->line == "quit");
  CHECK(rows_of(t, "setup").size() == rows_of(sm::q2_menu(), "setup").size());
  CHECK(row(t, "setup", "nothing") == nullptr);
}

TEST(lines_the_script_cannot_use_are_told_with_what_was_done) {
  sm::ScriptMenu s;
  read(s, R"(
color selection #ff0000
color shiny red
begin sound
  wobble a b
  toggle "x" s_x 40
  range "v" s_v 1
  values "v" s_w
  pairs "p" s_p a
  field -w 40 "f" s_f
  action -z "a" echo
  include nothing.menu
  toggle "kept" s_kept
end
begin empty
end
include deep.menu
frobnicate
begin never
  toggle "never" s_never
end
)",
       {}, {{"deep.menu", "include deep.menu\n"}});
  CHECK(warned(s, "q2pro.menu line 3: 'shiny' is not a state"));
  CHECK(warned(s, "line 5: 'wobble' is not a menu script word, so the line is skipped."));
  CHECK(warned(s, "a toggle's bit is 0 to 31, not 40"));
  CHECK(warned(s, "range is written range <name> <setting> <min> <max> [step]"));
  CHECK(warned(s, "values is written"));
  CHECK(warned(s, "pairs is written"));
  CHECK(warned(s, "a field's width is 1 to 32"));
  CHECK(warned(s, "-z is not one of action's options, so the row is left out."));
  CHECK(warned(s, "'include' is not a menu script word, so the line is skipped."));
  CHECK(warned(s, "the page empty has no rows, so it is left out."));
  CHECK(warned(s, "includes go too deep"));
  CHECK(warned(s, "'frobnicate' is not a menu script word, so the rest of the file is not read."));
  for (const std::string& w : s.warnings()) {
    CHECK_MSG(w.find('`') == std::string::npos && w.find("next:") == std::string::npos, w);
  }
  const sm::MenuTable& t = s.table();
  CHECK(rows_of(t, "sound").size() == 1 && row(t, "sound", "s_kept") != nullptr);
  CHECK(!has_page(t, "empty") && !has_page(t, "never"));

  sm::ScriptMenu open;
  read(open, "begin mine\n  toggle \"a\" s_a\n");
  CHECK(warned(open, "the page mine has no end, so it is left out."));
  CHECK(!has_page(open.table(), "mine"));
  sm::ScriptMenu quote;
  read(quote, "begin mine\n  action \"a echo\nend\n");
  CHECK(warned(quote, "line 2: a quote is not closed, so the line is skipped."));
}

TEST(a_later_page_of_a_name_replaces_the_earlier_and_includes_are_read) {
  sm::ScriptMenu s;
  read(s, "include more.menu\nbegin mine\n  toggle \"second\" s_two\nend\n", {},
       {{"more.menu", "begin mine\n  toggle \"first\" s_one\nend\n"}});
  CHECK(s.warnings().empty());
  CHECK(row(s.table(), "mine", "s_one") == nullptr && row(s.table(), "mine", "s_two") != nullptr);
}

TEST(the_game_menu_reads_the_script_off_its_files_unless_told_not_to) {
  const std::string script =
      "begin sound\n  values \"engine\" s_enable off on\n  field \"server\" adr5\nend\n"
      "begin mine\n  toggle \"thing\" s_thing\nend\n";
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files = {
      {"q2pro.menu", std::vector<uint8_t>(script.begin(), script.end())}};
  std::vector<uint8_t> pak;
  std::string error;
  CHECK_MSG(ge::vfs::write_pak(files, &pak, &error), error);
  ge::vfs::SearchPath sp;
  CHECK_MSG(ge::vfs::add_archive(&sp, "pak0.pak", pak, "baseq2", &error), error);

  ge::console::Console con;
  std::vector<std::string> ran;
  for (const char* c : {"sv", "toggleconsole", "s_enable", "s_thing"})
    con.add_command(c, "", [&ran, c](ge::console::Console&, const ge::console::Args& a) {
      ran.push_back(std::string(c) + (a.size() > 1 ? " " + a.rest(0) : ""));
    });
  con.cvars().add(
      {.name = "adr5", .fallback = "10.0.0.5:27910", .kind = ge::console::CvarKind::text});
  sm::GameMenu g;
  g.add_cvars(con);
  const sm::MenuTable& t = g.set_table(sm::q2_menu(), &sp);
  CHECK(&t != &sm::q2_menu() && has_page(t, "mine") && g.main().page("mine") != nullptr);
  sm::Host host{.console = &con, .table = &t};
  g.show(host, "sound");
  const ge::ui::Item* field = g.main().page("sound")->find("adr5");
  CHECK_MSG(field != nullptr && field->text == "server  10.0.0.5:27910",
            field != nullptr ? field->text + (field->disabled ? " dimmed" : "") : "none");
  // A values row writes the choice's number.
  g.main().page("sound")->find("s_enable")->choice = 1;
  sm::apply_event({.kind = ge::ui::EventKind::changed, .page = "sound", .item = "s_enable"},
                  g.main(), host);
  CHECK(!ran.empty() && ran.back() == "s_enable 1");

  con.execute("menu_script 0");
  con.run();
  CHECK(&g.set_table(sm::q2_menu(), &sp) == &sm::q2_menu());
  CHECK(g.main().page("mine") == nullptr);
  // Quake's table names no script: its own pages, script or not.
  con.execute("menu_script 1");
  con.run();
  CHECK(&g.set_table(sm::classic_menu(), &sp) == &sm::classic_menu());
}
