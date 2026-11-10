// The window over SDL3: the video subsystem's lifetime, the native handles by video driver,
// and the event pump. This file and nothing else includes SDL.
//
// ge:summary The window, input and clock implemented on SDL3 (the only file including SDL)
// ge:keywords sdl sdl3 video-driver event-pump scancode x11 wayland cocoa metal-view win32
// ge:skill platform
// ge:test tests/platform_window_test.cpp
#include "platform/window.hpp"

#include <SDL3/SDL.h>
// SDL_RunApp alone: this file defines no main (SDL_MAIN_HANDLED keeps SDL's own out of it).
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>

#include "platform/devices_sdl.hpp"

#if defined(__APPLE__)
#include <objc/message.h>
#include <objc/runtime.h>
#endif

#include <mutex>
#include <string>

#include "core/output.hpp"

namespace ge::platform {

namespace out = core::output;

// The folder chooser's answer: SDL may call back on a thread of its own.
struct FolderPick {
  std::mutex mutex;
  bool open = false, answered = false;
  std::string path, error;
};

struct Window::Impl {
  SDL_Window* window = nullptr;
  SDL_MetalView metal_view = nullptr;
  FolderPick pick;
};

int run_app(int argc, char** argv, int (*body)(int argc, char** argv)) {
  return SDL_RunApp(argc, argv, body, nullptr);
}

bool display_available() {
  if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) return false;
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
  return true;
}

Window::Window(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

Window::~Window() {
  if (impl_->metal_view) SDL_Metal_DestroyView(impl_->metal_view);
  if (impl_->window) SDL_DestroyWindow(impl_->window);
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

std::unique_ptr<Window> open_window(const WindowDesc& desc) {
  if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
    out::error(std::string("platform: no display to open a window on (") + SDL_GetError() +
               "). A headless test or lab must not open one; for a window in a container run "
               "under a virtual display (xvfb-run -a <cmd>); skill: platform.");
    return nullptr;
  }
  SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
  if (desc.resizable) flags |= SDL_WINDOW_RESIZABLE;
#if defined(__APPLE__)
  flags |= SDL_WINDOW_METAL;
#endif
  const std::string title(desc.title);
  auto impl = std::make_unique<Window::Impl>();
  impl->window = SDL_CreateWindow(title.c_str(), desc.width, desc.height, flags);
  if (!impl->window) {
    out::error(std::string("platform: SDL_CreateWindow failed (") + SDL_GetError() +
               "); skill: platform.");
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return nullptr;
  }
#if defined(__APPLE__)
  impl->metal_view = SDL_Metal_CreateView(impl->window);
#endif
  return std::unique_ptr<Window>(new Window(std::move(impl)));
}

NativeWindow Window::native() const {
  NativeWindow n;
  const SDL_PropertiesID props = SDL_GetWindowProperties(impl_->window);
  const std::string driver = SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "";
  if (impl_->metal_view) {
    n.kind = NativeWindow::Kind::metal;
    n.metal_layer = SDL_Metal_GetLayer(impl_->metal_view);
  } else if (driver == "x11") {
    n.kind = NativeWindow::Kind::x11;
    n.display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
    n.x11_window =
        static_cast<uint64_t>(SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
  } else if (driver == "wayland") {
    n.kind = NativeWindow::Kind::wayland;
    n.display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
    n.surface = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
  } else if (driver == "windows") {
    n.kind = NativeWindow::Kind::win32;
    n.surface = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    n.instance = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, nullptr);
  } else {
    out::error("platform: the video driver '" + driver +
               "' has no surface handles the GPU knows; add its case to Window::native and to "
               "engine/gpu/surface.cpp; skill: platform.");
  }
  return n;
}

void Window::pixel_size(int& width, int& height) const {
  if (!SDL_GetWindowSizeInPixels(impl_->window, &width, &height)) width = height = 0;
}

namespace {

void pump(Window& window, InputSnapshot& in, Devices* devices, input::Queue* queue) {
  in.pressed.fill(false);
  in.quit = in.focus_lost = in.focus_gained = in.resized = false;
  if (devices) {
    devices_begin(*devices, *queue);
    SDL_UpdateJoysticks();
  }
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (devices) devices_event(*devices, e, *queue);
    switch (e.type) {
      case SDL_EVENT_QUIT:
      case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        in.quit = true;
        break;
      case SDL_EVENT_KEY_DOWN:
        if (e.key.scancode < kKeyCount) {
          if (!e.key.repeat) in.pressed[e.key.scancode] = true;
          in.held[e.key.scancode] = true;
        }
        break;
      case SDL_EVENT_KEY_UP:
        if (e.key.scancode < kKeyCount) in.held[e.key.scancode] = false;
        break;
      case SDL_EVENT_MOUSE_MOTION:
        in.pointer_x = e.motion.x;
        in.pointer_y = e.motion.y;
        break;
      case SDL_EVENT_MOUSE_BUTTON_DOWN:
      case SDL_EVENT_MOUSE_BUTTON_UP: {
        const uint32_t bit = e.button.button == SDL_BUTTON_LEFT     ? 1u
                             : e.button.button == SDL_BUTTON_MIDDLE ? 2u
                             : e.button.button == SDL_BUTTON_RIGHT  ? 4u
                                                                    : 0u;
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
          in.pointer_buttons |= bit;
        } else {
          in.pointer_buttons &= ~bit;
        }
        break;
      }
      case SDL_EVENT_WINDOW_FOCUS_LOST:
        in.focus_lost = true;
        break;
      case SDL_EVENT_WINDOW_FOCUS_GAINED:
        in.focus_gained = true;
        break;
      case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        in.resized = true;
        break;
      default:
        break;
    }
  }
  window.pixel_size(in.width_px, in.height_px);
}

}  // namespace

