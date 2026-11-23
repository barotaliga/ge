// The dedicated server's own console words per game (net/server_words.hpp): one row a word, the
// help in a player's words starting with the word, each other spelling once and never a word of
// ours, no word the console has of its own (it would never be the game's), and each game's server
// declaring its table.
//
// ge:summary Tests the dedicated server's own console words per game: the tables and their use
// ge:keywords server-words game-words console-words dedicated-server manual test
// ge:skill console net testing
#include <set>
#include <string>

#include "check.hpp"
#include "console/console.hpp"
#include "shooters_edge/net/q2_game.hpp"
#include "shooters_edge/net/qw_game.hpp"
#include "shooters_edge/net/server_words.hpp"

namespace gs = ge::server;
namespace sn = shooters_edge::net;

namespace {

template <size_t N>
void check_table(const gs::GameWord (&words)[N], const char* game) {
  ge::console::Console con(true);
  std::set<std::string> names;
  for (const gs::GameWord& w : words) {
    const std::string name = w.name;
    CHECK_MSG(names.insert(name).second, std::string(game) + ": " + name + " is listed twice");
    const std::string help = w.help;
    CHECK_MSG(help.rfind(name + " ", 0) == 0 || help.rfind(name + ":", 0) == 0,
              std::string(game) + ": " + name + "'s help does not start with it: " + help);
    CHECK_MSG(help.find("next:") == std::string::npos && help.find('`') == std::string::npos,
              std::string(game) + ": " + name + "'s help is not in a player's words");
    CHECK_MSG(con.commands().count(name) == 0,
              std::string(game) + ": " + name + " is the console's own and never the game's");
  }
  for (const gs::GameWord& w : words) {
    if (w.also == nullptr) continue;
    CHECK_MSG(names.insert(w.also).second,
              std::string(game) + ": " + w.also + " is a word and a spelling, or two spellings");
    CHECK_MSG(con.commands().count(w.also) == 0,
              std::string(game) + ": " + w.also + " is the console's own");
  }
}

}  // namespace

TEST(each_games_server_words_are_one_row_each_in_a_players_words) {
  check_table(sn::kQwServerWords, "qw");
  check_table(sn::kQ2ServerWords, "q2");
}

TEST(each_games_server_declares_its_words) {
  CHECK(sn::qw_game().words == sn::kQwServerWords);
  CHECK(sn::qw_game().word_count == static_cast<int>(std::size(sn::kQwServerWords)));
  CHECK(sn::q2_game().words == sn::kQ2ServerWords);
  CHECK(sn::q2_game().word_count == static_cast<int>(std::size(sn::kQ2ServerWords)));
}
