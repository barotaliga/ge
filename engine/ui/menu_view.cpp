// A menu laid out and drawn, defined: the scale from the screen's shape, the column's place in
// landscape and portrait, the fit-to-screen shrink, the problems a screenshot would show, the
// highlight's glide, the page's fade, and the slabs, values, sliders and small print drawn.
//
// ge:summary The menu view defined: responsive layout, problems, the glide, slabs and values drawn
// ge:keywords ui menu view layout scale portrait landscape problems overflow glide fade slab slider
// ge:skill ui
// ge:test tests/ui_menu_test.cpp
#include "ui/menu_view.hpp"

#include <algorithm>

#include "core/libm/libm.hpp"

namespace ge::ui {

namespace {

const Font& title_font(const MenuStyle& s) {
  return s.title_font ? *s.title_font : ui_font(Weight::semibold);
}
const Font& item_font(const MenuStyle& s) {
  return s.item_font ? *s.item_font : ui_font(Weight::semibold);
}
const Font& small_font(const MenuStyle& s) {
  return s.small_font ? *s.small_font : ui_font(Weight::medium);
}

float item_height(const Item& it, const MenuStyle& st) {
  if (it.kind == ItemKind::label) return st.small_size * 1.7f;
  return it.prominent ? st.prominent_height : st.item_height;
}

// The column's height at the reference scale.
float block_height(const Page& p, bool root, const MenuStyle& st) {
  float h = (root ? st.logo_size : st.title_size) * 1.25f + st.gap * 2;
  for (size_t i = 0; i < p.items.size(); i++) h += item_height(p.items[i], st) + (i ? st.gap : 0);
  return h + st.small_size * 2.2f;  // the hint under the column
}

float text_size(const Item& it, const MenuStyle& st) {
  if (it.kind == ItemKind::label) return st.small_size;
  return it.prominent ? st.prominent_size : st.item_size;
}

// The baseline that centres a line of `size` in a box from y to y + h.
float centred_baseline(const Font& f, float size, float y, float h) {
  const VMetrics m = f.metrics(size);
  // Capitals sit between the baseline and about 0.7 of the ascent: centre those, not the ascent.
  const float cap = m.ascent * 0.72f;
  return y + h / 2 + cap / 2;
}

}  // namespace

MenuLayout layout_menu(const Menu& menu, int width, int height, const MenuStyle& st, Rect safe) {
  MenuLayout L;
  L.width = width, L.height = height;
  L.safe = safe.w > 0 && safe.h > 0 ? safe : Rect{0, 0, float(width), float(height)};
  L.portrait = height > width;
  const Page& p = menu.top();
  const bool root = menu.at_root();
  float s = L.portrait ? L.safe.w / 440.0f : L.safe.h / 720.0f;
  s = std::max(s, 0.72f);
  const float margin_of_short = std::min(L.safe.w, L.safe.h) * 0.08f;
  float m = 0, avail = 0, help_h = 0;
  // Shrink until the column fits between the margins and the help line (twice settles it: the
  // margins scale too).
  for (int pass = 0; pass < 3; pass++) {
    m = std::min(st.margin * s, std::max(margin_of_short, 16.0f));
    help_h = st.small_size * s * 1.8f;
    avail = L.safe.h - 2 * m - help_h;
    const float need = block_height(p, root, st) * s;
    if (need <= avail) break;
    s *= avail / need;
  }
  L.scale = s;
  const float cw = L.portrait ? L.safe.w - 2 * m : std::min(st.column_width * s, L.safe.w * 0.46f);
  const float bh = block_height(p, root, st) * s;
  float x = st.side == Side::right && !L.portrait ? L.safe.right() - m - cw : L.safe.x + m;
  float y = L.portrait ? L.safe.bottom() - m - help_h - bh : L.safe.y + m + (avail - bh) / 2;
  L.column = {x, y, cw, bh};
  const float title_h = (root ? st.logo_size : st.title_size) * s * 1.25f;
  L.title = {x, y, cw, title_h};
  y += title_h + st.gap * 2 * s;
  L.items.clear();
  for (size_t i = 0; i < p.items.size(); i++) {
    if (i) y += st.gap * s;
    const float h = item_height(p.items[i], st) * s;
    L.items.push_back({x, y, cw, h});
    y += h;
  }
  L.hint = {x, y + st.gap * s, cw, st.small_size * s * 1.6f};
  const float line = st.small_size * s * 1.6f;
  L.help = {L.safe.x + m, L.safe.bottom() - m * 0.5f - line, L.safe.w / 2 - m, line};
  L.footer = {L.safe.x + L.safe.w / 2, L.help.y, L.safe.w / 2 - m, line};
  return L;
}

std::vector<std::string> layout_problems(const Menu& menu, const MenuLayout& L,
                                         const MenuStyle& st) {
  std::vector<std::string> out;
  const Page& p = menu.top();
  const float s = L.scale;
  auto outside = [&](Rect r) {
    return r.x < L.safe.x - 0.5f || r.y < L.safe.y - 0.5f || r.right() > L.safe.right() + 0.5f ||
           r.bottom() > L.safe.bottom() + 0.5f;
  };
  const bool root = menu.at_root();
  const std::string& title = root ? st.logo : p.title;
  const float title_w = title_font(st).advance(title, (root ? st.logo_size : st.title_size) * s);
  if (title_w > L.column.w + 0.5f)
    out.push_back("page '" + p.id + "': its title is " + std::to_string(int(title_w)) +
                  " px wide in a " + std::to_string(int(L.column.w)) +
                  " px column; shorten it or lower MenuStyle::logo_size/title_size");
  const float small = st.small_size * s;
  const float corners = small_font(st).advance(st.help, small, st.tracking * s * 0.5f) +
                        small_font(st).advance(st.footer, small) + 2 * st.pad * s;
  if (corners > L.footer.right() - L.help.x + 0.5f)
    out.push_back("the help and the footer need " + std::to_string(int(corners)) +
                  " px on one line of " + std::to_string(int(L.footer.right() - L.help.x)) +
                  " px; shorten MenuStyle::help or MenuStyle::footer");
  for (size_t i = 0; i < p.items.size() && i < L.items.size(); i++) {
    const Item& it = p.items[i];
    const Rect r = L.items[i];
    if (outside(r))
      out.push_back("page '" + p.id + "' item '" + it.id +
                    "': its slab leaves the safe area; the page has too many items for the screen");
    if (i && r.y < L.items[i - 1].bottom() - 0.5f)
      out.push_back("page '" + p.id + "' item '" + it.id + "' overlaps the item above it");
    if (it.kind == ItemKind::label) continue;
    const float size = text_size(it, st) * s;
    float need = item_font(st).advance(it.text, size, st.tracking * s) + 2 * st.pad * s;
    if (it.kind == ItemKind::slider) {
      need += r.w * 0.36f;
    } else if (it.kind != ItemKind::action && it.kind != ItemKind::back) {
      need += item_font(st).advance("< " + item_value(it, st.on, st.off) + " >", size) + st.pad * s;
    }
    if (need > r.w + 0.5f)
      out.push_back("page '" + p.id + "' item '" + it.id + "': its words need " +
                    std::to_string(int(need)) + " px in a " + std::to_string(int(r.w)) +
                    " px slab; shorten the text or widen MenuStyle::column_width");
  }
  return out;
}

void MenuView::frame(const Menu& menu, const MenuLayout& layout, double dt_s) {
  const Page& p = menu.top();
  if (p.id != page_) {
    page_ = p.id;
    page_in_ = 0;
    placed_ = false;
  }
  page_in_ = std::min(1.0f, page_in_ + static_cast<float>(dt_s / 0.22));
  if (p.focus < 0 || p.focus >= static_cast<int>(layout.items.size())) return;
  const Rect target = layout.items[static_cast<size_t>(p.focus)];
  if (!placed_) {
    glow_ = target;
    placed_ = true;
    return;
  }
  // Closes most of the way in a tenth of a second, whatever the frame rate.
  const auto k = static_cast<float>(1 - core::libm::exp(-dt_s * 30));
  glow_.x += (target.x - glow_.x) * k;
  glow_.y += (target.y - glow_.y) * k;
  glow_.w += (target.w - glow_.w) * k;
  glow_.h += (target.h - glow_.h) * k;
}

void MenuView::settle(const Menu& menu, const MenuLayout& layout) {
  page_ = menu.top().id;
  page_in_ = 1;
  placed_ = false;
  frame(menu, layout, 0);
}

void MenuView::draw(DrawList& dl, const Menu& menu, const MenuLayout& L,
                    const MenuStyle& st) const {
  const Page& p = menu.top();
  const float s = L.scale;
  const bool same_page = p.id == page_;
  const float in = same_page ? page_in_ : 1;
  // A page eases in: it fades up and slides the last few pixels from the column's outer side.
  const float ease = 1 - (1 - in) * (1 - in);
  const float dx = (1 - ease) * 28 * s * (st.side == Side::right && !L.portrait ? 1.0f : -1.0f);
  auto fade = [&](Color c) { return with_alpha(c, c.a * ease); };
  auto moved = [&](Rect r) { return Rect{r.x + dx, r.y, r.w, r.h}; };

  // The title: the game's name on the root page, the page's own above any other.
  const bool root = menu.at_root();
  const std::string& title = root ? st.logo : p.title;
  if (!title.empty()) {
    const float size = (root ? st.logo_size : st.title_size) * s;
    const Rect t = moved(L.title);
    const float base = t.y + t.h * 0.8f;
    const Font& f = title_font(st);
    dl.text(f, size, t.x + 2 * s, base + 3 * s, title, fade(st.shadow), Align::left,
            st.tracking * s);
    dl.text(f, size, t.x, base, title, fade(st.text), Align::left, st.tracking * s);
  }

  // The slabs, then the highlight over the focused one, then the words over both.
  for (size_t i = 0; i < p.items.size() && i < L.items.size(); i++) {
    const Item& it = p.items[i];
    if (it.kind == ItemKind::label) continue;
    Box b;
    b.rect = moved(L.items[i]);
    b.radius = st.radius * s;
    b.fill = solid(fade(st.slab));
    b.border = 1;
    b.border_color = fade(st.slab_border);
    dl.box(b);
  }
  const Item* focused = menu.focused();
  if (focused != nullptr && placed_ && same_page) {
    const Rect g = moved(glow_);
    Box shadow;
    shadow.rect = {g.x, g.y + 4 * s, g.w, g.h};
    shadow.radius = st.radius * s;
    shadow.fill = solid(fade(st.shadow));
    shadow.feather = 14 * s;
    dl.box(shadow);
    Box hi;
    hi.rect = g;
    hi.radius = st.radius * s;
    hi.fill = linear(0, 0, 0, g.h, fade(st.accent_hi), fade(st.accent));
    dl.box(hi);
  }
  const Font& f = item_font(st);
  for (size_t i = 0; i < p.items.size() && i < L.items.size(); i++) {
    const Item& it = p.items[i];
    const Rect r = moved(L.items[i]);
    const bool is_focus = focused == &it;
    if (it.kind == ItemKind::label) {
      const Font& sf = small_font(st);
      const float size = st.small_size * s;
      dl.text(sf, size, r.x, r.bottom() - size * 0.45f, it.text, fade(st.text_dim), Align::left,
              st.tracking * s);
      continue;
    }
    const Color c = fade(it.disabled ? st.text_disabled : is_focus ? st.text_focus : st.text);
    const float size = text_size(it, st) * s;
    const float base = centred_baseline(f, size, r.y, r.h);
    dl.text(f, size, r.x + st.pad * s, base, it.text, c, Align::left, st.tracking * s);
    const float right = r.right() - st.pad * s;
    if (it.kind == ItemKind::slider) {
      const float tw = r.w * 0.3f, th = std::max(3.0f, 5 * s);
      const float tx = right - tw, ty = r.y + r.h / 2 - th / 2;
      const float span = static_cast<float>(it.max - it.min);
      const float share = span > 0 ? static_cast<float>((it.value - it.min) / span) : 0;
      dl.rect({tx, ty, tw, th}, fade(st.track), th / 2);
      dl.rect({tx, ty, tw * share, th}, c, th / 2);
      Box knob;
      knob.rect = {tx + tw * share - th * 1.4f, ty - th * 0.9f, th * 2.8f, th * 2.8f};
      knob.radius = th * 1.4f;
      knob.fill = solid(c);
      dl.box(knob);
      dl.text(f, size * 0.85f, tx - st.pad * s * 0.6f, base, item_value(it, st.on, st.off), c,
              Align::right);
    } else if (it.kind == ItemKind::toggle || it.kind == ItemKind::choice) {
      const std::string v = item_value(it, st.on, st.off);
      const std::string shown = is_focus ? "< " + v + " >" : v;
      dl.text(f, size, right, base, shown, is_focus ? c : fade(st.text_dim), Align::right);
    }
  }

  // The focused item's hint under the column, and the small print in the corners.
  const Font& sf = small_font(st);
  const float small = st.small_size * s;
  if (focused != nullptr && !focused->hint.empty()) {
    const Rect h = moved(L.hint);
    dl.text(sf, small, h.x, h.y + small, focused->hint, fade(st.text_dim));
  }
  if (!st.help.empty())
    dl.text(sf, small, L.help.x, L.help.bottom() - small * 0.45f, st.help, st.text_dim, Align::left,
            st.tracking * s * 0.5f);
  if (!st.footer.empty())
    dl.text(sf, small, L.footer.right(), L.footer.bottom() - small * 0.45f, st.footer, st.text_dim,
            Align::right);
}

}  // namespace ge::ui
