// The window's own settings (window_cvars.hpp).
//
// ge:summary The window's settings for any game: vid_fullscreen and its modes, vid_display, volume,
// ge:summary s_auto_focus
// ge:keywords add_window_cvars vid_modelist vid-fullscreen-back vid_hwgamma full_mode_of
// ge:keywords vid_displays follow_window_cvars window_style window_place_of
// ge:skill platform console
// ge:test tests/platform_window_cvars_test.cpp
#include "platform/window_cvars.hpp"

#include <string>
#include <vector>

#include "core/output.hpp"
#include "platform/audio.hpp"
#include "platform/window.hpp"

namespace ge::platform {

namespace {

void list_sound_devices(console::Console& c) {
  const std::vector<std::string> all = audio_devices();
  if (all.empty()) {
    c.print("No sound devices.");
    return;
  }
  c.print("Available devices:");
  for (const std::string& d : all) c.print(d);
}

}  // namespace

void add_window_cvars(console::Console& con, WindowCvars& w) {
  w.modelist = w.modes_applied = default_modelist();
  con.cvars().bind({.name = "vid_fullscreen",
                    .fallback = "0",
                    .help = "Whether the window covers the whole screen, and in which mode.",
                    .kind = console::CvarKind::integer,
                    .min = 0,
                    .values = "0: a window (the default); N: the whole screen in mode N of "
                              "vid_modelist (1: the first, the desktop's own)"},
                   &w.fullscreen);
  con.cvars().bind({.name = "_vid_fullscreen",
                    .fallback = "1",
                    .flags = console::kArchive,
                    .help = "The full-screen mode Alt+Enter goes back to from a window.",
                    .kind = console::CvarKind::integer,
                    .min = 1,
                    .values = "N: mode N of vid_modelist (1 the default); kept from the last "
                              "vid_fullscreen above 0"},
                   &w.fullscreen_back);
  con.cvars().bind({.name = "vid_modelist",
                    .fallback = w.modelist,
                    .help = "The full-screen modes vid_fullscreen picks from, counted from 1.",
                    .kind = console::CvarKind::text,
                    .values = "words: desktop (the desktop's own mode) or WxH, WxH@Hz, "
                              "WxH@Hz:bpp, WxH:bpp@Hz; the default the display's own list"},
                   &w.modelist);
  con.cvars().bind({.name = "vid_hwgamma",
                    .fallback = "0",
                    .help = "Whether the brightness (vid_gamma) is set by the display's gamma "
                            "ramp rather than in the picture.",
                    .kind = console::CvarKind::boolean,
                    .values = "0: in the picture (the default); 1: by the display, where it has "
                              "a ramp"},
                   &w.hwgamma);
  con.cvars().bind({.name = "vid_display",
                    .fallback = "0",
                    .help = "The display vid_fullscreen covers.",
                    .kind = console::CvarKind::integer,
                    .values = "0: the one the window is on (the default); 1..N: that one of the "
                              "displays vid_displays lists"},
                   &w.display);
  con.cvars().bind({.name = "volume",
                    .fallback = "0.7",
                    .help = "The game's sound, a played demo's sounds and voices too.",
                    .kind = console::CvarKind::number,
                    .min = 0,
                    .max = 1,
                    .values = "0: silent; 0..1: that loudness (0.7 the default)"},
                   &w.volume);
  con.cvars().bind({.name = "s_auto_focus",
                    .fallback = "0",
                    .flags = console::kArchive,
                    .help = "When the game is heard while you are in another window.",
                    .kind = console::CvarKind::integer,
                    .min = 0,
                    .max = 2,
                    .values = "0: always (the default); 1: not while the game is minimised; 2: "
                              "only while the game's window is the one you are in"},
                   &w.auto_focus);
  const struct {
    const char* name;
    const char* help;
    const char* values;
    bool* field;
  } style[] = {
      {"win_noborder", "Whether the window has no border or title bar.",
       "0: a border (the default); 1: none", &w.no_border},
      {"win_notitle", "Whether the window has no title bar (and so no border).",
       "0: a title bar (the default); 1: none", &w.no_title},
      {"win_alwaysontop", "Whether the window stays above every other window.",
       "0: as any window (the default); 1: always on top", &w.on_top},
      {"win_noresize", "Whether the window's edges no longer resize it.",
       "0: resized by its edges (the default); 1: a fixed size", &w.no_resize},
  };
  for (const auto& s : style)
    con.cvars().bind({.name = s.name,
                      .fallback = "0",
                      .flags = console::kArchive,
                      .help = s.help,
                      .kind = console::CvarKind::boolean,
                      .values = s.values},
                     s.field);
  con.cvars().bind({.name = "vid_geometry",
                    .fallback = "",
                    .flags = console::kArchive,
                    .help = "The window's size and place on the desktop.",
                    .kind = console::CvarKind::text,
                    .values = "\"\": as the game opens it (the default); WxH: that size; "
                              "WxH+X+Y: that size, X and Y from the desktop's top left; +X+Y: "
                              "that place alone"},
                   &w.geometry);
  con.cvars().bind({.name = "s_enable",
                    .fallback = "1",
                    .flags = console::kArchive,
                    .help = "Whether the game makes any sound.",
                    .kind = console::CvarKind::integer,
                    .min = 0,
                    .max = 2,
                    .values = "0: no sound at all; 1: the sound on (the default); 2: the same"},
                   &w.sound);
  con.cvars().bind({.name = "s_device",
                    .fallback = "",
                    .flags = console::kArchive,
                    .help = "The sound device the game plays on.",
                    .kind = console::CvarKind::text,
                    .values = "\"\": the system's default (the default); NAME: that device, as "
                              "s_devices lists them; ?: the list"},
                   &w.sound_device);
  con.add_command("s_devices", "list the sound devices s_device names",
                  [](console::Console& c, const console::Args&) { list_sound_devices(c); });
  con.add_command("vid_displays", "list the displays vid_display numbers",
                  [](console::Console& c, const console::Args&) {
                    const std::vector<Display> all = displays();
                    if (all.empty()) c.print("vid_displays: the desktop named no display");
                    for (const Display& d : all)
                      c.print(std::to_string(d.number) + ": " + d.name + ", " +
                              std::to_string(d.width_px) + "x" + std::to_string(d.height_px));
                  });
}

namespace {

// A whole number with an optional sign at `text[i]`, `i` moved past it; false when none is there.
bool take_int(std::string_view text, size_t& i, bool sign, int& out) {
  bool minus = false;
  if (sign) {
    if (i >= text.size() || (text[i] != '+' && text[i] != '-')) return false;
    minus = text[i++] == '-';
  }
  const size_t start = i;
  long v = 0;
  while (i < text.size() && text[i] >= '0' && text[i] <= '9' && v < 100000)
    v = v * 10 + (text[i++] - '0');
  if (i == start || (i < text.size() && text[i] >= '0' && text[i] <= '9')) return false;
  out = static_cast<int>(minus ? -v : v);
  return true;
}

}  // namespace

bool window_place_of(std::string_view text, WindowPlace& out) {
  WindowPlace p;
  size_t i = 0;
  if (!text.empty() && text[0] != '+' && text[0] != '-') {
    if (!take_int(text, i, false, p.width) || i >= text.size() ||
        (text[i] != 'x' && text[i] != 'X'))
      return false;
    ++i;
    if (!take_int(text, i, false, p.height) || p.width <= 0 || p.height <= 0) return false;
  }
  if (i < text.size()) {
    if (!take_int(text, i, true, p.x) || !take_int(text, i, true, p.y)) return false;
    p.has_place = true;
  }
  if (i != text.size() || (p.width == 0 && !p.has_place)) return false;
  out = p;
  return true;
}

namespace {

// "WxH", "WxH@Hz", "WxH@Hz:bpp" or "WxH:bpp@Hz"; false when malformed.
bool mode_text(std::string_view t, FullMode& m) {
  size_t i = 0;
  if (!take_int(t, i, false, m.width) || i >= t.size() || (t[i] != 'x' && t[i] != 'X'))
    return false;
  ++i;
  if (!take_int(t, i, false, m.height)) return false;
  bool hz = false, bpp = false;
  while (i < t.size()) {
    const char c = t[i++];
    if (c == '@' && !hz) {
      if (!take_int(t, i, false, m.hz)) return false;
      hz = true;
    } else if (c == ':' && !bpp) {
      if (!take_int(t, i, false, m.bpp)) return false;
      bpp = true;
    } else {
      return false;
    }
  }
  return true;
}

}  // namespace

bool full_mode_of(std::string_view list, int n, FullMode& out, std::string* why) {
  int k = 0;
  size_t i = 0;
  std::string_view word;
  while (i < list.size()) {
    while (i < list.size() && list[i] == ' ') ++i;
    const size_t start = i;
    while (i < list.size() && list[i] != ' ') ++i;
    if (i > start && ++k == n) {
      word = list.substr(start, i - start);
      break;
    }
  }
  const std::string num = std::to_string(n);
  if (word.empty()) {
    if (why != nullptr) *why = "Mode " + num + " not found";
    return false;
  }
  if (word == "desktop") return false;
  FullMode m;
  if (!mode_text(word, m)) {
    if (why != nullptr) *why = "Mode " + num + " is malformed";
    return false;
  }
  if (m.width < 320 || m.width > 8192 || m.height < 240 || m.height > 8192 || m.hz > 1000 ||
      m.bpp > 32) {
    if (why != nullptr)
      *why = "Mode " + std::to_string(m.width) + "x" + std::to_string(m.height) + "@" +
             std::to_string(m.hz) + ":" + std::to_string(m.bpp) + " doesn't look sane";
    return false;
  }
  out = m;
  return true;
}

std::string default_modelist() {
  std::string list = "desktop";
  for (const std::string& m : display_modes()) list += " " + m;
  return list;
}

WindowStyle window_style(const WindowCvars& w) {
  return {.border = !w.no_border && !w.no_title, .on_top = w.on_top, .resizable = !w.no_resize};
}

void follow_sound_device(AudioOut* out, WindowCvars& w, console::Console& con) {
  if (out == nullptr || w.sound_device == w.sound_applied) return;
  if (w.sound_device == "?") {  // the list, the setting left as it was
    list_sound_devices(con);
    con.cvars().set("s_device", w.sound_applied, console::Source::local);
    return;
  }
  if (!move_audio(*out, w.sound_device))
    con.print("There is no sound device called " + w.sound_device + "; the sound stays on " +
              (w.sound_applied.empty() ? std::string("the default device") : w.sound_applied) +
              ".");
  w.sound_applied = w.sound_device;  // asked once: a missing device is not asked for again
}

void follow_window_cvars(Window& window, WindowCvars& w, AudioOut* sound, console::Console* con) {
  if (con != nullptr) follow_sound_device(sound, w, *con);
  if (w.hwgamma) {  // the platform layer has no gamma ramp to read or write
    w.hwgamma = false;
    if (con != nullptr) {
      con->print("...hardware gamma not supported");
      con->cvars().set("vid_hwgamma", "0", console::Source::local);
    }
  }
  if (w.fullscreen > 0 && w.fullscreen != w.fullscreen_back) {  // kept, and saved with the cfg
    w.fullscreen_back = w.fullscreen;
    if (con != nullptr)
      con->cvars().set("_vid_fullscreen", std::to_string(w.fullscreen), console::Source::local);
  }
  // A new mode list takes effect at once, only while full-screen.
  const bool list_moved = w.fullscreen > 0 && w.modelist != w.modes_applied;
  w.modes_applied = w.modelist;
  if (w.fullscreen != w.full_applied || list_moved) {  // the size follows through `resized`
    FullMode mode;
    std::string why;
    const bool own = w.fullscreen > 0 && full_mode_of(w.modelist, w.fullscreen, mode, &why);
    if (!why.empty()) core::output::debug("platform: " + why + "; the desktop's own mode is used");
    if (!set_fullscreen(window, w.fullscreen > 0, w.display, own ? &mode : nullptr))
      w.fullscreen = w.full_applied;
    w.full_applied = w.fullscreen;
  }
  const WindowStyle want = window_style(w);
  if (!w.fullscreen &&
      (want.border != w.style_applied.border || want.on_top != w.style_applied.on_top ||
       want.resizable != w.style_applied.resizable)) {
    set_window_style(window, want);  // a refusal is said once: it is not asked again
    w.style_applied = want;
  }
  if (!w.fullscreen && w.geometry != w.geometry_applied) {
    WindowPlace place;
    if (window_place_of(w.geometry, place)) set_window_place(window, place);  // else left as it is
    w.geometry_applied = w.geometry;
  }
}

bool alt_enter(const InputSnapshot& in, console::Console& con) {
  constexpr size_t kLeftAlt = 226, kRightAlt = 230;  // the keys' HID usages
  if (!in.tapped(Key::enter) || !(in.held[kLeftAlt] || in.held[kRightAlt])) return false;
  const console::Cvar* v = con.cvars().find("vid_fullscreen");
  const console::Cvar* back = con.cvars().find("_vid_fullscreen");
  const bool full = v != nullptr && !v->value.empty() && v->value != "0";
  if (full) con.cvars().set("_vid_fullscreen", v->value, console::Source::local);
  const std::string to = back != nullptr && !back->value.empty() ? back->value : "1";
  con.execute(std::string("vid_fullscreen ") + (full ? "0" : to));
  return true;
}

double heard_volume(const WindowCvars& w, bool focused, bool minimised) {
  const bool silent =
      w.sound == 0 || (w.auto_focus >= 1 && minimised) || (w.auto_focus >= 2 && !focused);
  return silent ? 0 : w.volume;
}

}  // namespace ge::platform
