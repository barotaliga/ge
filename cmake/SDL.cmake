# THE PLATFORM LAYER's dependency: SDL3, fetched as its release archive pinned by tag and
# SHA-256 and linked statically (the platform skill). engine/platform is the only code that
# includes it.
#
# GE_PLATFORM decides whether engine/platform (the window, input, the frame's clock) and the
# apps that open a window are built. It is ON for the top-level build and OFF for a consumer
# that pulls the engine in only for its simulation; with it OFF nothing else changes.
#
# Defines, when GE_PLATFORM is ON: the target SDL3::SDL3-static.
include_guard(GLOBAL)

option(GE_PLATFORM "Build engine/platform (SDL3: the window, input, the frame clock)"
  ${PROJECT_IS_TOP_LEVEL})
if(NOT GE_PLATFORM)
  return()
endif()

# THE PIN. Moving it: change the version and the hash together and rerun the platform tests.
set(GE_SDL_VERSION "3.4.8")
set(GE_SDL_SHA256 "e9fff7467fb60f037e6708da18b25560649e4c63edc2a69bb871b960d9cbfbba")

include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
  URL "https://github.com/libsdl-org/SDL/releases/download/release-${GE_SDL_VERSION}/SDL3-${GE_SDL_VERSION}.tar.gz"
  URL_HASH SHA256=${GE_SDL_SHA256}
  DOWNLOAD_DIR "${GE_DEPS_DIR}"
  EXCLUDE_FROM_ALL SYSTEM)
FetchContent_MakeAvailable(SDL3)
