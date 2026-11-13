# A GAME'S WEB VIEWER (docs/hub.md § Watching in a browser): the engine compiled to WebAssembly,
# drawing through the browser's WebGPU onto the hub page's canvas. Made only in a web build (the
# `web` preset: Emscripten's toolchain sets GE_WEB); elsewhere the call does nothing.
#
#   ge_add_web_viewer(<game> SOURCES <file>... LIBS <target>...)
#
# makes `<game>_viewer.js` and `<game>_viewer.wasm` in `<build>/<game>_viewer/`, each with a gzip
# copy beside it (`.gz`, cmake/WebGzip.cmake), the folder a hub's `--viewer=<dir>` serves. The C++ waits (a fetch, a paint, the GPU's) suspend through JSPI,
# so the viewer is a plain loop in `main`; memory grows as the packs need.
include_guard(GLOBAL)

function(ge_add_web_viewer game)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;LIBS")
  if(NOT GE_WEB)
    return()
  endif()
  set(target ${game}_viewer)
  add_executable(${target} ${arg_SOURCES})
  target_link_libraries(${target} PRIVATE ge_web ${arg_LIBS})
  target_link_options(${target} PRIVATE -sJSPI -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=64MB
    -sMAXIMUM_MEMORY=2GB -sSTACK_SIZE=1MB -sENVIRONMENT=web -sEXIT_RUNTIME=0
    -sEXPORTED_FUNCTIONS=_main,_malloc,_free)
  set_target_properties(${target} PROPERTIES OUTPUT_NAME ${target} SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${target})
  # A gzip copy of the script and the wasm beside them, which a hub sends a browser that takes it.
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -DNAME=${target}.js -P ${PROJECT_SOURCE_DIR}/cmake/WebGzip.cmake
    COMMAND ${CMAKE_COMMAND} -DNAME=${target}.wasm -P ${PROJECT_SOURCE_DIR}/cmake/WebGzip.cmake
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/${target} VERBATIM)
endfunction()
