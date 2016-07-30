##
## Copyright (C) 2016 Joel Holdsworth <joel@airwebreathe.org.uk>
##
## This file is part of arachne-pnr.  Arachne-pnr is free software;
## you can redistribute it and/or modify it under the terms of the GNU
## General Public License version 2 as published by the Free Software
## Foundation.
##
## This program is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program. If not, see <http://www.gnu.org/licenses/>.
##

# We require at least autoconf 2.63 (AC_INIT format changed there).
AC_PREREQ([2.63])

AC_INIT([arachne-pnr], [0.1.0], [cotton@alum.mit.edu], [arachne-pnr],
	[https://github.com/cseed/arachne-pnr])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([autostuff])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
	tests/blif/run-test.sh
	tests/combinatorial/run-test.sh
	tests/error/run-test.sh
	tests/fsm/run-test.sh
	tests/regression/run-test.sh
	tests/simple/run-test.sh
	tests/simple/run-valgrind-test.sh])

AM_INIT_AUTOMAKE([subdir-objects foreign -Wall -Werror])
AM_SILENT_RULES([yes])

AC_CONFIG_FILES([Makefile])

AC_CANONICAL_HOST

# Setup version defines
APNR_PKG_VERSION_SET([PNR_PACKAGE_VERSION], [AC_PACKAGE_VERSION])

# Arguments
AC_ARG_ENABLE([developer-mode], AC_HELP_STRING([--enable-developer-mode],
	[Enables developer mode which enables extra dependencies.]))
AM_CONDITIONAL([ENABLE_DEVELOPER_MODE],
	[test "x$enable_developer_mode" == "xyes"])
AC_ARG_ENABLE([tests], AC_HELP_STRING([--disable-tests],
	[Disables tests which removes dependencies on yosys and icepack]))
AM_CONDITIONAL([ENABLE_TESTS],
	[test "x$enable_tests" != "xno" -o "x$enable_developer_mode" == "xyes"])

# Checks for programs
AC_PROG_CXX
AC_PROG_GREP
AC_PROG_MKDIR_P

AM_COND_IF(ENABLE_TESTS, [

APNR_REQUIRE_PROG([icebox_vlog],
	[icebox_vlog Lattice iCE40 bitstream to verilog converter])
APNR_REQUIRE_PROG([icepack], [icepack Lattice iCE40 bitstream packer])
APNR_REQUIRE_PROG([python], [python interpretter])
APNR_REQUIRE_PROG([shasum], [shasum SHA checksum tool])
APNR_REQUIRE_PROG([yosys], [yosys RTL synthesis framework])

])

AM_COND_IF(ENABLE_DEVELOPER_MODE, [

APNR_REQUIRE_PROG([valgrind], [valgrind instrumentation framework])

])

# Check for icebox
APNR_FIND_ICEBOX

# Check language features
AX_CXX_COMPILE_STDCXX_11

AC_OUTPUT
