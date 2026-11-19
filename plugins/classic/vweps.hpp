// THE WEAPON A PLAYER CARRIES, DRAWN IN HIS HANDS (the first classic game's visible weapons): the
// server names a list of models once a level, a player's entry carries an index into it, and the
// client draws the body without its own gun (the list's first entry) with the listed weapon on the
// body's frame. Facts: porting/shooters-edge/visible-weapons.md.
//
// - **The list** comes as a stuffed line `//vwep <names>` before the model list (`read_vwep_line`),
//   at most `kMaxVweps` names; entry 0 is the body, `-` an empty entry (a `-` body: the weapon
//   alone is the figure). A name with a `/` is a path as it is; a bare one is `progs/<name>`,
//   `.mdl` added when it has no dot (`vwep_path`). A name climbing out of the game folder (`..`, a
//   leading slash) is refused.
// - **The index** travels in the impulse of a player's command; 0 is none. A recording from the
//   server's side carries none: there the list is the fixed one (`kFixedVweps`) and a player's
//   weapon is the one his weapon stat names (`vwep_of_view_model`), a body on a death frame
//   (`is_death_frame`) drawn without its gun.
// - **Stand-ins** (`FigureParts`, `vwep_stand_in`): a player who lacks the listed files gets them
//   made from the game's own pack, never kept on disk. The body is the player figure with every
//   piece but the largest cut out (the gun in the hands, the axe the axe frames show, the muzzle's
//   flash). `w_axe` and `w_shot` are the figure's own axe and gun alone. Any other `w_<stem>` is
//   the pack's pickup model `g_<stem>` (`w_shot2`'s: `g_shot`, the double-barrelled shotgun lying
//   about) scaled to the hands and posed in every frame: its rear grip (the rearmost piece hanging
//   under it) in the rear hand, pointing and rolled as the figure's own gun, tipped up only as far
//   as keeps its underside from sinking below the front hand (morph/carry.hpp). A frame whose
//   hands come apart (a death) moves it the way the gun moves instead.
//
// ge:summary Visible weapons: the vwep list and index, the fixed list, stand-ins from the pack
// ge:keywords vwep visible-weapons view-weapons carried-weapon vw_index vwplayer w_axe w_shot
// ge:keywords read_vwep_line vwep_path stand-in cut-out figure-parts pickup-model g_rock
// ge:keywords r_drawvweps cl_novweps cl_vwep death-frames multiview
// ge:skill classic morph
// ge:test tests/classic_vweps_test.cpp
#pragma once

#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "morph/model.hpp"

namespace ge::classic {

inline constexpr size_t kMaxVweps = 32;
// The extension bit a client asks for and a server offers (`*z_ext`): visible weapons.
inline constexpr uint32_t kVwepExtBit = 0x80;

// The names of a stuffed `//vwep ...` line (white space between them, the first kMaxVweps kept)
// into `names`; false (names untouched) when `text` is not one. An empty line is a list of none:
// the feature off.
bool read_vwep_line(std::string_view text, std::vector<std::string>& names);
// The file a listed name opens ("" for `-`, and for a refused name: `refused` set).
std::string vwep_path(std::string_view name, bool* refused = nullptr);

// The list a server's recording is drawn with, and the weapon of a view model's file.
inline constexpr std::array<std::string_view, 9> kFixedVweps = {
    "vwplayer", "w_axe", "w_shot", "w_shot2", "w_nail", "w_nail2", "w_rock", "w_rock2", "w_light"};
// `progs/v_axe.mdl` 1 ... `progs/v_light.mdl` 8; 0 for any other.
int vwep_of_view_model(std::string_view path);
// The player figure's death frames (41..102: `axdeth1` to `deathe9`).
inline bool is_death_frame(int frame) { return frame >= 41 && frame <= 102; }

// The player figure's pieces (morph/carry.hpp): the body, the gun its frames hold, the axe its axe
// frames show; -1 for one not found. `piece_of` holds each vertex's piece. The hands are the two
// ends of the body's surface nearest the gun (its others the head and the feet), each the
// vertices within kHandReach of its end along the surface; the rear one nearer the gun's back.
struct FigureParts {
  std::vector<int> piece_of;
  int gun = -1, axe = -1;
  size_t stand = 0;                             // the frame the gun is measured in (`stand1`)
  std::vector<uint32_t> rear_hand, front_hand;  // empty when not found
};
inline constexpr double kHandReach = 5.5;  // units along the surface from a hand's end
// False when `figure` is not one with a body, a gun and an axe (another game's, a damaged file).
bool figure_parts(const morph::Model& figure, FigureParts& out);

// Opens a model of the pack by its path (`progs/g_rock.mdl`), false when it has none.
using OpenModel = std::function<bool(std::string_view path, morph::Model& out)>;
// The stand-in for listed file `path` (`progs/vwplayer.mdl`, `progs/w_rock.mdl`) made from the
// player figure `figure` and the pack; false for a name it cannot make.
// `skin_of`, when given, is set to the pack's model whose skin the stand-in wears (the figure's or
// the pickup's), so its enhanced copies are its too.
bool vwep_stand_in(std::string_view path, const morph::Model& figure, const FigureParts& parts,
                   const OpenModel& open, morph::Model& out, std::string* skin_of = nullptr);

}  // namespace ge::classic
