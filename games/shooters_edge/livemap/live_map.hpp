// THE LIVE MAP: a match watched from above, the map alive under it (docs/goals/M62.md). What a
// spectator sees on TV or in a recording played back, full screen or in a corner of the view he
// rides, and never on a server joined live: a player could read every enemy's place off it.
//
// THE PLATE (`build_plate`, once a level, on any thread): the map drawn straight down from its
// own faces (the engine's overview, engine/overview/plate.hpp, over the brush plugin's floors),
// the void a blueprint; the items the map places (`Spot`, with the seconds
// each takes to come back), its teleporters joined to where they lead, its liquids' pixels kept for
// the glints.
//
// THE FRAME (`draw_live_map`, every frame, into a draw list the GPU pass or the CPU raster paints):
// - every entity that carries a model, the game's own model drawn as a FIGURE (icons.hpp): at its
//   own angles and frame as the game shows it (an item turning 100 degrees a second, a torch's
//   flame flickering through its frames, a rocket pointing where it flies, a grenade tumbling),
//   only seen from the south from three quarters above, the way the items' icons are, larger than
//   life; the powers glow under them;
// - every player in his shirt and pants colours, dead ones dimmed, a quad or a shield as a glow
//   under him, his name under him on the full map; the player FOLLOWED in a faint blue glow off his
//   own figure and named over the map ("Following <name>");
// - rockets and grenades trailing smoke; an explosion only a short soft flash where it went off
//   (no flying particles), a teleport a faint ring;
// - an item taken: a ring that runs down to its return and the seconds left; back: a soft glow;
// - the water a translucent layer of its own over what lies under it (an item or a player under
//   the surface tinted by it and seen through it), glinting; the teleporters' ways flowing toward
//   where each puts you, in a map's preview only (`LiveView::links`), never in a match watched;
// - depth: a figure smaller the further below the map's top it stands, faster under a liquid,
//   and one under a liquid cut to the liquid round it (nothing shows past the shore); the health
//   and ammo boxes and the turning items (weapons, armour, powers: larger than life, still
//   turning) seen straight down; anything under a bridge or a ledge dimmed, a box in a dotted
//   square, the rest in a dotted ring.
// The figures are drawn on the CPU into one atlas of cells (`kCell` px), each kept by what it shows
// (the model, its frame, its colours, its yaw to `kYawSteps`, its size) and the least recently used
// one given up, at most `kNewFiguresAFrame` drawn a frame (the rest wait a frame as a dot), so the
// atlas goes to the GPU at most once a frame.
//
// ge:summary The live map: a match from above for TV and demos, the map alive, the player followed
// ge:keywords live-map overview spectate tv demo minimap from-above figures players miniature
// ge:keywords spinning-items respawn-timer sparkle water-glint teleporter-link following
// ge:keywords follow-player rocket-trail explosion demo_map corner-map anti-cheat
// ge:skill map-design ui replay
// ge:test tests/shooters_edge_live_map_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "overview/plate.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/livemap/icons.hpp"
#include "ui/draw.hpp"
#include "ui/image.hpp"

namespace ge::console {
class Cvars;
}

