cmake_minimum_required(VERSION 3.3)
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "")

project(dtasks)

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	option(DTASKS_CATCH_RUNTIME_ERRORS ON)
else()
	option(DTASKS_CATCH_RUNTIME_ERRORS OFF)
endif()

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
	find_package(Git QUIET)
		if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
		message(STATUS "Updating git submodules...")
		execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
			RESULT_VARIABLE UPDATE_SUBMODULES_RESULT
		)
		if(NOT UPDATE_SUBMODULES_RESULT EQUAL "0")
			message(FATAL_ERROR "git submodule update failed!")
		endif()
	endif()
endif()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.?pp")
file(GLOB_RECURSE INCLUDES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.?pp")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${INCLUDES} ${SOURCES})

add_library(dtasks ${INCLUDES} ${SOURCES})
add_library(dtasks::dtasks ALIAS dtasks)
target_compile_definitions(dtasks PUBLIC $<$<BOOL:${DTASKS_CATCH_RUNTIME_ERRORS}>:DTASKS_CATCH_RUNTIME_ERRORS>)
target_compile_features(dtasks PUBLIC cxx_std_17)
target_include_directories(dtasks PUBLIC include)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_compile_options(dtasks PRIVATE -fPIC)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	target_compile_options(dtasks PRIVATE /MP)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_compile_options(dtasks PRIVATE -Wextra -Wall -Werror=return-type $<$<NOT:$<CONFIG:Debug>>:-Werror>)
endif()
