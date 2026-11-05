// SHOOTER'S EDGE'S SCREENSHOTS PAGE (OPTIONS ▸ SCREENSHOTS, `menu_screenshots`): the engine's
// gallery (capture/gallery.hpp) drawn in the modern menu's look. The gallery does the listing,
// the moves, the loads and the deletes; this page holds the textures and the words:
//
// - THE GRID: the title, the page's count, the thumbnails in cells sized to the screen
//   (`gallery_cells`: a phone held upright gets two columns), the selected cell ringed in the
//   accent; a cell still loading is a plain slab, a file that does not read says so.
// - THE VIEWER: the picture at full size on a dark ground (its thumbnail stretched while the full
//   one loads), its place in the list over it.
// - THE CONFIRM: a box over either asking whether to delete it, ENTER deleting, ESC keeping it.
// - THE FOOTER: the selected file's name, when it was taken, its pixel size, format and bytes, or
//   why the last delete failed; under it the keys.
// - AN EMPTY FOLDER says F12 takes a screenshot and where they are kept.
//
// The textures are slots that stay at one address (the GPU pass keys its textures by it), filled
// again only when a picture's stamp moves (`ui::Image::revision`).
//
// ge:summary Shooter's Edge's screenshots page: the engine gallery drawn in the menu's look
// ge:keywords gallery screenshots page menu_screenshots thumbnail viewer confirm delete grid
// ge:keywords shooters-edge options slots revision footer empty-folder
// ge:skill ui replay
// ge:test tests/shooters_edge_menu_test.cpp
#pragma once

#include <cstdint>
#include <deque>
#include <vector>

#include "capture/gallery.hpp"
#include "ui/draw.hpp"
#include "ui/image.hpp"
#include "ui/menu_view.hpp"

namespace shooters_edge::menu {

class GalleryPage {
 public:
  // The page over a `width` x `height` frame: the gallery's grid set to the cells that fit, the
  // textures brought up to its pictures, every box drawn into `list`.
  void draw(ge::ui::DrawList& list, ge::capture::Gallery& gallery, int width, int height,
            const ge::ui::MenuStyle& style);
  // The entry under (x, y) in the last drawn grid; -1 none (or not on the grid).
  int hit(float x, float y) const;
  // What the layout drawn last found wrong: words wider than their box.
  const std::vector<std::string>& problems() const { return problems_; }

 private:
  struct Slot {
    ge::ui::Image image;
    uint64_t stamp = 0;  // the gallery's picture it holds
  };
  // `slot` holding `pic` (nothing when null); true when it holds a picture.
  bool fill(Slot& slot, const ge::capture::GalleryPicture* pic);

  std::deque<Slot> thumbs_;  // a deque: a slot keeps its address as more are added
  Slot full_;
  uint64_t revision_ = 0;
  std::vector<ge::ui::Rect> cells_;  // the page's cells, from its first entry
  int first_ = 0;
  bool on_grid_ = false;
  std::vector<std::string> problems_;
};

}  // namespace shooters_edge::menu
