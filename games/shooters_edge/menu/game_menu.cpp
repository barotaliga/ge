// The main menu over the game: opening and closing, the presses, the two looks and the
// classic-enhance menu mounted under OPTIONS (game_menu.hpp).
//
// ge:summary The main menu over the game: open, close, presses, the looks, classic enhance
// ge:keywords menu game-menu escape togglemenu menu_look draw update enhance job pointer
// ge:keywords pushmenu popmenu forcemenuoff
// ge:keywords load-game enhance_game take_ended ce_auto screenshots gallery
// ge:skill ui console classicenhance
// ge:test tests/shooters_edge_menu_test.cpp
#include "shooters_edge/menu/game_menu.hpp"

#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "classicenhance/cache.hpp"
#include "classicenhance/source.hpp"
#include "console/cvar.hpp"
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
  con.cvars().bind({.name = "menu_attract",
                    .fallback = "auto",
                    .flags = ge::console::kArchive,
                    .help = "The recordings a classic game plays in turn behind its main menu.",
                    .kind = ge::console::CvarKind::text,
                    .values = "auto: the game's own (the default); \"\": none, the title's night "
                              "sky; TEXT: these recordings, by name"},
                   &attract_);
  con.cvars().bind({.name = "menu_look",
                    .fallback = "auto",
                    .flags = ge::console::kArchive,
                    .help = "The main menu's look.",
                    .kind = ge::console::CvarKind::text,
                    .values = "auto: the pack's own pictures when the packs have them (the "
                              "default); classic: the pack's; modern: ours"},
                   &look_);
  con.cvars().bind({.name = "ce_auto",
                    .fallback = "1",
                    .flags = ge::console::kArchive,
                    .help = "Whether a game loaded by LOAD GAME or basedir is enhanced in the "
                            "background at once.",
                    .kind = ge::console::CvarKind::boolean,
                    .values = "0: only by CLASSIC ENHANCED > ENHANCE; 1: at once (the default)"},
                   &auto_);
  // The picture's switches too (classic/app/look.cpp registers them first): one setting each, so a
  // cfg's or the console's ce_ line reaches the menu's and the bake's copy as well.
  ce::bind_switches(con.cvars(), &switches_, ge::console::Overlap::same);
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
  // Quake's own words for the other pages (and Quake II's where they share one).
  con.add_command("menu_singleplayer", "menu_singleplayer: the main menu opened at SINGLE PLAYER",
                  opener("single"));
  con.add_command("menu_load", "menu_load: the main menu opened at LOAD", opener("load"));
  con.add_command("menu_save", "menu_save: the main menu opened at SAVE", opener("save"));
  con.add_command("menu_multiplayer", "menu_multiplayer: the main menu opened at MULTIPLAYER",
                  opener("multi"));
  con.add_command("menu_setup", "menu_setup: the main menu opened at MULTIPLAYER > SETUP",
                  opener("setup"));
  con.add_command("menu_keys", "menu_keys: the main menu opened at OPTIONS > CUSTOMIZE CONTROLS",
                  opener("controls"));
  con.add_command("menu_video", "menu_video: the main menu opened at OPTIONS > VIDEO",
                  opener("video"));
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
  con.add_command("menu_games",
                  "menu_games: the main menu opened at SELECT GAME (the classic games and their "
                  "installs)",
                  opener("games"));
  // pushmenu <name>: a page by its name, ours or Quake II's; popmenu one page back; forcemenuoff
  // every page closed.
  con.add_command("pushmenu", "pushmenu <page>: the main menu opened at that page",
                  [this](Con& c, const CArgs& a) {
                    if (a.size() < 2) {
                      c.print("Usage: " + a[0] + " <menu>");
                      return;
                    }
                    // A page the game's table has by that name first, then Quake II's names
                    // for ours.
                    static constexpr std::pair<std::string_view, std::string_view> kNames[] = {
                        {"singleplayer", "single"}, {"game", "single"},
                        {"loadgame", "load"},       {"savegame", "save"},
                        {"multiplayer", "multi"},   {"startserver", "host"},
                        {"players", "setup"},       {"keys", "controls"},
                        {"dmflags", "gameflags"},   {"legacy", "legacykeys"}};
                    std::string page = ge::console::lower(a[1]);
                    if (main_.page(page) == nullptr)
                      for (const auto& [name, ours] : kNames)
                        if (page == name) page = ours;
                    const bool special = page == "servers" || page == "demos" ||
                                         page == "screenshots" || page == "games";
                    if (!special && main_.page(page) == nullptr) {
                      c.print("No such menu: " + a[1]);
                      return;
                    }
                    toggle_ = true;
                    toggle_open_ = true;
                    toggle_page_ = page;
                  });
  con.add_command("popmenu", "popmenu: the main menu one page back (closed from its root)",
                  [this](Con&, const CArgs&) { pending_ = 1; });
  con.add_command("forcemenuoff", "forcemenuoff: the main menu closed, whatever page is up",
                  [this](Con&, const CArgs&) { pending_ = 2; });
  servers_.add_cvars(con);
  demos_.add_cvars(con);
}

