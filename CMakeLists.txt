cmake_minimum_required(VERSION 3.14)
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "")
set(CMAKE_DEBUG_POSTFIX "-d")

set(project_name dtasks)
set(${project_name}_version 2.0.0)
set(cxx_standard cxx_std_20)

project(${project_name} VERSION ${${project_name}_version})
set(is_root_project OFF) # indicate if this is the top-level project

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set(is_root_project ON)
  message(STATUS "${PROJECT_NAME} is top level")
endif()

# threads
find_package(Threads REQUIRED)

# options
option(DTASKS_INSTALL "Install dtasks and dependencies" ${is_root_project})

# ktl
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(
  ktl
  GIT_REPOSITORY https://github.com/karnkaul/ktl
  GIT_TAG 96a2188
)

if(DTASKS_INSTALL)
  if(NOT "${KTL_INSTALL}" STREQUAL "" AND NOT "${KTL_INSTALL}")
    message(WARNING "DTASKS_INSTALL set but KTL_INSTALL is not, install will fail")
  endif()

  set(KTL_INSTALL ON)
endif()

FetchContent_MakeAvailable(ktl)
FetchContent_GetProperties(ktl)

set(${project_name}_soversion ${PROJECT_VERSION_MAJOR})
add_library(${project_name})
add_library(${project_name}::${project_name} ALIAS ${project_name})
set_property(TARGET ${project_name} PROPERTY VERSION ${${project_name}_version})
set_property(TARGET ${project_name} PROPERTY SOVERSION ${${project_name}_soversion})
set_property(TARGET ${project_name} PROPERTY INTERFACE_${project_name}_MAJOR_VERSION ${${project_name}_soversion})
set_property(TARGET ${project_name} APPEND PROPERTY COMPATIBLE_INTERFACE_STRING ${project_name}_MAJOR_VERSION)

target_compile_features(${project_name} PUBLIC cxx_std_20)
target_include_directories(${project_name} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(${project_name} PUBLIC ktl::ktl)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  target_compile_options(${project_name} PRIVATE /MP)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(${project_name} PRIVATE -Wextra -Wall -Werror=return-type $<$<NOT:$<CONFIG:Debug>>:-Werror>)
endif()

target_link_libraries(${project_name} PRIVATE Threads::Threads)

target_sources(${project_name} PRIVATE
  include/dumb_tasks/executor.hpp
  src/executor.cpp
)
get_target_property(sources ${project_name} SOURCES)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${sources})

# version
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/dtasks_version.hpp.in")
  set(version_dst "${CMAKE_CURRENT_BINARY_DIR}/include/dumb_tasks/dtasks_version.hpp")
  message(STATUS "Configuring ${version_dst}")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/dtasks_version.hpp.in" "${version_dst}")
  source_group(TREE "${CMAKE_CURRENT_BINARY_DIR}" FILES "${version_dst}")
endif()

# install and export
if(DTASKS_INSTALL)
  install_and_export_target(TARGET ${PROJECT_NAME})
endif()
