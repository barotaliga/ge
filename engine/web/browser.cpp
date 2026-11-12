// The browser's side of a web viewer, defined (browser.hpp). Built only for Emscripten.
//
// ge:summary The web viewer's page calls defined: fetch, the paint, the canvas, the status line
// ge:keywords web browser emscripten em-async-js fetch request-animation-frame websocket
// ge:keywords keys pointer scancode take_input
// ge:skill net
#include "web/browser.hpp"

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <cstdlib>

namespace {

// 0 with the bytes in a malloc'd block; the HTTP status when it was not 2xx; -1 when no answer.
EM_ASYNC_JS(int, ge_web_fetch, (const char* url, uint8_t** out, int* len), {
  try {
    const r = await fetch(UTF8ToString(url));
    if (!r.ok) return r.status;
    const b = new Uint8Array(await r.arrayBuffer());
    const p = _malloc(b.length || 1);
    HEAPU8.set(b, p);
    HEAPU32[out >> 2] = p;
    HEAP32[len >> 2] = b.length;
    return 0;
  } catch (e) {
    return -1;
  }
});

// The JS is written without arrow functions: the C++ formatter would split `=>`.
EM_ASYNC_JS(double, ge_web_paint, (),
            { return await new Promise(function(done) { requestAnimationFrame(done); }); });

EM_JS(void, ge_web_say, (const char* line), {
  const t = UTF8ToString(line);
  const e = document.getElementById("viewer-status");
  if (e) e.textContent = t;
  if (t) console.log(t);
});

// The page's WebSockets, by number: each one's messages queued as they come, taken in order.
EM_JS(int, ge_web_ws_open, (const char* url), {
  if (!Module.geSockets) Module.geSockets = [];
  const s = {queue : [], state : 0};
  try {
    s.ws = new WebSocket(UTF8ToString(url));
  } catch (e) {
    return -1;
  }
  s.ws.binaryType = "arraybuffer";
  s.ws.onopen = function() { s.state = 1; };
  s.ws.onclose = function() { s.state = 2; };
  s.ws.onerror = function() { s.state = 2; };
  s.ws.onmessage = function(m) {
    if (m.data instanceof ArrayBuffer) s.queue.push(new Uint8Array(m.data));
  };
  Module.geSockets.push(s);
  return Module.geSockets.length - 1;
});

EM_JS(int, ge_web_ws_state, (int id), {
  const s = Module.geSockets && Module.geSockets[id];
  return s ? s.state : 2;
});

EM_JS(int, ge_web_ws_send, (int id, const uint8_t* data, int len), {
  const s = Module.geSockets && Module.geSockets[id];
  if (!s || s.state != 1) return 0;
  s.ws.send(HEAPU8.slice(data, data + len));
  return 1;
});

// The next message's length, or -1 when none is queued.
EM_JS(int, ge_web_ws_peek, (int id), {
  const s = Module.geSockets && Module.geSockets[id];
  return s && s.queue.length ? s.queue[0].length : -1;
});

EM_JS(void, ge_web_ws_take, (int id, uint8_t* out), {
  const s = Module.geSockets[id];
  HEAPU8.set(s.queue.shift(), out);
});

EM_JS(void, ge_web_ws_close, (int id), {
  const s = Module.geSockets && Module.geSockets[id];
  if (s && s.ws) s.ws.close();
  if (s) s.state = 2;
});

// The canvas's keys and pointer, gathered by listeners set on the first call: keys by the
// keyboard's scancode (the USB HID usage the platform layer uses too) as they go down, the pointer
// in the canvas's pixels. A click focuses the canvas, so its keys never scroll the page. The JS
// keeps to what the C++ formatter leaves alone: no `===`, no regular expressions, no `=>`.
EM_JS(int, ge_web_input, (const char* selector, int* keys, int most, float* xyb), {
  const c = document.querySelector(UTF8ToString(selector));
  if (!c) return 0;
  if (!c.geInput) {
    const g = {keys : [], x : -1, y : -1, b : 0};
    const codes = {
      Space : 44,
      ArrowRight : 79,
      ArrowLeft : 80,
      ArrowDown : 81,
      ArrowUp : 82,
      Home : 74,
      PageUp : 75,
      End : 77,
      PageDown : 78
    };
    c.tabIndex = 0;
    c.addEventListener(
        "keydown", function(e) {
          let k = codes[e.code];
          if (!k && e.code.length == 4 && e.code.indexOf("Key") == 0) k = e.code.charCodeAt(3) - 61;
          if (!k) return;
          g.keys.push(k);
          e.preventDefault();
        });
    const at = function(e) {
      const r = c.getBoundingClientRect();
      g.x = (e.clientX - r.left) * c.width / r.width;
      g.y = (e.clientY - r.top) * c.height / r.height;
    };
    c.addEventListener("pointermove", at);
    c.addEventListener(
        "pointerdown", function(e) {
          at(e);
          g.b = 1;
          c.focus();
        });
    window.addEventListener("pointerup", function() { g.b = 0; });
    c.addEventListener(
        "pointerleave", function() {
          if (!g.b) g.x = g.y = -1;
        });
    c.geInput = g;
  }
  const g = c.geInput;
  const n = Math.min(g.keys.length, most);
  for (let i = 0; i < n; i++) HEAP32[(keys >> 2) + i] = g.keys[i];
  g.keys = [];
  HEAPF32[xyb >> 2] = g.x;
  HEAPF32[(xyb >> 2) + 1] = g.y;
  HEAPF32[(xyb >> 2) + 2] = g.b;
  return n;
});

}  // namespace

namespace ge::web {

void take_input(const char* selector, PageInput& out) {
  int keys[64];
  float xyb[3] = {-1, -1, 0};
  const int n = ge_web_input(selector, keys, 64, xyb);
  out.pressed.assign(keys, keys + n);
  out.x = xyb[0];
  out.y = xyb[1];
  out.button = xyb[2] != 0;
}

int ws_open(const std::string& url) { return ge_web_ws_open(url.c_str()); }

WsState ws_state(int socket) {
  const int s = ge_web_ws_state(socket);
  return s == 0 ? WsState::connecting : s == 1 ? WsState::open : WsState::closed;
}

bool ws_send(int socket, std::span<const uint8_t> msg) {
  return ge_web_ws_send(socket, msg.data(), static_cast<int>(msg.size())) != 0;
}

bool ws_take(int socket, std::vector<uint8_t>& msg) {
  const int n = ge_web_ws_peek(socket);
  if (n < 0) return false;
  msg.resize(static_cast<size_t>(n));
  ge_web_ws_take(socket, msg.data());
  return true;
}

void ws_close(int socket) { ge_web_ws_close(socket); }

bool fetch_bytes(const std::string& url, std::vector<uint8_t>& out, std::string& error) {
  uint8_t* data = nullptr;
  int len = 0;
  const int status = ge_web_fetch(url.c_str(), &data, &len);
  if (status != 0) {
    error = status < 0 ? "the hub did not answer for " + url
                       : "the hub answered " + std::to_string(status) + " for " + url;
    return false;
  }
  out.assign(data, data + len);
  std::free(data);
  return true;
}

double next_frame() { return ge_web_paint(); }

void canvas_size(const char* selector, uint32_t& width, uint32_t& height) {
  int w = 0, h = 0;
  emscripten_get_canvas_element_size(selector, &w, &h);
  width = static_cast<uint32_t>(w > 0 ? w : 0);
  height = static_cast<uint32_t>(h > 0 ? h : 0);
}

void say(const std::string& line) { ge_web_say(line.c_str()); }

}  // namespace ge::web
