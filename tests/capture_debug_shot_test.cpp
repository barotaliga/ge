// engine/capture/debug_shot.hpp: `gpudebug` hides the console, lets its frames pass and takes the
// next one; the notes start with the time, the build and the frame's size, then the game's; the
// picture is the game's composition packed as PNG and handed to the clipboard with the notes as
// its text; the console is told what was copied, or why not; a second gpudebug while one is being
// made is refused; a frame nobody asked for is not taken.
//
// ge:summary Tests for gpudebug's flow: the frames, the notes, the PNG, the clipboard
// ge:keywords test gpudebug debug-shot clipboard notes
// ge:skill testing replay
#include <cstdint>
#include <string>
#include <vector>

#include "capture/debug_shot.hpp"
#include "capture/still.hpp"
#include "check.hpp"
#include "console/console.hpp"

namespace {

namespace gc = ge::capture;

struct Clipboard {
  int calls = 0;
  std::vector<uint8_t> png;
  std::string text;
};

// A shot whose game adds two notes, composes by adding a 10 px band, and copies to `clip`.
void wire(gc::DebugShot& shot, Clipboard& clip, int& started) {
  shot.build = "0123456789ab on main";
  shot.on_start = [&started](int frames) { started = frames; };
  shot.notes = [] { return std::vector<std::string>{"# where", "player at (1 2 3)"}; };
  shot.compose = [](int w, int h, std::span<const uint8_t> rgba, std::span<const std::string>) {
    gc::SheetPixels s{w, h + 10, std::vector<uint8_t>(static_cast<size_t>(w * (h + 10) * 4), 0)};
    std::copy(rgba.begin(), rgba.end(), s.rgba.begin());
    return s;
  };
  shot.to_clipboard = [&clip](std::span<const uint8_t> png, const std::string& text, std::string*) {
    ++clip.calls;
    clip.png.assign(png.begin(), png.end());
    clip.text = text;
    return true;
  };
}

}  // namespace

TEST(gpudebug_takes_the_frame_after_its_frames_with_the_console_hidden) {
  gc::DebugShot shot;
  Clipboard clip;
  int started = 0;
  wire(shot, clip, started);
  ge::console::Console con;
  std::vector<std::string> said;
  con.set_sink([&said](std::string_view l) { said.emplace_back(l); });
  gc::add_gpudebug_command(con, shot);
  CHECK(!shot.hiding());
  CHECK(!shot.write(4, 4, std::vector<uint8_t>(64, 9)));  // nobody asked: not taken
  con.execute("gpudebug 3");
  CHECK(started == 3);
  CHECK(shot.hiding());
  CHECK(!shot.due());
  CHECK(!shot.due());
  CHECK(shot.due());  // the third frame is the one
  CHECK(!shot.due());
  CHECK(shot.hiding());  // until the copy comes back
  // A second while this one is being made: refused, said so.
  con.execute("gpudebug");
  CHECK(!said.empty() && said.back().find("still being made") != std::string::npos);
  CHECK(shot.write(4, 4, std::vector<uint8_t>(64, 200)));
  CHECK(!shot.hiding());
  shot.flush();
  const std::vector<std::string> lines = shot.poll();
  CHECK(lines.size() == 1);
  CHECK_MSG(!lines.empty() && lines[0].find("on the clipboard") != std::string::npos,
            lines.empty() ? "" : lines[0]);
  CHECK(!lines.empty() && lines[0].find("4x14") != std::string::npos);
  CHECK(clip.calls == 1);
  // The PNG is the composed picture; the text is the notes, the engine's first.
  std::string why;
  const auto back = gc::decode_still(clip.png, why);
  CHECK_MSG(back.has_value() && back->width == 4 && back->height == 14, why);
  CHECK(clip.text.rfind("# gpudebug ", 0) == 0);
  CHECK(clip.text.find("\nbuild 0123456789ab on main\n") != std::string::npos);
  CHECK(clip.text.find("\nframe 4x4 (the console left out)\n") != std::string::npos);
  CHECK(clip.text.find("\n# where\nplayer at (1 2 3)\n") != std::string::npos);
  CHECK(shot.poll().empty());  // said once
  // Free again: a new one starts.
  CHECK(shot.start(1, nullptr));
  CHECK(shot.due());
}

TEST(a_copy_that_could_not_be_made_this_frame_is_asked_again) {
  gc::DebugShot shot;
  CHECK(shot.start(1, nullptr));
  CHECK(shot.due());
  shot.request();  // the readback ring was full
  CHECK(shot.due());
  CHECK(shot.hiding());
}

TEST(with_no_clipboard_the_console_is_told) {
  gc::DebugShot shot;
  CHECK(shot.start(1, nullptr));
  CHECK(shot.due());
  CHECK(shot.write(2, 2, std::vector<uint8_t>(16, 1)));
  shot.flush();
  const std::vector<std::string> lines = shot.poll();
  CHECK(lines.size() == 1 && lines[0].find("no clipboard") != std::string::npos);
}

TEST(a_refused_clipboard_says_why) {
  gc::DebugShot shot;
  shot.to_clipboard = [](std::span<const uint8_t>, const std::string&, std::string* why) {
    *why = "the clipboard is busy";
    return false;
  };
  CHECK(shot.start(1, nullptr));
  CHECK(shot.due());
  CHECK(shot.write(2, 2, std::vector<uint8_t>(16, 1)));
  shot.flush();
  const std::vector<std::string> lines = shot.poll();
  CHECK(lines.size() == 1 && lines[0] == "gpudebug: the clipboard is busy.");
}

TEST(the_text_is_the_lines_one_a_line) {
  const std::vector<std::string> lines = {"# a", "b"};
  CHECK(gc::DebugShot::text_of(lines) == "# a\nb\n");
}
