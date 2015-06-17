#
# Copyright (C) 2015 Joel Holdsworth, Cotton Seed
#
# This file is part of arachne-pnr.  Arachne-pnr is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License version 2 as published by the Free Software
# Foundation.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#

# FIXME test or disallow in-source builds
# FIXME test without yosys

cmake_minimum_required(VERSION 2.8.6)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")

# FIXME not portable
set(CMAKE_C_FLAGS_RELWITHDEBUG "-O3")
set(CMAKE_CXX_FLAGS_RELWITHDEBUG "-O3")

# default build RelWithDebug
if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected, default to RelWithDebug")
    set(CMAKE_BUILD_TYPE "RelWithDebug")
endif()

#
# Project
#

project(arachne-pnr)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

set(arachne_pnr_TITLE arachne-pnr)
set(arachne_pnr_DESCRIPTION "An FPGA Place-and-Route compilation step.")

set(arachne_pnr_VERSION_MAJOR 0)
set(arachne_pnr_VERSION_MINOR 1)
set(arachne_pnr_VERSION_MICRO 0)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

#
# Dependencies
#

function(require_program name)
	string(TOUPPER ${name} var)
	message("-- Finding program ${name}")
	find_program(${var} ${name})
	if(${${var}} MATCHES NOTFOUND)
		message(FATAL_ERROR "-- Failed to find ${name}")
	endif()	
	message("-- Finding program ${name}... ${${var}}")
endfunction()

function(find_program_verbose var name)
	message("-- Finding program ${name}")
	find_program(${var} ${name})
	message("-- Finding program ${name}... ${${var}}")
endfunction()

find_program_verbose(GNU_ENV env)

require_program(bash)
require_program(icebox_vlog)
require_program(icepack)
require_program(python)

find_program_verbose(YOSYS yosys)

find_program_verbose(VALGRIND valgrind)

#
# Sources
#

set(arachne_pnr_SOURCES
	src/arachne-pnr.cc
	src/blif.cc
	src/chipdb.cc
	src/configuration.cc
	src/constant.cc
	src/global.cc
	src/io.cc
	src/line_parser.cc
	src/location.cc
	src/netlist.cc
	src/pack.cc
	src/pcf.cc
	src/place.cc
	src/route.cc
	src/util.cc
)

#
# Definitions
#

# FIXME not portable, see https://stackoverflow.com/questions/10984442/how-to-detect-c11-support-of-a-compiler-with-cmake
add_definitions(-std=c++11)

add_definitions(-MD)
add_definitions(-Wall -Werror)

# FIXME include dir instead of definition?
add_definitions(-I${CMAKE_CURRENT_SOURCE_DIR}/src)

#
# Linker Configuration
#

set(arachne_pnr_LINK_LIBS
	"-lm")

add_executable(${PROJECT_NAME}
	${arachne_pnr_SOURCES})

target_link_libraries(${PROJECT_NAME} ${arachne_pnr_LINK_LIBS})

#
# Tests
#

add_custom_target(simpletest
  ctest -L fast "$(ARGS)")

function(add_arachne_pnr_test_script name subdir command)
	file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${subdir})
	add_test(NAME ${name}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${subdir}
		COMMAND ${GNU_ENV}
		CURRENT_SOURCE_DIR=${CMAKE_SOURCE_DIR}/${subdir}
		ARACHNE_PNR=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROJECT_NAME}
		ICEBOX_VLOG=${ICEBOX_VLOG}
		ICEPACK=${ICEPACK}
		PYTHON=${PYTHON}
		VALGRIND=${VALGRIND}
		YOSYS=${YOSYS}
		${BASH} 
		${CMAKE_SOURCE_DIR}/${subdir}/${command})
endfunction(add_arachne_pnr_test_script)

add_executable(test_bv
	tests/test_bv.cc)

enable_testing()

add_test(test_bv ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_bv)
set_tests_properties(test_bv PROPERTIES LABELS fast)

add_arachne_pnr_test_script(simple tests/simple "run-test.sh")
set_tests_properties(simple PROPERTIES LABELS fast)

add_arachne_pnr_test_script(regression tests/regression "run-test.sh")
set_tests_properties(regression PROPERTIES LABELS fast)

if(${YOSYS} MATCHES NOTFOUND)
	message(WARNING "-- yosys not found. Skipping yosys-based tests")
else()
        add_arachne_pnr_test_script(fsm tests/fsm "run-test.sh")
	add_arachne_pnr_test_script(combinatorial tests/combinatorial "run-test.sh")
endif()

if(${VALGRIND} MATCHES NOTFOUND)
	message(WARNING "-- valgrind not found. Skipping valgrind-based tests")
else()
	add_arachne_pnr_test_script(simple-valgrind
		tests/simple "run-valgrind-test.sh")
endif()

#
# Installation
#

install(TARGETS ${PROJECT_NAME} DESTINATION bin/)

#
# Packaging (handled by CPack)
#

set(CPACK_PACKAGE_VERSION_MAJOR ${arachne_pnr_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${arachne_pnr_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${arachne_pnr_VERSION_MICRO})
set(CPACK_PACKAGE_DESCRIPTION_FILE ${CMAKE_CURRENT_SOURCE_DIR}/README.md)
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/COPYING)
set(CPACK_SOURCE_IGNORE_FILES ${CMAKE_CURRENT_BINARY_DIR} ".gitignore" ".git")
set(CPACK_SOURCE_PACKAGE_FILE_NAME
	"${CMAKE_PROJECT_NAME}-${arachne_pnr_VERSION_MAJOR}.${arachne_pnr_VERSION_MINOR}.${arachne_pnr_VERSION_MICRO}")
set(CPACK_SOURCE_GENERATOR "TGZ")

include(CPack)