void GameMenu::set_art(ClassicArt art) { art_ = std::move(art); }

void GameMenu::load_refused(std::string why) {
  games_.set_note(why);
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
  if (!art_.menu_ready()) return false;
  return look_ == "classic" || look_ == "auto";
}

void GameMenu::show(Host& host, std::string_view page) {
  in_enhance_ = false;
  in_gallery_ = false;
  in_servers_ = false;
  in_demos_ = false;
  in_games_ = false;
  waiting_.clear();
  main_.reset();
  const bool gallery = page == "screenshots";
  if (gallery) page = "options";
  const bool servers = page == "servers";
  if (servers) page = "multi";
  const bool demos = page == "demos";
  if (demos) page = "main";
  const bool games = page == "games";
  if (games) page = "main";
  if (page != "main") main_.open(page);
  refresh_main(host);
  main_.top().focus = -1;
  main_.move_focus(0);
  help_page_ = 0;
  open_ = true;
  cue(ui::MenuCue::enter);
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
  if (games) {
    main_.top().focus = -1;
    main_.focus("games");
    show_games();
  }
}

void GameMenu::hide() {
  if (open_) cue(ui::MenuCue::back);
  open_ = false;
  waiting_.clear();
  in_enhance_ = false;
  in_gallery_ = false;
  in_servers_ = false;
  in_demos_ = false;
  in_games_ = false;
}

void GameMenu::show_games() {
  cue(ui::MenuCue::enter);
  in_games_ = true;
  games_.open();
}

void GameMenu::update_games(const ui::MenuInput& in, Host& host) {
  if (in.up || in.down) cue(ui::MenuCue::move);
  switch (games_.input(in)) {
    case GamesAction::leave:
      cue(ui::MenuCue::back);
      in_games_ = false;
      refresh_main(host);
      return;
    case GamesAction::open:
      game_ask_ = {.asked = true,
                   .browse = false,
                   .id = games_.chosen().id,
                   .install = games_.chosen().install};
      cue(ui::MenuCue::enter);
      return;
    case GamesAction::browse:
      cue(ui::MenuCue::enter);
      game_ask_ = {.asked = true, .browse = true, .id = games_.chosen().id, .install = {}};
      return;
    case GamesAction::none:
      return;
  }
}

void GameMenu::show_demos() {
  cue(ui::MenuCue::enter);
  in_demos_ = true;
  demos_.open();
}

