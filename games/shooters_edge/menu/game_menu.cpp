// The main menu over the game: opening and closing, the presses, the two looks and the
// classic-enhance menu mounted under OPTIONS (game_menu.hpp).
//
// ge:summary The main menu over the game: open, close, presses, the looks, classic enhance
// ge:keywords menu game-menu escape togglemenu menu_look draw update enhance job pointer
// ge:skill ui console classicenhance
// ge:test tests/shooters_edge_menu_test.cpp
#include "shooters_edge/menu/game_menu.hpp"

#include <algorithm>
#include <utility>

#include "classicenhance/cache.hpp"
#include "classicenhance/source.hpp"
#include "core/output.hpp"

namespace shooters_edge::menu {

namespace {

namespace ui = ge::ui;
namespace ce = ge::classicenhance;

ui::MenuStyle modern_style() {
  ui::MenuStyle s;
  s.logo = "SHOOTER'S EDGE";
  s.help = "ENTER SELECT   ESC BACK";
  s.footer = "DIMMED: NOT IN THE GAME YET";
  s.accent = ui::hex(0xb5542a);
  s.accent_hi = ui::hex(0xd8703c);
  s.slab = ui::hex(0x15100c, 0.62f);
  return s;
}

}  // namespace

GameMenu::GameMenu() : main_(build_menu()), style_(modern_style()) {
  enhance_ = ce::classic_menu(packs_, switches_);
}

GameMenu::~GameMenu() = default;

void GameMenu::add_cvars(ge::console::Console& con) {
  con_ = &con;
  con.cvars().bind({.name = "menu_look",
                    .fallback = "auto",
                    .flags = ge::console::kArchive,
                    .help = "the main menu's look: auto (the pack's own pictures when the packs "
                            "have them), classic or modern",
                    .kind = ge::console::CvarKind::text},
                   &look_);
  ce::bind_switches(con.cvars(), &switches_);
  ce::sync_switches(enhance_, switches_);
  using CArgs = ge::console::Args;
  using Con = ge::console::Console;
  const auto opener = [this](const std::string& page) {
    return [this, page](Con&, const CArgs&) {
      toggle_ = true;
      toggle_page_ = page;
    };
  };
  con.add_command("togglemenu", "togglemenu: the main menu open or closed (the Escape key)",
                  opener("main"));
  con.add_command("menu_main", "menu_main: the main menu opened at its root", opener("main"));
  con.add_command("menu_options", "menu_options: the main menu opened at OPTIONS",
                  opener("options"));
  con.add_command("menu_quit", "menu_quit: the main menu opened at QUIT", opener("quit"));
}

void GameMenu::set_art(ClassicArt art) { art_ = std::move(art); }

void GameMenu::set_packs(std::vector<std::string> paths) {
  packs_ = std::move(paths);
  enhance_ = ce::classic_menu(packs_, switches_);
  pack_ = 0;
}

bool GameMenu::classic() const {
  if (!art_.loaded()) return false;
  return look_ == "classic" || look_ == "auto";
}

void GameMenu::show(Host& host, std::string_view page) {
  in_enhance_ = false;
  waiting_.clear();
  main_.reset();
  if (page != "main") main_.open(page);
  refresh(main_, host, classic());
  main_.top().focus = -1;
  main_.move_focus(0);
  help_page_ = 0;
  open_ = true;
}

void GameMenu::hide() {
  open_ = false;
  waiting_.clear();
  in_enhance_ = false;
}

void GameMenu::start_enhance() {
  if (packs_.empty() || (job_ && job_->running())) return;
  const size_t at = static_cast<size_t>(std::clamp(pack_, 0, static_cast<int>(packs_.size()) - 1));
  ce::Source src;
  std::string error;
  if (!ce::open_source(packs_[at], &src, &error)) {
    if (con_) con_->print("classic enhance: " + error);
    return;
  }
  const std::filesystem::path root = ce::default_cache_root();
  if (root.empty()) {
    if (con_) con_->print("classic enhance: no cache folder on this machine; set GE_CLASSIC_CACHE");
    return;
  }
  ce::RunOptions opts;
  ce::apply_bakes(switches_, &opts);
  job_ = std::make_unique<ce::EnhanceJob>();
  job_->start(std::move(src), root, std::move(opts));
}

void GameMenu::apply_enhance(const ui::MenuEvent& e, Host& host) {
  if (con_ == nullptr) return;
  switch (ce::apply_menu_event(e, enhance_, con_->cvars(), &switches_, &pack_)) {
    case ce::MenuAction::back:
      in_enhance_ = false;
      refresh(main_, host, classic());
      break;
    case ce::MenuAction::enhance:
      start_enhance();
      break;
    case ce::MenuAction::cancel:
      if (job_) job_->cancel();
      break;
    case ce::MenuAction::play:
      con_->execute("map start");
      hide();
      break;
    case ce::MenuAction::none:
      break;
  }
}

void GameMenu::press_key(std::string_view key, Host& host) {
  if (waiting_.empty() || waiting_fresh_ || host.console == nullptr) return;
  const Control* c = control_of(waiting_);
  waiting_.clear();
  if (c != nullptr && (key == "backspace" || key == "del")) {
    unbind_control(*host.console, c->command);
  } else if (c != nullptr && key != "escape") {
    if (!bind_control(*host.console, c->command, key))
      host.console->print("controls: '" + std::string(key) + "' names no key to bind");
  }
  refresh(main_, host, classic());
}

void GameMenu::update(const ui::MenuInput& in, bool escape, Host& host, double dt) {
  dt_ = dt;
  waiting_fresh_ = false;
  if (!waiting_.empty()) return;  // the keys are press_key's until one comes
  if (toggle_) {
    toggle_ = false;
    if (open_ && toggle_page_ == "main")
      hide();
    else
      show(host, toggle_page_);
    return;
  }
  if (!open_) {
    if (escape) show(host);
    return;
  }
  if (job_) ce::sync_enhancing(enhance_, job_->view());
  if (in_enhance_) {
    for (const ui::MenuEvent& e : enhance_.input(in, hits_)) apply_enhance(e, host);
    return;
  }
  const bool help = classic() && main_.top().id == "help" && help_pages(art_) > 0;
  if (help && (in.left || in.right || in.accept)) {
    help_page_ += in.left ? -1 : 1;
    return;
  }
  for (const ui::MenuEvent& e : main_.input(in, hits_)) {
    switch (apply_event(e, main_, host)) {
      case Outcome::close:
      case Outcome::back_at_root:
        hide();
        return;
      case Outcome::bind_key:
        waiting_ = e.item;
        waiting_fresh_ = true;
        if (ui::Item* it = main_.top().find(e.item); it != nullptr) {
          it->choices = {"PRESS A KEY"};
          it->choice = 0;
        }
        return;
      case Outcome::classic_enhance:
        in_enhance_ = true;
        enhance_.reset();
        ce::sync_switches(enhance_, switches_);
        enhance_.top().focus = -1;
        enhance_.move_focus(0);
        return;
      case Outcome::none:
        break;
    }
    if (e.kind == ui::EventKind::opened) {
      help_page_ = 0;
      refresh(main_, host, classic());
    }
  }
}

void GameMenu::draw(ui::DrawList& list, int width, int height, double t) {
  problems_.clear();
  if (!open_) return;
  if (classic() && !in_enhance_) {
    const ClassicLayout l = layout_classic(main_, art_, width, height);
    hits_ = l.items;
    problems_ = classic_problems(main_, l, width, height);
    draw_classic(list, main_, art_, l, t, help_page_);
    return;
  }
  ui::Menu& m = in_enhance_ ? enhance_ : main_;
  list.rect({0, 0, static_cast<float>(width), static_cast<float>(height)},
            ui::hex(0x000000, 0.55f));
  const ui::MenuLayout layout = ui::layout_menu(m, width, height, style_);
  hits_ = layout.items;
  problems_ = ui::layout_problems(m, layout, style_);
  if (settle_)
    view_.settle(m, layout);
  else
    view_.frame(m, layout, dt_);
  settle_ = false;
  view_.draw(list, m, layout, style_);
  if (in_enhance_ && m.top().id == "enhancing" && job_) {
    ce::PanelStyle ps;
    ps.scale = layout.scale;
    ce::draw_progress(list, job_->view(), ce::panel_area(layout), ps, thumbs_,
                      packs_.empty() ? std::string() : packs_[static_cast<size_t>(pack_)]);
  }
}

}  // namespace shooters_edge::menu
