// CUSTOMIZE CONTROLS' moves and binding them (controls_page.hpp).
//
// ge:summary CUSTOMIZE CONTROLS: the moves, the keys bound to each, binding and clearing a move
// ge:keywords customize-controls bind_control unbind_control bound_keys keys_text
// ge:skill ui console input
// ge:test tests/shooters_edge_menu_test.cpp
#include "shooters_edge/menu/controls_page.hpp"

#include "console/binds.hpp"
#include "console/cvar.hpp"

namespace shooters_edge::menu {

namespace {

// CUSTOMIZE CONTROLS' moves, in the classic page's order.
constexpr Control kControls[] = {
    {"+attack", "ATTACK"},      {"+forward", "MOVE FORWARD"},  {"+back", "MOVE BACK"},
    {"+moveleft", "STEP LEFT"}, {"+moveright", "STEP RIGHT"},  {"+jump", "JUMP"},
    {"+speed", "WALK / RUN"},   {"+strafe", "SIDESTEP"},       {"+left", "TURN LEFT"},
    {"+right", "TURN RIGHT"},   {"impulse 10", "NEXT WEAPON"},
};

}  // namespace

std::span<const Control> controls() { return kControls; }

const Control* control_of(std::string_view id) {
  if (id.size() < 2 || id[0] != 'c') return nullptr;
  size_t i = 0;
  for (char c : id.substr(1)) {
    if (c < '0' || c > '9') return nullptr;
    i = i * 10 + static_cast<size_t>(c - '0');
  }
  return i < std::size(kControls) ? &kControls[i] : nullptr;
}

std::vector<std::string> bound_keys(const ge::console::Binds& binds, std::string_view command) {
  std::vector<std::string> out;
  for (const auto& [key, cmd] : binds.table(ge::console::BindMode::game))
    if (ge::console::lower(cmd) == ge::console::lower(command)) out.push_back(key);
  return out;
}

std::string keys_text(const std::vector<std::string>& keys) {
  if (keys.empty()) return "---";
  const auto shown = [](const std::string& k) {
    if (k == "mwheelup") return std::string("WHEEL UP");
    if (k == "mwheeldown") return std::string("WHEEL DN");
    std::string out;
    for (char c : k) out += c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
    return out;
  };
  // As many as fit the classic value column's glyphs; the first always.
  std::string out = shown(keys[0]);
  for (size_t i = 1; i < keys.size(); i++) {
    const std::string next = out + ", " + shown(keys[i]);
    if (next.size() > kKeysChars) break;
    out = next;
  }
  return out;
}

bool bind_control(ge::console::Console& con, std::string_view command, std::string_view key) {
  std::string k;
  if (!ge::console::normalize_key(key, k)) return false;
  const std::vector<std::string> had = bound_keys(con.binds(), command);
  if (had.size() >= 2)
    for (const std::string& old : had) con.binds().remove(ge::console::BindMode::game, old);
  con.binds().set(ge::console::BindMode::game, k, command);
  return true;
}

void unbind_control(ge::console::Console& con, std::string_view command) {
  for (const std::string& k : bound_keys(con.binds(), command))
    con.binds().remove(ge::console::BindMode::game, k);
}

}  // namespace shooters_edge::menu
