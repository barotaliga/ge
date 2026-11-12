// THE BROWSER'S SIDE OF A WEB VIEWER (Emscripten builds only, GE_WEB): the few calls a game's
// viewer makes on the page that holds it (engine/hub/watch.hpp writes that page).
//
// - `fetch_bytes`: a file from the hub (a demo by its `/d/...` path, a content pack) read whole.
// - `next_frame`: waits for the page's next paint and answers its time, ms; the viewer's loop
//   draws one frame a paint, so a hidden tab costs nothing.
// - `canvas_size`: the canvas's pixel size (the page sets it; 1280 x 720 by default).
// - `ws_open`, `ws_send`, `ws_take`, `ws_close`: a WebSocket (a TV node's, tv/websocket.hpp),
//   its binary messages queued as they come and taken whole in order, never waited for: the
//   viewer's loop takes what has come each frame.
// - `say`: a line in the page's status line (`#viewer-status`) and the console: what is loading,
//   or why there is no picture, in a viewer's words.
//
// Each wait (`fetch_bytes`, `next_frame`, and the GPU's own waits) suspends the C++ stack through
// JSPI (`-sJSPI`, cmake/Web.cmake), so a viewer is written as a plain loop in `main`.
//
// ge:summary The web viewer's calls on its page: fetch a file, wait for a paint, the canvas, say
// ge:keywords web browser emscripten jspi fetch_bytes next_frame request-animation-frame canvas
// ge:keywords canvas_size say viewer-status websocket ws_open ws_take ws_send
// ge:skill net
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ge::web {

// `url`'s bytes into `out`. False with the URL and the HTTP status (or that it did not answer).
bool fetch_bytes(const std::string& url, std::vector<uint8_t>& out, std::string& error);
// Waits for the page's next paint; its time, ms.
double next_frame();
// The canvas `selector` names: its pixel size.
void canvas_size(const char* selector, uint32_t& width, uint32_t& height);
enum class WsState : uint8_t { connecting, open, closed };
// A WebSocket to `url` (`ws://host:port/`): its number, or -1 when the browser refused the URL.
int ws_open(const std::string& url);
WsState ws_state(int socket);
// One binary message; false when the socket is not open.
bool ws_send(int socket, std::span<const uint8_t> msg);
// The oldest message not taken yet into `msg`; false when none has come.
bool ws_take(int socket, std::vector<uint8_t>& msg);
void ws_close(int socket);

// `line` in the page's status line and the console ("" clears the status line).
void say(const std::string& line);

}  // namespace ge::web
