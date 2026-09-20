cmake_minimum_required(VERSION 3.27)
project(
  libjson
  VERSION 0.0.0
  DESCRIPTION "Small C++ JSON library"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(libjson src/libjson/parser.cpp src/libjson/serializer.cpp)
target_include_directories(libjson PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(json src/main.cpp)
target_link_libraries(json PRIVATE libjson)

include(CTest)
if(BUILD_TESTING)
  add_executable(parser_tests tests/parser_tests.cpp)
  target_link_libraries(parser_tests PRIVATE libjson)
  foreach(
    test_name IN
    ITEMS literals
          integers
          invalid_integers
          strings
          invalid_strings
          utf8
          containers
          invalid_containers
          error_offsets)
    add_test(NAME parser.${test_name} COMMAND parser_tests ${test_name})
  endforeach()

  add_executable(serializer_tests tests/serializer_tests.cpp)
  target_link_libraries(serializer_tests PRIVATE libjson)
  foreach(
    test_name IN
    ITEMS literals
          integers
          strings
          controls
          utf8
          invalid_utf8
          arrays
          objects
          round_trips)
    add_test(NAME serializer.${test_name} COMMAND serializer_tests ${test_name})
  endforeach()
endif()