void poll_input(Window& window, InputSnapshot& in) { pump(window, in, nullptr, nullptr); }

void poll_input(Window& window, InputSnapshot& in, Devices& devices, input::Queue& queue) {
  pump(window, in, &devices, &queue);
}

void set_relative_mouse(Window& window, bool on) {
  SDL_SetWindowRelativeMouseMode(window.impl_->window, on);
}

bool relative_mouse(const Window& window) {
  return SDL_GetWindowRelativeMouseMode(window.impl_->window);
}

void set_text_input(Window& window, bool on) {
  if (on) {
    SDL_StartTextInput(window.impl_->window);
  } else {
    SDL_StopTextInput(window.impl_->window);
  }
}

bool pick_folder(Window& window, std::string_view title, std::string_view start) {
  FolderPick& pick = window.impl_->pick;
  {
    const std::lock_guard<std::mutex> lock(pick.mutex);
    if (pick.open) return false;
    pick.open = true;
    pick.answered = false;
    pick.path.clear();
    pick.error.clear();
  }
  const auto answer = [](void* user, const char* const* list, int) {
    auto* p = static_cast<FolderPick*>(user);
    const std::lock_guard<std::mutex> lock(p->mutex);
    if (list == nullptr)
      p->error =
          std::string("the desktop could not show a folder chooser (") + SDL_GetError() + ")";
    else if (list[0] != nullptr)
      p->path = list[0];
    p->answered = true;
  };
  const std::string t(title), at(start);
  const SDL_PropertiesID props = SDL_CreateProperties();
  SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, window.impl_->window);
  SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_TITLE_STRING, t.c_str());
  if (!at.empty()) SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_LOCATION_STRING, at.c_str());
  SDL_ShowFileDialogWithProperties(SDL_FILEDIALOG_OPENFOLDER, answer, &pick, props);
  SDL_DestroyProperties(props);
  return true;
}

