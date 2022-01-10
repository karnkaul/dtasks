cmake_minimum_required(VERSION 3.14)
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "")
set(CMAKE_DEBUG_POSTFIX "-d")

project(dtasks)

# ktl
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(
  ktl
  GIT_REPOSITORY https://github.com/karnkaul/ktl
  GIT_TAG a6c5f32
)
FetchContent_MakeAvailable(ktl)
FetchContent_GetProperties(ktl)

set(${PROJECT_NAME}_version 1.0.0)
set(${PROJECT_NAME}_soversion 1)
add_library(${PROJECT_NAME})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
set_property(TARGET ${PROJECT_NAME} PROPERTY VERSION ${${PROJECT_NAME}_version})
set_property(TARGET ${PROJECT_NAME} PROPERTY SOVERSION ${${PROJECT_NAME}_soversion})
set_property(TARGET ${PROJECT_NAME} PROPERTY INTERFACE_${PROJECT_NAME}_MAJOR_VERSION ${${PROJECT_NAME}_soversion})
set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY COMPATIBLE_INTERFACE_STRING ${PROJECT_NAME}_MAJOR_VERSION)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
target_include_directories(${PROJECT_NAME} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(${PROJECT_NAME} PUBLIC ktl::ktl)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  target_compile_options(${PROJECT_NAME} PRIVATE /MP)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(${PROJECT_NAME} PRIVATE -Wextra -Wall -Werror=return-type $<$<NOT:$<CONFIG:Debug>>:-Werror>)
  endif()
if(UNIX)
  target_link_libraries(${PROJECT_NAME} PRIVATE pthread)
endif()

target_sources(${PROJECT_NAME} PRIVATE
  include/dumb_tasks/detail/id.hpp
  include/dumb_tasks/scheduler.hpp
  include/dumb_tasks/task_queue.hpp
)
add_subdirectory(src)
get_target_property(sources ${PROJECT_NAME} SOURCES)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${sources})

# version
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}_version.hpp.in")
  message(STATUS "Configuring ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}_version.hpp")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}_version.hpp.in" "${CMAKE_CURRENT_BINARY_DIR}/include/${PROJECT_NAME}/${PROJECT_NAME}_version.hpp")
  source_group(TREE "${CMAKE_CURRENT_BINARY_DIR}" FILES "${CMAKE_CURRENT_BINARY_DIR}/include/${PROJECT_NAME}/${PROJECT_NAME}_version.hpp")
endif()

# install and export
install_and_export_target(TARGET ${PROJECT_NAME})
