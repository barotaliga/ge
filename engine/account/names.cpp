// An account's name (account/names.hpp): the check and the folding.
//
// ge:summary An account's name: the check of its characters and the folding into its key
// ge:keywords account name fold key look-alike high-bit caret
// ge:skill net
// ge:test tests/account_names_test.cpp
#include "account/names.hpp"

namespace ge::account {

namespace {

bool name_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
         c == '-' || c == '.';
}

}  // namespace

std::string check_account_name(std::string_view name) {
  const std::string what =
      "A name is 2 to 15 letters, digits, dashes, dots or underscores, with no spaces.";
  if (name.size() < kNameMin || name.size() > kNameMax) return what;
  bool alnum = false;
  for (const char c : name) {
    if (!name_char(c)) return what;
    if (c != '_' && c != '-' && c != '.') alnum = true;
  }
  if (!alnum) return "A name needs at least one letter or digit.";
  return "";
}

std::string name_key(const NameRules& rules, std::string_view name) {
  std::string out;
  out.reserve(name.size());
  for (size_t i = 0; i < name.size(); i++) {
    auto c = static_cast<uint8_t>(name[i]);
    if (rules.caret_colours && c == '^' && i + 1 < name.size()) {
      i++;  // the colour's character goes with the caret
      continue;
    }
    if (rules.high_bit) {
      c &= 0x7F;
      if (c >= 0x12 && c <= 0x1B) c = static_cast<uint8_t>('0' + (c - 0x12));  // the digit glyphs
      if (c == 0x10) c = '[';
      if (c == 0x11) c = ']';
    }
    if (c >= 'A' && c <= 'Z') c = static_cast<uint8_t>(c - 'A' + 'a');
    switch (c) {
      case '0':
        c = 'o';
        break;
      case '1':
      case 'i':
      case '|':
      case '!':
        c = 'l';
        break;
      case '5':
      case '$':
        c = 's';
        break;
      case '3':
        c = 'e';
        break;
      case '4':
      case '@':
        c = 'a';
        break;
      default:
        break;
    }
    if (c == ' ' || c == '_' || c == '-' || c == '.' || c < 0x20 || c == 0x7F) continue;
    out.push_back(static_cast<char>(c));
  }
  return out;
}

}  // namespace ge::account