namespace shooters_edge::livemap {

// The settings a spectator picks (`add_live_map_cvars`).
struct LiveMapSettings {
  int mode = 0;          // demo_map: 0 off, 1 the whole screen over the HUD, 2 a corner of the view
  float scale = 0.34f;   // demo_map_scale: the corner map's height, a share of the screen's
  int names = 1;         // demo_map_names: the players' names under them on the whole map
  float figures = 2.4f;  // demo_map_figures: the figures' size against life
};

inline constexpr int kCell = 64;              // px: one figure's cell in the atlas
inline constexpr int kAtlasCells = 24;        // cells a side
inline constexpr int kYawSteps = 32;          // a figure's yaw and pitch kept to 360 / this
inline constexpr int kNewFiguresAFrame = 24;  // figures drawn on the CPU a frame, at most
inline constexpr double kItemSpinDegS = 100;  // an item's turn, degrees a second
inline constexpr double kElevationDeg = 48;   // the figures seen from this far above

// An item the map places.
struct Spot {
  double at[3] = {};  // units
  const ItemKind* kind = nullptr;
};

// A teleporter's trigger joined to its destination, units (x, y).
struct Link {
  double from[2] = {}, to[2] = {};
};

// The map from above (engine/overview's picture: the floors shaded, the liquids a layer, the
// roofs) with the game's things on it.
struct Plate : ge::overview::Picture {
  std::string map;
  std::vector<Spot> spots;
  std::vector<Link> links;
};

// The map file `bytes` (`maps/<name>.bsp`, any family the brush plugin reads) as a plate `max_px`
// px on its long side; false with the reason when it does not read. A map that keeps its textures
// outside (38) takes them from `files` (read from any thread: the archives are held in memory).
bool build_plate(std::span<const uint8_t> bytes, const std::string& name,
                 std::span<const uint8_t> palette, int max_px, Plate* out, std::string* error,
                 const client::SearchPath* files = nullptr);

// Where and how the frame is drawn.
struct LiveView {
  ge::ui::Rect area;     // px: the map fitted in it, centred
  ge::ui::Rect place{};  // px: when set (w > 0), the map drawn exactly here instead (a zoom)
  bool fill = false;     // the whole area painted the plate's void first (the whole screen)
  bool names = false;    // players' names under them
  bool links = false;    // the teleporters' ways, flowing to where each puts you: a map's preview
                         // only, never a match watched (they would show where players go)
  int follow = -1;       // the slot followed (-1: none)
  float figures = 2.4f;  // the figures' size against life
  float min_figure_px = 18;  // a player's figure at least this tall, px
  double now_s = 0;          // the demo's clock, s: what turns, flows and runs down
};

// One figure kept in the atlas.
struct FigureKey {
  int model = 0, frame = 0, skin = 0, top = -1, bottom = -1, yaw = 0, pitch = 0, size = 0;
  bool above = false;  // seen straight down (a box, a turning item), else from three quarters
  bool operator<(const FigureKey& o) const;
};

struct LiveMap {
  std::shared_ptr<const Plate> plate;
  // The models by their name, read the first time an entity shows one (`ok` false: none).
  const client::SearchPath* files = nullptr;
  std::vector<uint8_t> palette;
  struct Art {
    ModelArt art;
    Mesh box;  // a small map's (the first family's boxes)
    bool small_map = false;
    float radius = 0;  // units: the farthest point of the first frame from the origin
    bool ok = false;
  };
  std::map<std::string, int> model_of;
  std::vector<std::unique_ptr<Art>> models;
  // The figures' atlas and which cell holds what.
  ge::ui::Image atlas;
  std::map<FigureKey, int> cells;
  std::vector<FigureKey> held;  // by cell
  std::vector<int64_t> used;    // by cell: the frame it was last drawn
  // The followed player's halo: his figure's own outline, blurred and blue (`halo_for` its key).
  ge::ui::Image halo;
  FigureKey halo_for;
  // The figures under a liquid, each cut to the liquid round it (so nothing under the surface
  // shows past its shore): a second, smaller atlas, its cells kept by the figure and its place.
  struct UnderKey {
    FigureKey key;
    int x = 0, y = 0, size = 0, scale = 0;  // plate px, screen px, screen px a plate px x 1000
    bool operator==(const UnderKey& o) const;
  };
  ge::ui::Image under;
  std::vector<UnderKey> under_held;
  std::vector<int64_t> under_used;
  int under_drawn_this_frame = 0;
  int64_t frames = 0;
  int drawn_this_frame = 0;
  int figures_drawn = 0;  // in all (a lab's count)
  // What happens over time.
  struct Gone {
    bool present = true;
    double since = -1;  // when it went or came back, s
  };
  std::vector<Gone> spots;  // by the plate's spot
  struct Flash {
    double at[2] = {};
    double t = 0;
    int kind = 0;  // 0 an explosion, 1 a teleport, 2 an item back
  };
  std::vector<Flash> flashes;
  struct Trail {
    std::vector<std::array<float, 3>> points;  // x, y (units), the clock
    int64_t seen = 0;
    bool rocket = false;
  };
  std::map<int, Trail> trails;  // by entity number
  int64_t effects_frame = -1;   // the demo's frame whose effects were taken
  double last_now = -1;
};

// A new level: the plate made (its picture `max_px` on the long side) and what ran on the last
// one forgotten. `files` must outlive the map's use.
bool live_map_level(LiveMap& lm, const client::SearchPath& files, std::span<const uint8_t> palette,
                    std::span<const uint8_t> bsp, const std::string& name, int max_px,
                    std::string* error);
// The same with a plate made elsewhere (on another thread).
void live_map_take(LiveMap& lm, const client::SearchPath& files, std::span<const uint8_t> palette,
                   std::shared_ptr<const Plate> plate);

// The frame of `s` (the state drawn) into `dl`. Nothing without a plate.
void draw_live_map(LiveMap& lm, const client::DemoState& s, const LiveView& v,
                   ge::ui::DrawList& dl);

// The map's rectangle in `area` (the plate fitted, centred), px.
ge::ui::Rect live_map_rect(const Plate& p, ge::ui::Rect area);

// The palette of the game `s` was recorded in (the sequel's, from its pictures, for its formats;
// else the classic game's), viewing `store` or a pack; empty when the files hold none.
std::span<const uint8_t> recording_palette(const client::SearchPath& files,
                                           const client::DemoState& s, std::vector<uint8_t>& store);

// The next slot to follow after `from` (each way by `dir`): a player playing (named, not a
// spectator) in slot order, round the end; -1 when nobody plays.
int next_followed(const client::DemoState& s, int from, int dir);

// The words over `s` that a spectator reads: "Following <name>".
std::string following_text(const client::DemoState& s, int slot);

// `demo_map`, `demo_map_scale`, `demo_map_names`, `demo_map_figures` bound to `settings`.
void add_live_map_cvars(ge::console::Cvars& cvars, LiveMapSettings& settings);

}  // namespace shooters_edge::livemap
