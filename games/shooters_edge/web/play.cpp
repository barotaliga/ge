// The web viewer's play mode (play.hpp): the listen server in the page, the canvas's keys and
// mouse its commands, the window's frame drawn on the canvas.
//
// ge:summary The web viewer's play mode defined: the match, the bots, the controls, the frame
// ge:keywords web play play-in-browser listen_advance take_play_input hud scores
// ge:skill net shooter
#include "shooters_edge/web/play.hpp"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gpu/device.hpp"
#include "gpu/surface.hpp"
#include "input/state.hpp"
#include "platform/window.hpp"
#include "shooters_edge/app/effects.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/client/classic_pictures.hpp"
#include "shooters_edge/client/controls.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/held.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/listen_sound.hpp"
#include "shooters_edge/client/lobby_console.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/scores.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "shooters_edge/client/title.hpp"
#include "shooters_edge/client/touch_play.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/hud/layout.hpp"
#include "shooters_edge/hud/lobby_page.hpp"
#include "shooters_edge/hud/lobby_panel.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "ui/draw.hpp"
#include "web/browser.hpp"
#include "web/page_audio.hpp"

namespace shooters_edge::web {

namespace {

namespace cl = client;
namespace se = hud;

// The keys the default moves leave out (USB HID usages, as the page reports them).
constexpr uint16_t kEnter = 40, kTab = 43, kF1 = 58, kF2 = 59, kF4 = 61, kF5 = 62, kF6 = 63,
                   kRight = 79, kLeft = 80, kDown = 81, kUp = 82;

// The page's mouse moves in the browser's pixels, about two of a desktop mouse's raw counts.
constexpr double kPageSensitivity = 0.12;  // degrees a pixel

int fail(const std::string& line, const std::string& detail = "") {
  if (!detail.empty()) std::fprintf(stderr, "shooters_edge_viewer: %s\n", detail.c_str());
  ge::web::say(line);
  return 1;
}

}  // namespace

int play_in_page(const ge::web::ViewerArgs& args, const cl::SearchPath& files,
                 const char* canvas_id) {
  std::string error;
  ge::web::say("Starting the game...");
  ge::progs::Progs progs;
  if (!cl::baseline_progs(progs, error))
    return fail("The game did not start here. Play it in the game below.", error);
  ge::web::say("Loading " + args.play + "...");
  std::unique_ptr<cl::LoadedMap> first = cl::load_named_map(files, args.play, error);
  if (!first)
    return fail("This hub does not have the map " + args.play + ", so it cannot be played here.",
                error);

  cl::Listen l;
  cl::ListenOptions opt = net::listen_options();
  opt.files = &files;
  opt.lobby = true;
  opt.lobby_settings.min_players = 1;
  opt.lobby_settings.countdown_s = 3;
  opt.lobby_settings.warm_on_join = true;
  if (!cl::listen_start(l, progs, std::move(first), opt, error))
    return fail("The game did not start on " + args.play + ". Play it in the game below.", error);
  // The game heard on the page (web/page_audio.hpp), as the window hears its own game.
  ge::audio::Synth synth;
  cl::ListenHearing heard(synth);
  ge::web::audio_open();
  for (int i = 0; i < args.bots; i++) cl::listen_lobby(l, "addbot average");
  cl::listen_lobby(l, "warmup");
  cl::listen_lobby(l, "ready");

  ge::web::say("Starting the picture...");
  std::unique_ptr<ge::gpu::Device> device = ge::gpu::create_device();
  if (!device) return fail("This browser cannot draw the game. Play it in the game below.");
  ge::platform::NativeWindow canvas;
  canvas.kind = ge::platform::NativeWindow::Kind::canvas;
  canvas.canvas = canvas_id;
  ge::gpu::Surface surface;
  if (!surface.create(*device, canvas))
    return fail("This browser cannot draw the game. Play it in the game below.");
  uint32_t w = 0, h = 0;
  ge::web::canvas_size(canvas_id, w, h);
  if (w == 0 || h == 0) return fail("The picture has no room on this page.");
  surface.configure(*device, w, h);
  const int iw = static_cast<int>(w), ih = static_cast<int>(h);

  const cl::GameAdapter& game = cl::classic_adapter();
  const look::Picture look;
  std::vector<ge::viewmodel::HeldModel> weapons;
  // The packs' own held weapons (a classic game is played with its own things alone).
  bool at_eye = game.held_weapons(game, files, weapons, error);
  if (!at_eye)
    std::fprintf(stderr, "shooters_edge_viewer: no weapon is drawn: %s\n", error.c_str());
  cl::ClassicPictures pictures = cl::load_classic_pictures(files, false, {}, &game);
  pictures.hud.finish();
  se::HudArt free_art = se::free_art();
  free_art.finish();
  const se::HudArt& art = pictures.hud.bar.empty() ? free_art : pictures.hud;
  se::Hud hud = se::make_hud();
  menu::BoardView board;

  cl::FxState fx_state;
  {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> pal = cl::find_file(files, "gfx/palette.lmp", store);
    fx_state.palette.assign(pal.begin(), pal.end());
  }
  ge::particles::Book book;
  {
    std::vector<std::string> faults;
    cl::load_effects(files, book, faults);
  }
  cl::ControlSettings controls;
  controls.sensitivity = kPageSensitivity;
  ge::input::InputState in;
  // A phone's or a tablet's screen (touch_play.hpp), laid out for the canvas.
  cl::TouchPlay touch = cl::touch_play(static_cast<float>(w) / static_cast<float>(std::max(h, 1u)));
  cl::ViewState shown;
  cl::HeldState held;
  held.hand.at_eye = at_eye;
  const view::ViewSettings view_settings;
  cl::FxFrame fx;
  cl::WorldDrawPtr world, night;  // the map's, and the main menu's night behind the lobby
  const cl::LoadedMap* drawn = nullptr;
  size_t world_models = 0;
  int told = -1;  // the status line's last word on the pointer: 1 locked, 0 not

  double t = 0;
  double last_ms = ge::web::next_frame();
  for (uint64_t frame = 0;; frame++) {
    const double now_ms = ge::web::next_frame();
    const double dt = std::min((now_ms - last_ms) / 1000.0, cl::kMostPlayStep);
    last_ms = now_ms;
    t += dt;

    // A map the server asks for: the page has one map's files, so another plays this one again.
    if (!l.pending_map.empty()) {
      std::string name = l.pending_map;
      l.pending_map.clear();
      std::unique_ptr<cl::LoadedMap> next = cl::load_named_map(files, name, error);
      if (!next) next = cl::load_named_map(files, args.play, error);
      if (!next || !cl::listen_change(l, std::move(next), error))
        return fail("The next map did not start. Reload the page to play again.", error);
      shown = {};
      fx_state.models = nullptr;
      held = {};
      held.hand.at_eye = at_eye;
    }

    const bool locked = ge::web::take_play_input(canvas_id, in);
    if (!locked) in.mouse.dx = in.mouse.dy = 0;
    if (told != (locked || touch.seen ? 1 : 0)) {  // a thumb on the screen needs no click
      told = locked || touch.seen ? 1 : 0;
      ge::web::say(told != 0 ? "" : "Click the picture to play. Escape lets the mouse go.");
    }
    // The lobby's keys: F1 ready or break, F2 the other team, F4 the warmup or the lobby's screen;
    // on the screen F5 and F6 a bot more or less, ENTER the start.
    const auto tap = [&](uint16_t k) { return ge::input::key_pressed(in, k); };
    cl::lobby_keys(l, {.f1 = tap(kF1),
                       .f2 = tap(kF2),
                       .f4 = tap(kF4),
                       .f5 = tap(kF5),
                       .f6 = tap(kF6),
                       .enter = tap(kEnter)});
    // The lobby's own screen over the main menu's night while the player is off the map.
    if (cl::on_lobby_screen(l)) {
      cl::listen_advance(l, dt, ge::shooter::MoveCmd{});
      if (!night) {
        night = cl::make_title_world(*device, w, h, surface.format(), look, error);
        if (!night)
          return fail("This browser cannot draw the game. Play it in the game below.", error);
      }
      ge::ui::DrawList page;
      page.begin(iw, ih);
      se::draw_lobby_page(page, cl::listen_lobby_view(l), iw, ih);
      const wgpu::TextureView target = surface.acquire(*device);
      if (!target) continue;
      cl::draw_world(*device, *night, cl::title_view(t), {}, nullptr, dt, page, target);
      surface.present();
      continue;
    }
    cl::Moves moves = cl::key_moves(in);
    moves.left = ge::input::key_held(in, kLeft);
    moves.right = ge::input::key_held(in, kRight);
    moves.forward = moves.forward || ge::input::key_held(in, kUp);
    moves.back = moves.back || ge::input::key_held(in, kDown);
    {  // a thumb's walk and look, and the buttons (touch_play.hpp)
      float yaw = 0, pitch = 0;
      cl::touch_moves(touch, in, t, moves, yaw, pitch);
      in.mouse.dx += yaw / controls.sensitivity;
      in.mouse.dy += pitch / controls.sensitivity;
    }
    cl::turn_view(in, moves, controls, dt, l.view);
    const ge::shooter::MoveCmd cmd = cl::read_controls(moves, in, controls);
    if (cmd.impulse != 0) l.impulse = cmd.impulse;
    const int frames = cl::listen_advance(l, dt, cmd);
    heard.frame(l, files, static_cast<float>(dt));  // the game heard (web/page_audio.hpp)
    ge::web::audio_pump(synth, dt);
    if (std::exchange(l.restarted, false)) {  // the match began: the warmup's leftovers go
      shown = {};
      fx_state.models = nullptr;
      held = {};
      held.hand.at_eye = at_eye;
    }

    if (l.map.get() != drawn || l.server->models.size() != world_models) {
      ge::web::say("Loading " + l.map->name + "...");
      world = cl::create_world_draw(*device, files, *l.map, w, h, surface.format(), look, weapons,
                                    error, {l.server->models, {}}, game);
      if (!world)
        return fail(
            "This browser's graphics cannot draw this game's world yet. Play it in the game "
            "below.",
            error);
      drawn = l.map.get();
      world_models = l.server->models.size();
      told = -1;
    }

    ge::render::FrameView view = cl::frame_view(l, shown, frames, t, dt);
    view.frame = frame;
    fx_state.models = &cl::world_models(*world);
    fx_state.book = &book;
    fx_state.on = cl::fx_switches(look);
    cl::step_fx(l, shown, fx_state, t, dt, fx);
    view::HandFrame hand = cl::step_held(l, shown, held, view_settings, weapons, t, dt);
    if (hand.flashing) fx.lights.push_back(effects::muzzle_light(hand.flash.position));

    ge::ui::DrawList overlay;
    overlay.begin(iw, ih);
    se::fill_hud(hud, cl::status_of(l, t), art);
    hud.layout.draw(overlay, hud.layout.frame(iw, ih, se::hud_style(art)), se::hud_style(art));
    hand.lift = se::status_lift(hud, ih);
    if (l.room != nullptr) se::draw_lobby(overlay, cl::listen_lobby_view(l), iw, ih);
    cl::draw_touch(touch, overlay, iw, ih);
    cl::draw_scores(overlay, l, pictures.menu, iw, ih, ge::input::key_held(in, kTab), false, board);

    const wgpu::TextureView target = surface.acquire(*device);
    if (!target) continue;
    cl::draw_world(*device, *world, view, fx, &hand, dt, overlay, target);
    surface.present();
  }
}

}  // namespace shooters_edge::web
