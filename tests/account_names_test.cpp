// Tests: an account's name (engine/account/names.hpp): what a name may be, and the key it is
// reserved by, folded per game (case, look-alikes, separators, coloured and high-bit text).
//
// ge:summary Tests: account names checked, keys folded by each game's rules and by every game's
// ge:keywords test account name fold key look-alike high-bit caret check_account_name name_key
// ge:skill net testing
#include <string>

#include "account/names.hpp"
#include "check.hpp"

namespace ga = ge::account;

TEST(a_name_is_2_to_15_letters_digits_dashes_dots_or_underscores) {
  CHECK(ga::check_account_name("Niclas").empty());
  CHECK(ga::check_account_name("a.b-c_9").empty());
  CHECK(ga::check_account_name("x").size() > 0);
  CHECK(ga::check_account_name("sixteen_letters_").size() > 0);
  CHECK(ga::check_account_name("two words").size() > 0);
  CHECK(ga::check_account_name("caret^1").size() > 0);
  CHECK(ga::check_account_name("__").size() > 0);
  const std::string why = ga::check_account_name("two words");
  CHECK(why.find("next:") == std::string::npos && why.find('`') == std::string::npos);
}

TEST(look_alikes_case_and_separators_share_a_key) {
  const ga::NameRules plain{};
  const std::string key = ga::name_key(plain, "Niclas");
  CHECK(key == "nlclas");
  CHECK(ga::name_key(plain, "n1clas") == key);
  CHECK(ga::name_key(plain, "NIC_LAS") == key);
  CHECK(ga::name_key(plain, "nic.las") == key);
  CHECK(ga::name_key(plain, "Zer0") == ga::name_key(plain, "zero"));
  CHECK(ga::name_key(plain, "Rocket") != key);
}

TEST(a_game_with_coloured_text_folds_it_away_and_one_without_keeps_it) {
  const ga::NameRules high{31, true, false}, caret{15, false, true}, plain{};
  std::string red = "Niclas";
  for (char& c : red) c = static_cast<char>(static_cast<uint8_t>(c) | 0x80);
  CHECK(ga::name_key(high, red) == "nlclas");
  CHECK(ga::name_key(plain, red) != "nlclas");
  // the special digit glyphs read as digits, which fold as their look-alikes
  const std::string gold = std::string("zer") + static_cast<char>(0x12);  // the glyph for 0
  CHECK(ga::name_key(high, gold) == ga::name_key(plain, "zero"));
  CHECK(ga::name_key(caret, "^1Nic^7las") == "nlclas");
  CHECK(ga::name_key(ga::kEveryGame, red) == ga::name_key(ga::kEveryGame, "^2niclas"));
}