bool folder_picked(Window& window, std::string* path, std::string* error) {
  FolderPick& pick = window.impl_->pick;
  const std::lock_guard<std::mutex> lock(pick.mutex);
  if (!pick.answered) return false;
  *path = pick.path;
  *error = pick.error;
  pick.open = pick.answered = false;
  return true;
}

std::vector<Display> displays() {
  std::vector<Display> out;
  // Held for the call (SDL counts each hold), so the list is there before a window opens: a `+`
  // word on the command line asks before the client's window exists.
  if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) return out;
  int count = 0;
  SDL_DisplayID* ids = SDL_GetDisplays(&count);
  for (int i = 0; ids != nullptr && i < count; ++i) {
    Display d;
    d.number = i + 1;
    const char* name = SDL_GetDisplayName(ids[i]);
    d.name = name != nullptr ? name : "";
    if (const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(ids[i])) {
      d.width_px = int(double(mode->w) * double(mode->pixel_density));
      d.height_px = int(double(mode->h) * double(mode->pixel_density));
    }
    out.push_back(d);
  }
  SDL_free(ids);
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
  return out;
}

bool set_fullscreen(Window& window, bool on, int display) {
  SDL_Window* w = window.impl_->window;
  if (on && display > 0) {
    // The window moved onto that display first: the fullscreen covers the display it stands on.
    int count = 0;
    SDL_DisplayID* ids = SDL_GetDisplays(&count);
    if (ids == nullptr || display > count) {
      SDL_free(ids);
      out::error("platform: there is no display " + std::to_string(display) + " (there are " +
                 std::to_string(count) +
                 "); fullscreen goes on the window's own display; pass a number the display list "
                 "names (vid_displays) or 0; skill: platform.");
    } else {
      const SDL_DisplayID id = ids[display - 1];
      SDL_free(ids);
      SDL_SetWindowPosition(w, int(SDL_WINDOWPOS_CENTERED_DISPLAY(id)),
                            int(SDL_WINDOWPOS_CENTERED_DISPLAY(id)));
      SDL_SyncWindow(w);
    }
  }
  // A null mode is the borderless desktop fullscreen: the display keeps its mode.
  if (!SDL_SetWindowFullscreenMode(w, nullptr) || !SDL_SetWindowFullscreen(w, on)) {
    out::error(std::string("platform: the window could not go ") +
               (on ? "fullscreen" : "back to a window") + " (" + SDL_GetError() +
               "); the window stays as it was; skill: platform.");
    return false;
  }
  return true;
}

uint64_t now_ns() { return SDL_GetTicksNS(); }

void wait_until_ns(uint64_t deadline_ns) {
  const uint64_t now = SDL_GetTicksNS();
  if (deadline_ns > now) SDL_DelayPrecise(deadline_ns - now);
}

double refresh_hz(const Window& window) {
  const SDL_DisplayID id = SDL_GetDisplayForWindow(window.impl_->window);
  if (id == 0) return 0;
  const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(id);
  return mode != nullptr && mode->refresh_rate > 0 ? static_cast<double>(mode->refresh_rate) : 0;
}

void set_swap_images(Window& window, int images) {
#if defined(__APPLE__)
  if (window.impl_->metal_view == nullptr) return;
  void* layer = SDL_Metal_GetLayer(window.impl_->metal_view);
  if (layer == nullptr) return;
  // The layer's maximumDrawableCount (2 or 3), set through the Objective-C runtime so this file
  // stays C++.
  const unsigned long count = images <= 2 ? 2ul : 3ul;
  using SetCount = void (*)(void*, SEL, unsigned long);
  reinterpret_cast<SetCount>(objc_msgSend)(layer, sel_registerName("setMaximumDrawableCount:"),
                                           count);
#else
  (void)window;
  (void)images;
#endif
}

std::string prefs_dir(std::string_view org, std::string_view app) {
  char* path = SDL_GetPrefPath(std::string(org).c_str(), std::string(app).c_str());
  if (!path) return {};
  std::string dir = path;
  SDL_free(path);
  return dir;
}

}  // namespace ge::platform
