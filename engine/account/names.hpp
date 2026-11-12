// AN ACCOUNT'S NAME, AND THE KEY IT IS RESERVED BY (docs/accounts.md § Names). One account
// holds one name in every game, so a name must be one every game can show as it is: 2 to 15
// characters of letters, digits, `_`, `-` and `.` (no space: a console line splits on it).
//
// A name is reserved by its KEY, the name FOLDED: whatever a game draws differently but a player
// reads alike is made the same. A guest's name on a game's server is folded by that game's rules
// (`NameRules`: a game whose names carry coloured or high-bit text strips it first) and refused
// when its key is an account's. The folding:
//   - a game's own text marks go: the high bit of each byte (coloured text) and a game's special
//     glyphs for digits and brackets, with `high_bit`; a caret and the character after it, with
//     `caret_colours`;
//   - letters go to lower case;
//   - look-alikes go to one letter: 0 o; 1 i l | ! all l; 5 $ s; 3 e; 4 @ a;
//   - separators go: space, `_`, `-`, `.`.
// So `Niclas`, `n1clas`, `NIC_LAS` and the same name in coloured text share the key `nlclas`.
// An account's own key is folded by every rule at once (`kEveryGame`), so a sign-up is refused a
// name any game's guest could pass off as another's.
//
//   check_account_name(name) -> "" or the words saying what a name may be
//   name_key(rules, name)    -> the folded key
//
// ge:summary An account's name: what a name may be, and its key folded per game for the reservation
// ge:keywords account name names reserved reservation fold folding key look-alike confusable
// ge:keywords lower-case high-bit coloured caret colour check_account_name name_key name_rules
// ge:keywords every-game forcenick
// ge:skill net
// ge:test tests/account_names_test.cpp
#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace ge::account {

constexpr size_t kNameMin = 2, kNameMax = 15;

// What a game's names may carry beside the letters, and how long its player list shows them.
struct NameRules {
  uint8_t max_len = kNameMax;
  bool high_bit = false;       // a byte's high bit draws it in another colour; special digit glyphs
  bool caret_colours = false;  // `^` and the character after it set the colour
};

// Folded by every game's rules: the key an account's own name is held by.
constexpr NameRules kEveryGame{kNameMax, true, true};

// "" when `name` may be an account's; otherwise a sentence a player reads saying what may be.
std::string check_account_name(std::string_view name);

// The key `name` is reserved by, folded by `rules` (above). Empty when nothing is left.
std::string name_key(const NameRules& rules, std::string_view name);

}  // namespace ge::account
