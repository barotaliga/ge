// THE CHAT IGNORE LISTS: a player ignored by name, without case, by part of the name or by
// number, kept through a rename and gone when another player takes the place or a new level comes;
// team chat dropped only with ignore_mode 1; teams by their team now; opponents always or while a
// match runs; spectators while you play or always; the flood of one line said again; TV viewers.
//
// ge:summary Tests: the chat ignore lists and settings
// ge:keywords test chat-ignore ignore ignore_team ignore_opponents ignore_spec ignore_flood
// ge:skill console testing
#include "check.hpp"
#include "console/chat_ignore.hpp"
#include "console/console.hpp"

namespace con = ge::console;

namespace {

struct Room {
  con::Console c;
  con::ChatIgnore g;
  con::ChatRoster r;
  Room() {
    con::add_chat_ignore(c, g);
    g.roster = [this] { return r; };
    r.names[0] = "me";
    r.names[1] = "Bob";
    r.names[2] = "bobby";
    r.names[3] = "watcher";
    r.spectator[3] = true;
    r.teams[0] = r.teams[1] = "red";
    r.teams[2] = "blue";
    r.me = 0;
    r.teams_play = true;
  }
  void run(const char* line) {
    c.execute(line);
    c.run();
  }
  bool drops(con::ChatKind kind, int place, const std::string& text = "hi", double t = 0) {
    return con::chat_ignored(g, r, {kind, place, text}, t);
  }
};

}  // namespace

TEST(a_player_is_ignored_by_name_or_number_until_the_place_changes_hands) {
  Room m;
  m.run("ignore bob");  // without case: Bob, not bobby
  CHECK(m.drops(con::ChatKind::everyone, 1));
  CHECK(!m.drops(con::ChatKind::everyone, 2));
  CHECK(!m.drops(con::ChatKind::team, 1));  // team chat stays with ignore_mode 0
  m.run("ignore_mode 1");
  CHECK(m.drops(con::ChatKind::team, 1));
  m.run("ignore 3");  // the third place's player
  CHECK(m.drops(con::ChatKind::everyone, 2));
  m.r.names[2] = "robert";  // a rename keeps it
  CHECK(m.drops(con::ChatKind::everyone, 2));
  m.r.names[2] = "";  // left
  CHECK(!m.drops(con::ChatKind::none, -1));
  m.r.names[2] = "newcomer";  // another player on the place
  CHECK(!m.drops(con::ChatKind::everyone, 2));
  m.run("unignore Bob");
  CHECK(!m.drops(con::ChatKind::everyone, 1));
  m.run("ignore Bob");
  con::chat_new_level(m.g);
  CHECK(!m.drops(con::ChatKind::everyone, 1));
}

TEST(teams_opponents_and_spectators) {
  Room m;
  m.run("ignore_team green");  // no such team playing
  CHECK(m.g.teams.empty());
  m.run("ignore_team blue");
  CHECK(m.drops(con::ChatKind::everyone, 2));
  m.r.teams[2] = "red";  // judged by the team now
  CHECK(!m.drops(con::ChatKind::everyone, 2));
  m.run("unignoreall_team");
  m.r.teams[2] = "blue";
  m.run("ignore_opponents 2");
  m.r.match_running = false;
  CHECK(!m.drops(con::ChatKind::everyone, 2));
  m.r.match_running = true;
  CHECK(m.drops(con::ChatKind::everyone, 2));
  CHECK(!m.drops(con::ChatKind::everyone, 1));  // a teammate
  CHECK(!m.drops(con::ChatKind::team, 2));      // never team chat
  m.run("ignore_opponents 0");
  CHECK(!m.drops(con::ChatKind::spectator, 3));
  m.run("ignore_spec 1");
  CHECK(m.drops(con::ChatKind::spectator, 3));
  m.r.spectator[0] = true;  // watching yourself: 1 lets it through
  CHECK(!m.drops(con::ChatKind::spectator, 3));
  CHECK(!m.drops(con::ChatKind::spectator_team, 3));
  m.run("ignore_spec 2");
  CHECK(m.drops(con::ChatKind::spectator_team, 3));
  CHECK(!m.drops(con::ChatKind::tv, -1));
  m.run("ignore_tv 1");
  CHECK(m.drops(con::ChatKind::tv, -1));
}

TEST(the_flood_drops_a_line_said_again_soon) {
  Room m;
  m.run("ignore_flood 1");
  CHECK(!m.drops(con::ChatKind::everyone, 1, "Bob: gg", 0));
  CHECK(m.drops(con::ChatKind::everyone, 1, "Bob: gg", 1));
  CHECK(!m.drops(con::ChatKind::everyone, 1, "Bob: gl", 1));    // another line
  CHECK(!m.drops(con::ChatKind::everyone, 1, "Bob: gg", 4.5));  // from the first kept, 4 s on
  CHECK(!m.drops(con::ChatKind::team, 1, "(Bob): go", 0));
  CHECK(!m.drops(con::ChatKind::team, 1, "(Bob): go", 1));  // team chat only with 2
  m.run("ignore_flood 2");
  CHECK(!m.drops(con::ChatKind::team, 1, "(Bob): go", 1.5));  // kept and remembered now
  CHECK(m.drops(con::ChatKind::team, 1, "(Bob): go", 2));
  CHECK(!m.drops(con::ChatKind::everyone, 0, "me: hi", 0));
  CHECK(!m.drops(con::ChatKind::everyone, 0, "me: hi", 0.5));  // never your own
}

TEST(text_and_nick_filters_drop_chat_lines) {
  CHECK(con::chat_text_matches("*camp*", "bob: stop camping"));
  CHECK(!con::chat_text_matches("camp", "bob: camp"));  // the whole line
  CHECK(con::chat_text_matches("b?b: \\*", "bob: *"));
  CHECK(!con::chat_text_matches("*CAMP*", "bob: camp"));  // case counts
  con::Console c;
  con::ChatIgnore g;
  con::add_chat_ignore(c, g);
  c.execute("ignoretext *noob*; ignoretext ab; ignorenick Bob");
  CHECK(g.texts.size() == 1 && g.nicks.size() == 1);  // "ab" too short
  con::ChatRoster r;
  const auto line = [](const char* text) {
    con::ChatLine l;
    l.chat = true;
    l.text = text;
    return l;
  };
  CHECK(con::chat_ignored(g, r, line("eve: you noob\n"), 0));
  CHECK(con::chat_ignored(g, r, line("(Bob): rl here"), 0));
  CHECK(con::chat_ignored(g, r, line("[SPEC] Bob: hi"), 0));
  CHECK(!con::chat_ignored(g, r, line("bob: hi"), 0));
  con::ChatLine server = line("Bob: not chat");
  server.chat = false;
  CHECK(!con::chat_ignored(g, r, server, 0));  // only chat lines
  c.clear_output();
  c.execute("ignoretext");
  CHECK(c.output().size() == 2 && c.output()[1] == "\"*noob*\" (1 hit)");
  c.execute("unignorenick all; unignoretext *noob*");
  CHECK(g.texts.empty() && g.nicks.empty());
}
