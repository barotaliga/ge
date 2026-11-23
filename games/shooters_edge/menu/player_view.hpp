// THE PLAYER PAGE'S FIGURE (Quake's, QuakeWorld's and Quake II's setup page alike): the player's
// model drawn live on the CPU in the skin and colours the page's rows hold, standing on a slow
// turntable the pointer drags round, holding the weapon the page's WEAPON row picks (the carried
// weapon when visible weapons are on, `r_drawvweps`), and firing it when the figure is clicked.
//
// - **The figure**: Quake's `progs/player.mdl` wearing `skins/<skin>.pcx` (QuakeWorld's skins,
//   downloaded ones too: the files' search path holds them) in the shirt and pants colours; with
//   visible weapons on, the body without its gun (`progs/vwplayer.mdl`, or the stand-in cut from
//   the figure) and the weapon's own model (`progs/w_<name>.mdl` or its stand-in,
//   plugins/classic/vweps.hpp) on the body's frame. Quake II's `players/<model>/tris.md2` in
//   `<skin>.pcx` (.tga, .png) and the folder's carried weapon (`w_<name>.md2`, else its
//   `weapon.md2`, else male's), or always `weapon.md2` with visible weapons off.
// - **The motion**: the figure stands (its stand frames at 10 a second); a click on it plays the
//   held weapon's attack frames once (Quake's axe, shotgun, nailgun, rocket and lightning frames by
//   their names; Quake II's attack frames); a drag turns it (half a degree a pixel), and left
//   alone it turns on by itself, a full turn in 12 seconds.
// - **The colour picker** (`swatch_hit`): the fourteen colours a shirt or pants can take, drawn as
//   two rows of swatches under the figure; a click on one sets `topcolor` or `bottomcolor`.
//
// The picture is drawn again only when the frame, the turn or the look changed; the models are read
// once per look. The drawing is the engine's (overview/mesh_picture.hpp's `draw_turned`), the
// figure art the classic plugin's (classic/figure_art.hpp).
//
// ge:summary The player page's figure: the model live in skin and colours, turned, armed, firing
// ge:keywords player-page player-setup figure turntable drag-to-turn skin-selector colour-picker
// ge:keywords swatches vwep carried-weapon firing-animation attack-frames r_drawvweps menuplyr
// ge:skill ui shooter
// ge:test tests/shooters_edge_player_view_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "classic/figure_art.hpp"
#include "ui/image.hpp"
#include "vfs/search.hpp"

namespace shooters_edge::menu {

// What the figure wears and holds, from the page's rows and the console.
struct FigureLook {
  bool q2 = false;
  std::string skin = {};  // Quake: a `skins/` picture's name ("" base); Quake II: `<model>/<skin>`
  int top = -1, bottom = -1;  // Quake's shirt and pants colours (0..13)
  bool vweps = false;         // r_drawvweps: the carried weapon drawn
  int weapon = 0;             // an index into figure_weapons(q2)
  bool operator==(const FigureLook&) const = default;
};

// The weapons the WEAPON row offers, in the game's order, as a player reads them.
std::span<const std::string_view> figure_weapons(bool q2);
// The `skins/` pictures the files hold (QuakeWorld's skins, downloaded ones too), by name without
// the folder and the extension, sorted; `base` first when it is there.
std::vector<std::string> quake_skins(const ge::vfs::SearchPath& files);

// The pointer over the page this frame (screen px).
struct Pointer {
  float x = 0, y = 0;
  bool held = false;   // the primary button down
  bool click = false;  // released where it was pressed
};

class PlayerView {
 public:
  // The game's files and palette (768 bytes); a change reads the models again.
  void set_files(const ge::vfs::SearchPath* files, std::span<const uint8_t> palette);
  void set_look(const FigureLook& look);
  // One frame: the pointer over `area` (the figure's place on the screen) and `dt` seconds.
  void update(const Pointer& p, const ge::ui::Rect& area, double dt);
  void fire();
  // Turns the figure to `deg` and stops its own turning (a lab's still); `frame_s` its time.
  void hold(double yaw_deg, double time_s);

  // A figure to draw: the files hold the model.
  bool ready();
  // The figure `w` x `h` px; null when not ready.
  const ge::ui::Image* picture(int w, int h);

  double yaw() const { return yaw_; }
  bool firing() const { return fire_s_ >= 0; }
  bool dragging() const { return dragging_; }
  int frame() const { return frame_; }  // the body's frame drawn last
  const FigureLook& look() const { return look_; }

 private:
  void load();
  int pick_frame() const;

  const ge::vfs::SearchPath* files_ = nullptr;
  std::vector<uint8_t> palette_;
  FigureLook look_;
  bool loaded_ = false, have_ = false;
  ge::classic::ModelArt body_, weapon_;
  bool weapon_wears_skin_ = false;  // a stand-in cut from the figure: it wears the player's skin
  std::vector<int> idle_, attack_;  // the body's frames
  double t_ = 0, fire_s_ = -1, yaw_ = 0;
  bool dragging_ = false, held_ = false, spin_ = true;
  float last_x_ = 0, press_x_ = 0, press_y_ = 0;
  int frame_ = 0;
  ge::ui::Image picture_;
  int drawn_w_ = 0, drawn_h_ = 0, drawn_frame_ = -1;
  double drawn_yaw_ = 1e9;
  bool dirty_ = true;
};

// The colour picker under the figure in `area`: which swatch the point (x, y) is on, its row
// (0 shirt, 1 pants) in `row` and its colour (0..13) returned; -1 none.
int swatch_hit(const ge::ui::Rect& area, float x, float y, int* row);
// Swatch `colour` of row `row` in `area`.
ge::ui::Rect swatch_rect(const ge::ui::Rect& area, int row, int colour);

}  // namespace shooters_edge::menu
