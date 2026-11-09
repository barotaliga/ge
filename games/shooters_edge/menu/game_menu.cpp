// The main menu over the game: opening and closing, the presses, the two looks and the
// classic-enhance menu mounted under OPTIONS (game_menu.hpp).
//
// ge:summary The main menu over the game: open, close, presses, the looks, classic enhance
// ge:keywords menu game-menu escape togglemenu menu_look draw update enhance job pointer
// ge:keywords load-game enhance_game take_ended ce_auto screenshots gallery
// ge:skill ui console classicenhance
// ge:test tests/shooters_edge_menu_test.cpp
#include "shooters_edge/menu/game_menu.hpp"

#include <filesystem>
#include <system_error>
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
  enhance_ = ce::classic_menu({}, switches_);
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
  con.cvars().bind({.name = "ce_auto",
                    .fallback = "1",
                    .flags = ge::console::kArchive,
                    .help = "1: a game loaded (LOAD GAME, basedir) is enhanced in the background "
                            "at once; 0: only by CLASSIC ENHANCED > ENHANCE",
                    .kind = ge::console::CvarKind::boolean},
                   &auto_);
  ce::bind_switches(con.cvars(), &switches_);
  ce::sync_switches(enhance_, switches_);
  using CArgs = ge::console::Args;
  using Con = ge::console::Console;
  const auto opener = [this](std::string_view page) {  // a page's id, a literal
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
  con.add_command("menu_screenshots",
                  "menu_screenshots: the main menu opened at OPTIONS > SCREENSHOTS (the "
                  "screenshots F12 took)",
                  opener("screenshots"));
  con.add_command("menu_servers",
                  "menu_servers: the main menu opened at MULTIPLAYER > JOIN A GAME (the server "
                  "browser)",
                  opener("servers"));
  con.add_command("menu_demos",
                  "menu_demos: the main menu opened at DEMOS (recordings anywhere on the machine)",
                  opener("demos"));
  servers_.add_cvars(con);
  demos_.add_cvars(con);
}

void GameMenu::set_art(ClassicArt art) { art_ = std::move(art); }

void GameMenu::load_refused(std::string why) {
  load_note_ = std::move(why);
  if (ui::Page* p = main_.page("enhanced")) {
    if (ui::Item* it = p->find("load")) it->hint = "NOT LOADED: " + load_note_;
  }
}

void GameMenu::set_game(std::string basedir) {
  game_ = std::move(basedir);
  load_note_.clear();
  std::vector<std::string> one;
  if (!game_.empty()) one.push_back(game_);
  enhance_ = ce::classic_menu(one, switches_);
  pack_ = 0;
}

bool GameMenu::classic() const {
  if (!art_.loaded()) return false;
  return look_ == "classic" || look_ == "auto";
}

void GameMenu::show(Host& host, std::string_view page) {
  in_enhance_ = false;
  in_gallery_ = false;
  in_servers_ = false;
  in_demos_ = false;
  waiting_.clear();
  main_.reset();
  const bool gallery = page == "screenshots";
  if (gallery) page = "options";
  const bool servers = page == "servers";
  if (servers) page = "multi";
  const bool demos = page == "demos";
  if (demos) page = "main";
  if (page != "main") main_.open(page);
  refresh_main(host);
  main_.top().focus = -1;
  main_.move_focus(0);
  help_page_ = 0;
  open_ = true;
  if (gallery) {
    main_.top().focus = -1;
    main_.focus("screenshots");
    show_gallery();
  }
  if (servers) {
    main_.top().focus = -1;
    main_.focus("join");
    show_servers();
  }
  if (demos) {
    main_.top().focus = -1;
    main_.focus("demos");
    show_demos();
  }
}

void GameMenu::hide() {
  open_ = false;
  waiting_.clear();
  in_enhance_ = false;
  in_gallery_ = false;
  in_servers_ = false;
  in_demos_ = false;
}

void GameMenu::show_demos() {
  in_demos_ = true;
  demos_.open();
}

void GameMenu::update_demos(const ui::MenuInput& in, Host& host) {
  switch (demos_.input(in)) {
    case DemosAction::leave:
      in_demos_ = false;
      refresh_main(host);
      return;
    case DemosAction::play:
      demo_ask_ = demos_.chosen();
      if (con_ != nullptr) con_->print("demos: " + demo_ask_ + ": " + demos_.facts());
      hide();
      return;
    case DemosAction::none:
      return;
  }
}

void GameMenu::show_servers() {
  in_servers_ = true;
  std::string why;
  if (!servers_.open(&why) && con_ != nullptr) con_->print("join a game: " + why);
}

void GameMenu::update_servers(const ui::MenuInput& in, Host& host) {
  servers_.frame(dt_);
  switch (servers_.input(in, in.refresh)) {
    case ServersAction::leave:
      in_servers_ = false;
      refresh_main(host);
      return;
    case ServersAction::join:
      if (host.console != nullptr) host.console->execute("connect " + servers_.chosen());
      hide();
      return;
    case ServersAction::none:
      return;
  }
}

void GameMenu::show_gallery() {
  in_gallery_ = true;
  gallery_.scan();  // a screenshot taken since the last look
  gallery_.reset();
}

void GameMenu::update_gallery(const ui::MenuInput& in, Host& host) {
  ge::capture::GalleryInput gi;
  gi.up = in.up;
  gi.down = in.down;
  gi.left = in.left;
  gi.right = in.right;
  gi.accept = in.accept;
  gi.back = in.back;
  gi.del = in.del;
  if (in.pointer_moved || in.click) gi.pointed = gallery_page_.hit(in.pointer_x, in.pointer_y);
  gi.click = in.click && gi.pointed >= 0;
  if (gallery_.input(gi) == ge::capture::GalleryAction::leave) {
    in_gallery_ = false;
    refresh_main(host);
    return;
  }
  gallery_.poll();
}

