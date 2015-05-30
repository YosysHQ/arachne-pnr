#
# Copyright (C) 2015 Joel Holdsworth
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

cmake_minimum_required(VERSION 2.8.6)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")

#
# Project
#

project(arachne-pnr)

set(arachne_pnr_TITLE arachne-pnr)
set(arachne_pnr_DESCRIPTION "An FPGA Place-and-Route compilation step.")

set(arachne_pnr_VERSION_MAJOR 0)
set(arachne_pnr_VERSION_MINOR 1)
set(arachne_pnr_VERSION_MICRO 0)

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

find_program(GNU_ENV env)

require_program(bash)
require_program(icebox_vlog)
require_program(icepack)
require_program(python)
require_program(yosys)
require_program(valgrind)

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

add_definitions(-std=c++11)
add_definitions(-MD)
add_definitions(-Wall -Werror)
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

function(add_arachne_pnr_test_script name subdir command)
	add_test(NAME ${name}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${subdir}
		COMMAND ${GNU_ENV}
		ARACHNE_PNR=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}
		ICEBOX_VLOG=${ICEBOX_VLOG}
		ICEPACK=${ICEPACK}
		PYTHON=${PYTHON}
		VALGRIND=${VALGRIND}
		YOSYS=${YOSYS}
		${BASH}
		${command})
endfunction(add_arachne_pnr_test_script)

add_executable(test_bv
	tests/test_bv.cc)

enable_testing()

add_test(test_bv ${CMAKE_CURRENT_BINARY_DIR}/test_bv)
add_arachne_pnr_test_script(simple tests/simple "run-test.sh")
add_arachne_pnr_test_script(regression tests/regression "run-test.sh")
add_arachne_pnr_test_script(simple-valgrind
	tests/simple "run-valgrind-test.sh")
add_arachne_pnr_test_script(fsm tests/fsm "run-test.sh")

add_custom_target(simpletest ${CMAKE_CTEST_COMMAND} -R '^\(test_bv|simple\)$$'
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME} ${CMAKE_CURRENT_BINARY_DIR}/test_bv
	${CMAKE_CURRENT_SOURCE_DIR}/tests/simple/run-test.sh)

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
