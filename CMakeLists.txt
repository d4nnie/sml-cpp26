cmake_minimum_required(VERSION 3.28)
project(sml_cpp26 CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Reflection is still gated behind a flag in GCC 16.
add_compile_options(-freflection -Wall -Wextra -Wno-unused-parameter)

find_package(sml REQUIRED)

add_library(reflect_sml INTERFACE)
target_include_directories(reflect_sml INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(reflect_sml INTERFACE cxx_std_26)

# --- examples ---------------------------------------------------------------

add_executable(side_by_side     examples/01_side_by_side.cpp)
target_link_libraries(side_by_side PRIVATE sml::sml reflect_sml)

# A demo that compiles ONLY when BUG_FIXED is defined. Excluded from ALL so
# `cmake --build` doesn't break by default. Use:
#   cmake --build build/Release --target exhaustiveness_demo
# to see the static_assert fire, or
#   cmake -DBUG_FIXED=1 --build build/Release --target exhaustiveness_demo
# to compile the fixed version.
add_executable(exhaustiveness_demo EXCLUDE_FROM_ALL
    examples/02_exhaustiveness_demo.cpp)
target_link_libraries(exhaustiveness_demo PRIVATE reflect_sml)
target_compile_definitions(exhaustiveness_demo PRIVATE BUG_FIXED=$<BOOL:${BUG_FIXED}>)
