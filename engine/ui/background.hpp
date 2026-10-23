// A BACKDROP: what is behind a menu, a loading card, a pause card or a console panel. Most games
// put one of a few things there: a flat colour, a gradient, a picture (often drifting slowly, so
// the menu does not look frozen), or the game itself running behind the menu, softened so the
// words in front stay readable. A `Background` is that as data: an ordered list of layers,
// bottom first, drawn into any rectangle by `draw_background` (the whole screen for a menu, the
// panel's box for a console).
//
// THE LAYERS:
//   - paint: a colour or a gradient (paint.hpp), in UNIT coordinates of the rectangle (0..1
//     across and 0..1 down), so one backdrop reads the same on a wide monitor and on a phone held
//     upright; a radial paint is an ellipse of the rectangle's proportions;
//   - image: a picture placed by `fit` round its focus point, optionally DRIFTING (a slow zoom
//     and pan that eases out and back over `drift.period_s`, the "slow push" of a title screen),
//     tinted and faded by `tint`;
//   - scene: the same as an image, but the picture is the one handed to `draw_background` this
//     frame: the live game frame (or the GPU's colour target, M22b) the menu sits over.
//
// THE FINISHES, as paint layers over the rest: a SCRIM (a gradient that darkens one side, where
// a column of buttons stands) and a VIGNETTE (darkened corners), plus `dim` for a flat veil. A
// blurred backdrop is an image layer over `blurred(...)` (image.hpp), made once.
//
// ge:summary Menu and console backdrops: colour, gradient, drifting image, live scene, scrims
// ge:keywords ui background backdrop menu console panel gradient image picture scene live
// ge:keywords drift ken-burns slow-push zoom pan scrim vignette dim tint wallpaper title
// ge:skill ui
// ge:test tests/ui_background_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "ui/draw.hpp"
#include "ui/image.hpp"
#include "ui/paint.hpp"

namespace ge::ui {

enum class LayerKind : uint8_t { paint, image, scene };

// A slow zoom and pan: from (zoom_from, focus_from) to (zoom_to, focus_to) and back, eased, once
// every `period_s`. 0 holds the `from` framing.
struct Drift {
  float period_s = 0;
  float zoom_from = 1, zoom_to = 1;
  float focus_from_x = 0.5f, focus_from_y = 0.5f;
  float focus_to_x = 0.5f, focus_to_y = 0.5f;
};

struct Layer {
  LayerKind kind = LayerKind::paint;
  Paint paint;                   // paint: in unit coordinates
  const Image* image = nullptr;  // image: the picture (kept by reference)
  Fit fit = Fit::cover;
  Drift drift;
  Color tint{1, 1, 1, 1};  // image, scene: multiplies the picture; its alpha fades it
};

enum class Side : uint8_t { left, right, top, bottom };

struct Background {
  std::vector<Layer> layers;

  // The common starts.
  static Background color(Color c);
  // A gradient across the rectangle at `degrees` (CSS's: 180 runs top to bottom).
  static Background gradient(float degrees, const Stop* stops, int n);
  static Background gradient(float degrees, Color from, Color to);
  static Background image(const Image& img, Fit fit = Fit::cover, Drift drift = {});
  static Background scene(Drift drift = {});

  // Finishes, each appended as a layer over what is there.
  Background& paint(const Paint& unit_paint);
  Background& glow(float cx, float cy, float radius, Color c);  // a soft radial light
  Background& scrim(Side side, float strength = 0.75f, float reach = 0.6f, Color c = {0, 0, 0, 1});
  Background& vignette(float strength = 0.5f, Color c = {0, 0, 0, 1});
  Background& dim(float amount, Color c = {0, 0, 0, 1});
  Background& tint(Color c);  // the last image or scene layer's
};

// The drift's framing at time t: the zoom and the focus point.
struct Framing {
  float zoom = 1, focus_x = 0.5f, focus_y = 0.5f;
};
Framing framing_at(const Drift& d, double time_s);

// The layers of `bg` drawn into `area` at time `time_s`. `scene` is this frame's picture for a
// scene layer (none: the layer is skipped).
void draw_background(DrawList& list, const Background& bg, Rect area, double time_s,
                     const Image* scene = nullptr);

}  // namespace ge::ui
