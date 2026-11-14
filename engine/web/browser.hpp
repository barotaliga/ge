// THE BROWSER'S SIDE OF A WEB VIEWER (Emscripten builds only, GE_WEB): the few calls a game's
// viewer makes on the page that holds it (engine/hub/watch.hpp writes that page).
//
// - `fetch_bytes`: a file from the hub (a demo by its `/d/...` path, a content pack) read whole,
//   the status line and the page's bar following the download when it is named.
// - `next_frame`: waits for the page's next paint and answers its time, ms; the viewer's loop
//   draws one frame a paint, so a hidden tab costs nothing.
// - `canvas_size`: the canvas's pixel size (the page sets it; 1280 x 720 by default).
// - `ws_open`, `ws_send`, `ws_take`, `ws_close`: a WebSocket (a TV node's, tv/websocket.hpp),
//   its binary messages queued as they come and taken whole in order, never waited for: the
//   viewer's loop takes what has come each frame.
// - `wt_open`, `wt_send`, `wt_take`, `wt_close`: a WebTransport session (the page's way to a
//   game server: datagrams to an endpoint, engine/webtransport, which the forwarder sends on as
//   UDP), its certificate trusted by its hash; `wt_pipe` makes it a net transport's pipe, so a
//   game's client plays over it as it would over a socket.
// - `take_input`: the canvas's keys pressed since the last call (by scancode, the USB HID usage
//   the platform layer uses) and its pointer in canvas pixels; a click focuses the canvas.
// - `take_play_input`: a game played on the page: keys held and let go, the mouse's buttons,
//   wheel and movement with the pointer locked to the canvas, as `ge::input::InputState`.
// - `say`: a line in the page's status line (`#viewer-status`) and the console: what is loading,
//   or why there is no picture, in a viewer's words.
//
// Each wait (`fetch_bytes`, `next_frame`, and the GPU's own waits) suspends the C++ stack through
// JSPI (`-sJSPI`, cmake/Web.cmake), so a viewer is written as a plain loop in `main`.
//
// ge:summary The web viewer's calls on its page: fetch a file, wait for a paint, the canvas, say
// ge:keywords web browser emscripten jspi fetch_bytes next_frame request-animation-frame canvas
// ge:keywords canvas_size say viewer-status websocket ws_open ws_take ws_send
// ge:keywords take_play_input pointer-lock play-in-browser webtransport wt_open wt_pipe datagram
// ge:keywords ws_pipe ws_why websocket-fallback
// ge:skill net
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "net/transport.hpp"

namespace ge::input {
struct InputState;
}

namespace ge::web {

// `url`'s bytes into `out`. False with the URL and the HTTP status (or that it did not answer).
// With `progress` ("Loading the demo..."), the status line says it with the megabytes so far and
// the page's bar fills as the bytes come.
bool fetch_bytes(const std::string& url, std::vector<uint8_t>& out, std::string& error,
                 const std::string& progress = "");
// Waits for the page's next paint; its time, ms.
double next_frame();
// The canvas `selector` names: its pixel size.
void canvas_size(const char* selector, uint32_t& width, uint32_t& height);
struct PageInput {
  std::vector<int> pressed;  // scancodes that went down since the last call, in order
  float x = -1, y = -1;      // the pointer over the canvas, px (-1: not over it)
  bool button = false;       // a button held
};
void take_input(const char* selector, PageInput& out);
// A game played on the page (M56i): the canvas's keys, mouse buttons, wheel and the mouse's
// movement since the last call, as the engine's input state (`begin_frame` first, so its edges are
// this paint's): keys held, pressed and released by scancode, the buttons as MouseButton bits, the
// wheel in notches (+ away from the player), the movement in the browser's pixels while the
// pointer is locked to the canvas (a click on it locks it, Escape lets it go; never locked
// otherwise). Losing the focus lets every key go. True while the pointer is locked.
bool take_play_input(const char* selector, ge::input::InputState& s);

enum class WsState : uint8_t { connecting, open, closed };
// A WebSocket to `url` (`ws://host:port/`): its number, or -1 when the browser refused the URL.
int ws_open(const std::string& url);
WsState ws_state(int socket);
// One binary message; false when the socket is not open.
bool ws_send(int socket, std::span<const uint8_t> msg);
// The oldest message not taken yet into `msg`; false when none has come.
bool ws_take(int socket, std::vector<uint8_t>& msg);
// Why the socket closed: the close's reason when the other end gave one ("" while it is open).
std::string ws_why(int socket);
void ws_close(int socket);
// The socket as a transport's pipe (net::open_pipe_transport), each message one datagram: the
// forwarder's WebSocket face (forward/websocket.hpp), for a browser without WebTransport.
net::DatagramPipe ws_pipe(int socket, const net::Address& peer);

enum class WtState : uint8_t { connecting, open, closed };
// A WebTransport session to `url` (`https://host:port/path`), its certificate trusted by
// `cert_hash` (64 hex digits; "" trusts the browser's own authorities): its number, or -1 when the
// browser has no WebTransport or refused the URL.
int wt_open(const std::string& url, const std::string& cert_hash);
WtState wt_state(int session);
// Why the session closed or never opened ("" while it is open).
std::string wt_why(int session);
// One datagram (at most webtransport's kMaxDatagram); false when the session is not open.
bool wt_send(int session, std::span<const uint8_t> datagram);
// The oldest datagram not taken yet; false when none has come.
bool wt_take(int session, std::vector<uint8_t>& datagram);
void wt_close(int session);
// The session as a transport's pipe (net::open_pipe_transport): every datagram sent goes into it,
// whatever its address, and every one received is reported from `peer`.
net::DatagramPipe wt_pipe(int session, const net::Address& peer);

// `line` in the page's status line and the console ("" clears the status line).
void say(const std::string& line);

}  // namespace ge::web