void GameMenu::update_demos(const ui::MenuInput& in, Host& host) {
  if (in.up || in.down || in.left || in.right) cue(ui::MenuCue::move);
  switch (demos_.input(in)) {
    case DemosAction::leave:
      cue(ui::MenuCue::back);
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
  cue(ui::MenuCue::enter);
  in_servers_ = true;
  std::string why;
  if (!servers_.open(&why) && con_ != nullptr) con_->print("join a game: " + why);
}

void GameMenu::update_servers(const ui::MenuInput& in, Host& host) {
  servers_.frame(dt_);
  if (in.up || in.down || in.left || in.right) cue(ui::MenuCue::move);
  switch (servers_.input(in, in.refresh)) {
    case ServersAction::leave:
      cue(ui::MenuCue::back);
      in_servers_ = false;
      refresh_main(host);
      return;
    case ServersAction::join:
      if (host.console != nullptr && servers_.finds_routes())  // sb_findroutes
        servers_.connect_by_route(*host.console, servers_.chosen(), false);
      else if (host.console != nullptr)
        host.console->execute("connect " + servers_.chosen());
      if (servers_.hide_on_join()) hide();  // sb_autohide
      return;
    case ServersAction::none:
      return;
  }
}

void GameMenu::show_gallery() {
  cue(ui::MenuCue::enter);
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
  if (in.up || in.down || in.left || in.right) cue(ui::MenuCue::move);
  if (in.accept || gi.click) cue(ui::MenuCue::enter);
  if (gallery_.input(gi) == ge::capture::GalleryAction::leave) {
    cue(ui::MenuCue::back);
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
    return refuse("No game is loaded. Load one in OPTIONS > CLASSIC ENHANCED > LOAD GAME.");
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
  if (root.empty())
    return refuse("There is no folder on this computer to keep the enhanced files in.");
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
  const std::string command(bind_command(*table_, waiting_page_, waiting_));
  waiting_.clear();
  cue(key == "escape" ? ui::MenuCue::back : ui::MenuCue::change);
  if (!command.empty() && (key == "backspace" || key == "del")) {
    unbind_control(*host.console, command);
  } else if (!command.empty() && key != "escape") {
    if (!bind_control(*host.console, command, key))
      host.console->print("controls: '" + std::string(key) + "' names no key to bind");
  }
  refresh_main(host);
}

void GameMenu::update(const ui::MenuInput& in, bool escape, Host& host, double dt) {
  dt_ = dt;
  waiting_fresh_ = false;
  if (!waiting_.empty()) return;  // the keys are press_key's until one comes
  if (pending_ != 0) {
    const int p = pending_;
    pending_ = 0;
    if (p == 2 || !open_) {
      if (open_) hide();
      return;
    }
    update(ui::MenuInput{.back = true}, false, host, dt);
    return;
  }
  if (toggle_) {
    toggle_ = false;
    const bool push = toggle_open_;
    toggle_open_ = false;
    if (open_ && toggle_page_ == "main" && !push)
      hide();
    else if (toggle_page_ == "controls" && main_.page("keys") != nullptr)
      show(host, "keys");  // a game whose menu has its own key page (Quake II's)
    else
      show(host, toggle_page_);
    return;
  }
  if (!open_) {
    if (escape && host.in_level && host.screen_up && host.console != nullptr) {
      host.console->execute("putaway");  // the rules' screen taken down, no menu
      return;
    }
    if (escape) {  // in a level, the game's in-game page when it has one
      const std::string_view in_level = table_of(host).in_level;
      show(host, host.in_level && !in_level.empty() && main_.page(in_level) != nullptr
                     ? in_level
                     : std::string_view("main"));
    }
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
  if (in_games_) {
    update_games(in, host);
    return;
  }
  if (in_enhance_) ce::sync_enhancing(enhance_, board());
  // The focus where it was before the presses: moved (by a key or the pointer) is a click.
  ui::Menu& driven = in_enhance_ ? enhance_ : main_;
  const std::string page_before = driven.top().id;
  const int focus_before = driven.top().focus;
  const auto moved = [&] {
    return driven.top().id == page_before && driven.top().focus != focus_before;
  };
  if (in_enhance_) {
    const std::vector<ui::MenuEvent> events = enhance_.input(in, hits_);
    cue(ui::menu_cue(events, moved()));
    for (const ui::MenuEvent& e : events) apply_enhance(e, host);
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
    cue(ui::MenuCue::move);
    return;
  }
  const std::vector<ui::MenuEvent> events = main_.input(in, hits_);
  cue(ui::menu_cue(events, moved()));
  for (const ui::MenuEvent& e : events) {
    switch (apply_event(e, main_, host)) {
      case Outcome::close:
      case Outcome::back_at_root:
        hide();
        return;
      case Outcome::bind_key:
        waiting_ = e.item;
        waiting_page_ = e.page;
        waiting_fresh_ = true;
        if (ui::Item* it = main_.top().find(e.item); it != nullptr) {
          it->choices = {"PRESS A KEY"};
          if (e.page != "controls") it->hint = "Press the desired key, Escape to cancel";
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
      case Outcome::games:
        show_games();
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
    if (classic())
      servers_.draw_classic(list, art_, width, height, t);
    else
      servers_.draw(list, width, height, style_);
    problems_ = servers_.problems();
    return;
  }
  if (in_demos_) {
    if (classic())
      demos_.draw_classic(list, art_, width, height, t);
    else
      demos_.draw(list, width, height, style_);
    problems_ = demos_.problems();
    return;
  }
  if (in_games_) {
    if (classic())
      games_.draw_classic(list, art_, width, height, t);
    else
      games_.draw(list, width, height, style_);
    problems_ = games_.problems();
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