void GameMenu::refresh_main(const Host& host) {
  refresh(main_, host, classic());
  ui::Page* p = main_.page("enhanced");
  if (p == nullptr) return;
  // ENHANCE has nothing to enhance until a game is loaded; LOAD GAME says why one was not.
  if (ui::Item* it = p->find("enhance"); it != nullptr && game_.empty()) it->disabled = true;
  if (ui::Item* it = p->find("load")) {
    const Entry* e = find_entry("enhanced", "load", table_of(host));
    it->hint = !load_note_.empty() ? "NOT LOADED: " + load_note_
               : e != nullptr      ? std::string(e->hint)
                                   : it->hint;
  }
}

ce::BoardView GameMenu::board() const {
  if (start_error_.empty() && job_) return job_->view();
  ce::BoardView refused;  // a run that did not start shows why, as a failed run does
  refused.failed = true;
  refused.error = start_error_;
  return refused;
}

void GameMenu::watch_enhancing() {
  in_enhance_ = true;
  enhance_.reset();
  enhance_.open("enhancing");
  ce::sync_switches(enhance_, switches_);
  ce::sync_enhancing(enhance_, board());
  enhance_.top().focus = -1;
  enhance_.move_focus(0);
}

bool GameMenu::enhance_game(bool watch, std::string* error) {
  start_error_.clear();
  const auto refuse = [&](std::string why) {
    start_error_ = std::move(why);
    *error = start_error_;
    if (watch && open_) watch_enhancing();
    return false;
  };
  if (game_.empty())
    return refuse("no game is loaded; next: OPTIONS > CLASSIC ENHANCED > LOAD GAME");
  if (job_ && job_->running()) {
    if (job_game_ == game_) {
      if (watch && open_) watch_enhancing();
      return true;
    }
    job_.reset();  // a run of another install: stopped after the item it is on
  }
  // A path that is not there is refused at once, with the reason the open gives (no pack is read
  // for it); an install is opened and hashed on the run's own thread.
  if (std::error_code ec; !std::filesystem::exists(game_, ec)) {
    ce::Source src;
    std::string why;
    if (!ce::open_source(game_, &src, &why)) return refuse(why);
  }
  const std::filesystem::path root = ce::default_cache_root();
  if (root.empty()) return refuse("no cache folder on this machine; next: set GE_CLASSIC_CACHE");
  ce::RunOptions opts;
  ce::apply_bakes(switches_, &opts);
  job_ = std::make_unique<ce::EnhanceJob>();
  // A pack that does not open fails the run's first stage, its reason on the board.
  job_->start_at(game_, root, std::move(opts));
  job_game_ = game_;
  job_live_ = true;
  if (watch && open_) watch_enhancing();
  return true;
}

bool GameMenu::take_ended() {
  if (!job_ || !job_live_ || job_->running()) return false;
  job_live_ = false;
  return true;
}

void GameMenu::apply_enhance(const ui::MenuEvent& e, Host& host) {
  if (con_ == nullptr) return;
  switch (ce::apply_menu_event(e, enhance_, con_->cvars(), &switches_, &pack_)) {
    case ce::MenuAction::back:
      in_enhance_ = false;
      refresh_main(host);
      break;
    case ce::MenuAction::enhance:
      if (std::string why; !enhance_game(true, &why)) con_->print("classic enhance: " + why);
      break;
    case ce::MenuAction::cancel:
      if (job_) job_->cancel();
      break;
    case ce::MenuAction::play:  // the game is loaded already, the world drawn from the cache
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
  refresh_main(host);
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
  if (in_gallery_) {
    update_gallery(in, host);
    return;
  }
  if (in_servers_) {
    update_servers(in, host);
    return;
  }
  if (in_demos_) {
    update_demos(in, host);
    return;
  }
  if (in_enhance_) ce::sync_enhancing(enhance_, board());
  if (in_enhance_) {
    for (const ui::MenuEvent& e : enhance_.input(in, hits_)) apply_enhance(e, host);
    // BACK from the enhancing page: the game's own page, not classic-enhance's pack choice.
    if (in_enhance_ && enhance_.top().id == "classic") {
      in_enhance_ = false;
      refresh_main(host);
    }
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
      case Outcome::load_game:
        load_asked_ = true;
        return;
      case Outcome::screenshots:
        show_gallery();
        return;
      case Outcome::servers:
        show_servers();
        return;
      case Outcome::demos:
        show_demos();
        return;
      case Outcome::classic_enhance:
        if (std::string why; !enhance_game(true, &why) && con_ != nullptr)
          con_->print("classic enhance: " + why);
        return;
      case Outcome::none:
        break;
    }
    if (e.kind == ui::EventKind::opened) {
      help_page_ = 0;
      refresh_main(host);
    }
  }
}

void GameMenu::draw(ui::DrawList& list, int width, int height, double t) {
  problems_.clear();
  if (!open_) return;
  if (in_gallery_) {
    gallery_page_.draw(list, gallery_, width, height, style_);
    problems_ = gallery_page_.problems();
    return;
  }
  if (in_servers_) {
    servers_.draw(list, width, height, style_);
    problems_ = servers_.problems();
    return;
  }
  if (in_demos_) {
    demos_.draw(list, width, height, style_);
    problems_ = demos_.problems();
    return;
  }
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
  if (in_enhance_ && m.top().id == "enhancing" && (job_ || !start_error_.empty())) {
    ce::PanelStyle ps;
    ps.scale = layout.scale;
    ce::draw_progress(list, board(), ce::panel_area(layout), ps, thumbs_,
                      start_error_.empty() ? job_game_ : game_);
  }
}

}  // namespace shooters_edge::menu
