cmake_minimum_required(VERSION 3.21)
project(Lab2PngToPnm C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS} -O3 -D ZLIB" )
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

add_subdirectory("zlib")
# add_subdirectory("libdeflate")

add_executable(Lab2PngToPnm main.c)
target_link_libraries(Lab2PngToPnm zlib)
# target_link_libraries(Lab2PngToPnm libdeflate)