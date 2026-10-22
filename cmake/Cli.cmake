# THE ge COMMAND LINE's build (engine/cli; skill: agent-interface). Included by the root
# CMakeLists.txt in every build, top-level or pulled in by a game with FetchContent, because a
# game repo builds the `ge` of the engine it pins and runs it (`make agents-check` there is
# `ge doctor`). Targets:
#
#   ge_cli   the commands as a library (the tests link it)
#   ge       the executable, written to the top-level build directory (build/<preset>/ge in
#            this tree and in a game's) and installed to bin/ with the package
#
# The consumer skills (consumer-skills/<name>/SKILL.md), the game scaffold (scaffold/**) and the
# VERSION file are compiled into ge (cmake/EmbedFiles.cmake), so a ge carries the skills and the
# scaffold of exactly the engine it was built from and reads nothing from disk to write them.
include_guard(GLOBAL)
include(GNUInstallDirs)

option(GE_CLI "Build the ge command line (engine/cli)" ON)

function(ge_add_cli)
  if(NOT GE_CLI)
    return()
  endif()
  set(root "${PROJECT_SOURCE_DIR}")
  file(GLOB_RECURSE embedded CONFIGURE_DEPENDS LIST_DIRECTORIES false
    "${root}/consumer-skills/*/SKILL.md" "${root}/scaffold/*")
  set(script "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/EmbedFiles.cmake")
  set(gen "${PROJECT_BINARY_DIR}/gen/cli/embedded_files.cpp")
  add_custom_command(OUTPUT "${gen}"
    COMMAND ${CMAKE_COMMAND} -DROOT=${root} -DOUT=${gen} -P "${script}"
    DEPENDS ${embedded} "${root}/VERSION" "${script}"
    COMMENT "ge: embedding the consumer skills and the scaffold"
    VERBATIM)

  file(GLOB sources CONFIGURE_DEPENDS "${root}/engine/cli/*.cpp")
  list(FILTER sources EXCLUDE REGEX "/main\\.cpp$")
  add_library(ge_cli STATIC ${sources} "${gen}")
  target_include_directories(ge_cli PUBLIC "$<BUILD_INTERFACE:${root}/engine>")
  target_link_libraries(ge_cli PUBLIC ge_core)

  add_executable(ge "${root}/engine/cli/main.cpp")
  target_link_libraries(ge PRIVATE ge_cli)
  set_target_properties(ge PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
  install(TARGETS ge RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endfunction()
