# Stand-alone tests for the calculation core (no OpenCPN needed).
#   cmake -S tests -B build-tests && cmake --build build-tests
#   build-tests/test_core
cmake_minimum_required(VERSION 3.12)
project(shannondepth_tests CXX)
set(CMAKE_CXX_STANDARD 17)
add_executable(test_core test_core.cpp ../src/core/depth_core.cpp)
if (MSVC)
  target_compile_options(test_core PRIVATE /W4 /utf-8)
else ()
  target_compile_options(test_core PRIVATE -Wall -Wextra)
endif ()
enable_testing()
add_test(NAME core COMMAND test_core)
