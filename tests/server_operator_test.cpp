// engine/server/operator.hpp: a player picked by a slot or a name (exact, then in any case or by a
// unique part when partial names are on) with each refusal in the server's words; a line's raw
// rest; the stuff lists' words and their answers.
//
// ge:summary Test: picking a player, a line's raw rest, the connect and begin stuff lists
// ge:keywords test operator pick_player sv_enhanced_setplayer line_rest addstuffcmd delstuffcmd
// ge:keywords liststuffcmds stuff-lists
// ge:skill net
#include <string>
#include <vector>

#include "check.hpp"
#include "server/operator.hpp"

namespace gs = ge::server;

TEST(a_player_is_a_slot_an_exact_name_or_with_partial_names_a_unique_part) {
  const std::vector<gs::PickSlot> slots = {
      {"Ranger", true}, {"", false}, {"grunt", true}, {"Rangeress", true}, {"old", false}};
  std::string error;
  CHECK(gs::pick_player(slots, "2", false, error) == 2 && error.empty());
  CHECK(gs::pick_player(slots, "9", false, error) == -1 && error == "Bad client slot number: 9\n");
  CHECK(gs::pick_player(slots, "1", false, error) == -1 &&
        error == "Client slot 1 is not active.\n");
  CHECK(gs::pick_player(slots, "Ranger", false, error) == 0);
  CHECK(gs::pick_player(slots, "ranger", false, error) == -1 &&
        error == "Userid 'ranger' is not on the server.\n");
  CHECK(gs::pick_player(slots, "ranger", true, error) == 0);  // equal but for case: at once
  CHECK(gs::pick_player(slots, "GRU", true, error) == 2);
  CHECK(gs::pick_player(slots, "ang", true, error) == -1 &&
        error == "'ang' matches multiple clients.\n");
  CHECK(gs::pick_player(slots, "old", true, error) == -1 &&
        error == "No clients matching 'old' found.\n");
  CHECK(gs::pick_player(slots, "", true, error) == -1 && error.empty());
}

TEST(a_lines_raw_rest_keeps_its_spacing_and_loses_one_pair_of_quotes) {
  CHECK(gs::line_rest("stuff 3 echo   two  words  ", 2) == "echo   two  words");
  CHECK(gs::line_rest("stuffall \"say hi; wave\"", 1) == "say hi; wave");
  CHECK(gs::line_rest("printall \"a\" b \"c\"", 1) == "a\" b \"c");
  CHECK(gs::line_rest("stuff 3", 2).empty());
}

TEST(the_stuff_lists_take_lines_once_and_lose_them_by_number_text_or_all) {
  gs::StuffLists l;
  std::string out;
  CHECK(!gs::stuff_list_command(l, "status", out));
  CHECK(gs::stuff_list_command(l, "addstuffcmd begin", out) &&
        out == "Usage: addstuffcmd <list> <command>\n");
  CHECK(gs::stuff_list_command(l, "addstuffcmd spawn echo x", out) &&
        out == "Unknown stuffcmd list: spawn\n");
  CHECK(gs::stuff_list_command(l, "liststuffcmds begin", out) &&
        out == "No stuffcmds registered.\n");
  CHECK(gs::stuff_list_command(l, "addstuffcmd begin echo one", out) && out.empty());
  CHECK(gs::stuff_list_command(l, "addstuffcmd begin echo two", out) && out.empty());
  CHECK(gs::stuff_list_command(l, "addstuffcmd begin echo one", out) &&
        out == "Stuffcmd already exists: echo one\n");
  CHECK(gs::stuff_list_command(l, "liststuffcmds begin", out) &&
        out == "id command\n-- -------\n1  echo one\n2  echo two\n");
  CHECK(gs::stuff_list_command(l, "delstuffcmd begin 3", out) &&
        out == "No such stuffcmd index: 3\n");
  CHECK(gs::stuff_list_command(l, "delstuffcmd begin echo three", out) &&
        out == "No such stuffcmd string: echo three\n");
  CHECK(gs::stuff_list_command(l, "delstuffcmd begin 1", out) && out.empty());
  CHECK(l.begin.size() == 1 && l.begin[0] == "echo two" && l.connect.empty());
  CHECK(gs::stuff_list_command(l, "delstuffcmd begin all", out) && l.begin.empty());
  CHECK(gs::stuff_list_command(l, "liststuffcmds", out) && out == "Usage: liststuffcmds <list>\n");
}
