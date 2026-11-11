# Stand-alone tests for the calculation core and feed reader (no OpenCPN
# needed).
#   cmake -S tests -B build-tests && cmake --build build-tests
#   build-tests/test_core && build-tests/test_feed .
cmake_minimum_required(VERSION 3.12)
project(shannondepth_tests CXX)
set(CMAKE_CXX_STANDARD 17)
set(JSONCPP ${CMAKE_CURRENT_SOURCE_DIR}/../opencpn-libs/jsoncpp)
add_library(test_jsoncpp STATIC
  ${JSONCPP}/src/lib_json/json_reader.cpp
  ${JSONCPP}/src/lib_json/json_value.cpp
  ${JSONCPP}/src/lib_json/json_writer.cpp)
target_include_directories(test_jsoncpp PUBLIC ${JSONCPP}/include)
add_executable(test_core test_core.cpp ../src/core/depth_core.cpp)
add_executable(test_feed test_feed.cpp ../src/core/feed.cpp ../src/core/depth_core.cpp)
target_link_libraries(test_feed test_jsoncpp)
foreach (t test_core test_feed)
  if (MSVC)
    target_compile_options(${t} PRIVATE /W4 /utf-8)
  else ()
    target_compile_options(${t} PRIVATE -Wall -Wextra)
  endif ()
endforeach ()
enable_testing()
add_test(NAME core COMMAND test_core)
add_test(NAME feed COMMAND test_feed ${CMAKE_CURRENT_SOURCE_DIR}/..)
