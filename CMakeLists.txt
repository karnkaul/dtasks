cmake_minimum_required(VERSION 3.3)
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "")

project(dtasks)

# ktl
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(
	ktl
	GIT_REPOSITORY https://github.com/karnkaul/ktl
	GIT_TAG 4247b26
	GIT_SHALLOW ON
)
FetchContent_MakeAvailable(ktl)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.[hc]pp")
file(GLOB_RECURSE INCLUDES CONFIGURE_DEPENDS "include/*.hpp")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${INCLUDES} ${SOURCES})

add_library(dtasks ${INCLUDES} ${SOURCES})
add_library(dtasks::dtasks ALIAS dtasks)
target_compile_features(dtasks PUBLIC cxx_std_17)
target_include_directories(dtasks PUBLIC include)
target_link_libraries(dtasks PUBLIC ktl::ktl)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	target_compile_options(dtasks PRIVATE /MP)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_compile_options(dtasks PRIVATE -Wextra -Wall -Werror=return-type $<$<NOT:$<CONFIG:Debug>>:-Werror>)
endif()
