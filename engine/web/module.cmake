# The web (engine/web, docs/hub.md § Watching in a browser): what a game's viewer in the browser
# shares. The viewer's arguments (the hub page's `Module.arguments`), read on every platform and
# tested natively; in an Emscripten build (GE_WEB) the browser's side too: files fetched from the
# hub, the frame the page paints, the canvas's size and the page's status line. A game's
# `<id>_viewer` is made by `ge_add_web_viewer` (cmake/Web.cmake).
set(GE_WEB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/engine/web/viewer_args.cpp
                    ${CMAKE_CURRENT_SOURCE_DIR}/engine/web/file_list.cpp)
if(GE_WEB)
  list(APPEND GE_WEB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/engine/web/browser.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/engine/web/fetch_files.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/engine/web/page_audio.cpp)
endif()
add_library(ge_web STATIC ${GE_WEB_SOURCES})
target_link_libraries(ge_web PUBLIC ge_core ge_input ge_net)
if(GE_WEB)
  # the files fetched by hash, checked, packed; the page's sound: the synth heard (page_audio)
  target_link_libraries(ge_web PUBLIC ge_vfs ge_mod ge_audio)
endif()

ge_module(ge_web EXPORT web)
include(cmake/Web.cmake)
