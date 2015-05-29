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

#
# Linker Configuration
#

set(arachne_pnr_LINK_LIBS
	"-lm")

add_executable(${PROJECT_NAME}
	${arachne_pnr_SOURCES})

target_link_libraries(${PROJECT_NAME} ${arachne_pnr_LINK_LIBS})

#
# Installation
#

install(TARGETS ${PROJECT_NAME} DESTINATION bin/)
