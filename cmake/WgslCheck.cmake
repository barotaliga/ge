# The script half of `shaders_check` (cmake/Wgsl.cmake), run as `cmake -P` per resolved shader:
# compile every entry point of WGSL with Tint to SPIR-V, and touch STAMP when all compile.
# Inputs: TINT, WGSL, STAMP.
cmake_minimum_required(VERSION 3.25)
file(READ "${WGSL}" text)
string(REGEX MATCHALL "@(vertex|fragment|compute)[^f]*fn[ \t\n]+[A-Za-z_][A-Za-z0-9_]*" found "${text}")
if(NOT found)
  message(FATAL_ERROR "wgsl: ${WGSL} has no @vertex, @fragment or @compute entry point.")
endif()
foreach(entry IN LISTS found)
  string(REGEX REPLACE ".*fn[ \t\n]+" "" ep "${entry}")
  execute_process(COMMAND "${TINT}" --format spirv --ep "${ep}" -o "${STAMP}.${ep}.spv" "${WGSL}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE output)
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "wgsl: ${WGSL} (entry point ${ep}) does not compile:\n${output}\n"
      "The line numbers are the resolved file's (#include lines expanded); skill: gpu.")
  endif()
endforeach()
file(TOUCH "${STAMP}")
