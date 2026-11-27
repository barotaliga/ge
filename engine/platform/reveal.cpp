// A file shown in the system's file manager, the steps run as processes (reveal.hpp).
//
// ge:summary The reveal steps run by SDL's processes, and the console's `reveal`
// ge:keywords reveal sdl-process add_reveal_command
// ge:skill platform
#include "platform/reveal.hpp"

#include <SDL3/SDL.h>

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "capture/reveal.hpp"

namespace ge::platform {

namespace {

// Starts one step; for a judged one waits for its answer (dbus-send gives up after 2 s).
bool run_step(const capture::RevealStep& step, std::string* why) {
  std::vector<const char*> args;
  args.reserve(step.argv.size() + 1);
  for (const std::string& a : step.argv) args.push_back(a.c_str());
  args.push_back(nullptr);
  const SDL_PropertiesID props = SDL_CreateProperties();
  SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER,
                         reinterpret_cast<void*>(const_cast<char**>(args.data())));
  if (!step.line.empty())
    SDL_SetStringProperty(props, SDL_PROP_PROCESS_CREATE_CMDLINE_STRING, step.line.c_str());
  if (step.judged) {
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_NULL);
  } else {
    SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, true);
  }
  SDL_Process* process = SDL_CreateProcessWithProperties(props);
  SDL_DestroyProperties(props);
  if (process == nullptr) {
    if (why) *why = step.argv[0] + " did not start (" + SDL_GetError() + ")";
    return false;
  }
  bool ok = true;
  if (step.judged) {
    int code = 0;
    SDL_WaitProcess(process, true, &code);
    ok = code == 0;
    if (!ok && why) *why = step.argv[0] + " answered " + std::to_string(code);
  }
  SDL_DestroyProcess(process);
  return ok;
}

}  // namespace

bool reveal(std::string_view path, bool folder, std::string* why) {
  const std::vector<capture::RevealStep> steps =
      capture::reveal_steps(capture::this_desktop(), path, folder);
  std::string last = "no path was given";
  for (const capture::RevealStep& s : steps)
    if (run_step(s, &last)) return true;
  if (why) *why = last;
  return false;
}

void add_reveal_command(console::Console& console) {
  console.add_command(
      "reveal",
      "reveal <path>: the file shown selected in its folder in the system's file manager (Finder, "
      "Explorer or the desktop's own); a folder's path opens the folder",
      [](console::Console& c, const console::Args& a) {
        if (a.source == console::Source::server) return;  // never a line a server sent
        if (a.size() < 2) {
          c.print("Say which file to show: reveal <path>.");
          return;
        }
        std::error_code ec;
        const std::filesystem::path p = std::filesystem::absolute(a[1], ec);
        if (ec || !std::filesystem::exists(p, ec)) {
          c.print("There is no file at " + a[1] + " to show.");
          return;
        }
        const bool folder = std::filesystem::is_directory(p, ec);
        std::string why;
        if (!reveal(p.string(), folder, &why))
          c.print("The file manager could not be opened on " + p.string() + ": " + why + ".");
      });
}

}  // namespace ge::platform
